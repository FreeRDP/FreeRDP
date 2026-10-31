/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* freerdp-shadow-service: runs freerdp-shadow-cli on the console session, as
 * SYSTEM, so a remote user sees and uses whatever that session shows: the
 * user's desktop, and the secure desktop too (the logon and lock screens, UAC
 * prompts), which only SYSTEM may capture and drive.
 *
 * A service runs in session 0, which has no screen. This one copies its own
 * SYSTEM token, moves the copy to the console session and starts the shadow
 * server with it there (on WinSta0\Default; the server then follows the input
 * desktop). When the console session changes (a logoff, a user switch) or the
 * server ends, it starts it again on the session that is now the console's.
 * It also sends the secure attention sequence (Ctrl+Alt+Del) when the server
 * asks for it (win_sas.h), as only a service may.
 *
 *   freerdp-shadow-service install [--log <dir>] [--allow-sas] -- <freerdp-shadow-cli options>
 *   freerdp-shadow-service uninstall
 *   freerdp-shadow-service run [--log <dir>] -- <options>    what the service manager starts
 *
 * freerdp-shadow-cli.exe is taken from the folder of this program. With
 * --log, this service writes service.log there and each start of the shadow
 * server its own shadow-<session>-<time>.log (WLog's file appender).
 * --allow-sas sets the policy that lets services send Ctrl+Alt+Del
 * (SoftwareSASGeneration = 1); without it Windows ignores the request.
 * Uninstall puts the policy back as it was before. */

#include <windows.h>
#include <wtsapi32.h>
#include <shellapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

#include "win_sas.h"

#define SERVICE_NAME L"FreeRDPShadow"
#define SERVICE_DISPLAY L"FreeRDP shadow server"
#define SERVICE_ABOUT                                                                  \
	L"Shares the console session's screen, logon and lock screens included, over RDP " \
	L"(freerdp-shadow-cli)"
#define SHADOW_CLI L"freerdp-shadow-cli.exe"
#define NO_SESSION 0xFFFFFFFF

static SERVICE_STATUS_HANDLE g_statusHandle = NULL;
static SERVICE_STATUS g_status = { 0 };
static HANDLE g_stopEvent = NULL;
static HANDLE g_sessionEvent = NULL;
static WCHAR g_logDir[MAX_PATH] = { 0 };
static WCHAR* g_options = NULL;

/* ---- the log: service.log in --log <dir>, else the debugger's output */

static void say(const WCHAR* format, ...)
{
	WCHAR text[1024] = { 0 };
	SYSTEMTIME now = { 0 };
	va_list args;

	GetLocalTime(&now);
	int n = _snwprintf_s(text, ARRAYSIZE(text), _TRUNCATE, L"%04u-%02u-%02u %02u:%02u:%02u ",
	                     now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
	if (n < 0)
		n = 0;
	va_start(args, format);
	(void)_vsnwprintf_s(text + n, ARRAYSIZE(text) - n, _TRUNCATE, format, args);
	va_end(args);
	OutputDebugStringW(text);
	OutputDebugStringW(L"\n");

	if (!g_logDir[0])
		return;

	WCHAR path[MAX_PATH] = { 0 };
	(void)_snwprintf_s(path, ARRAYSIZE(path), _TRUNCATE, L"%ls\\service.log", g_logDir);
	FILE* fp = NULL;
	if (_wfopen_s(&fp, path, L"a, ccs=UTF-8") == 0 && fp)
	{
		(void)fwprintf(fp, L"%ls\n", text);
		(void)fclose(fp);
	}
}

/* ---- command lines */

/* appends one argument to a command line, quoted the way
 * CommandLineToArgvW reads it back */
static BOOL append_quoted(WCHAR* line, size_t size, const WCHAR* arg)
{
	size_t len = wcslen(line);
	const BOOL quote = (arg[0] == L'\0') || (wcspbrk(arg, L" \t\"") != NULL);

#define PUT(c)               \
	do                       \
	{                        \
		if (len + 1 >= size) \
			return FALSE;    \
		line[len++] = (c);   \
	} while (0)

	if (len > 0)
		PUT(L' ');
	if (!quote)
	{
		for (const WCHAR* p = arg; *p; p++)
			PUT(*p);
		line[len] = L'\0';
		return TRUE;
	}

	PUT(L'"');
	for (const WCHAR* p = arg;; p++)
	{
		size_t backslashes = 0;
		while (*p == L'\\')
		{
			backslashes++;
			p++;
		}
		if (*p == L'\0')
		{
			/* double the backslashes before the closing quote */
			for (size_t i = 0; i < backslashes * 2; i++)
				PUT(L'\\');
			break;
		}
		if (*p == L'"')
		{
			for (size_t i = 0; i < backslashes * 2 + 1; i++)
				PUT(L'\\');
			PUT(L'"');
		}
		else
		{
			for (size_t i = 0; i < backslashes; i++)
				PUT(L'\\');
			PUT(*p);
		}
	}
	PUT(L'"');
	line[len] = L'\0';
	return TRUE;
#undef PUT
}

/* the folder of this program, with a trailing backslash */
static BOOL program_folder(WCHAR* folder, DWORD size)
{
	DWORD n = GetModuleFileNameW(NULL, folder, size);
	if ((n == 0) || (n >= size))
		return FALSE;
	WCHAR* slash = wcsrchr(folder, L'\\');
	if (!slash)
		return FALSE;
	slash[1] = L'\0';
	return TRUE;
}

/* ---- the shadow server on the console session */

static HANDLE start_shadow(DWORD session)
{
	HANDLE self = NULL;
	HANDLE token = NULL;
	HANDLE process = NULL;
	WCHAR folder[MAX_PATH] = { 0 };
	WCHAR exe[MAX_PATH] = { 0 };
	static WCHAR line[32768];
	STARTUPINFOW si = { 0 };
	PROCESS_INFORMATION pi = { 0 };

	if (!program_folder(folder, ARRAYSIZE(folder)))
		return NULL;
	(void)_snwprintf_s(exe, ARRAYSIZE(exe), _TRUNCATE, L"%ls%ls", folder, SHADOW_CLI);
	line[0] = L'\0';
	if (!append_quoted(line, ARRAYSIZE(line), exe))
		return NULL;
	if (g_options && g_options[0])
	{
		if (wcslen(line) + 1 + wcslen(g_options) + 1 > ARRAYSIZE(line))
			return NULL;
		wcscat_s(line, ARRAYSIZE(line), L" ");
		wcscat_s(line, ARRAYSIZE(line), g_options);
	}

	/* our own SYSTEM token, moved to the console session (SE_TCB_NAME,
	 * which SYSTEM holds, allows the move) */
	if (!OpenProcessToken(GetCurrentProcess(),
	                      TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY |
	                          TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
	                      &self))
	{
		say(L"OpenProcessToken failed: 0x%08lX", GetLastError());
		goto out;
	}
	if (!DuplicateTokenEx(self, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &token))
	{
		say(L"DuplicateTokenEx failed: 0x%08lX", GetLastError());
		goto out;
	}
	if (!SetTokenInformation(token, TokenSessionId, &session, sizeof(session)))
	{
		say(L"SetTokenInformation(TokenSessionId %lu) failed: 0x%08lX", session, GetLastError());
		goto out;
	}

	/* WLog to a file of its own, when there is a log folder */
	if (g_logDir[0])
	{
		WCHAR name[64] = { 0 };
		SYSTEMTIME now = { 0 };
		GetLocalTime(&now);
		(void)_snwprintf_s(name, ARRAYSIZE(name), _TRUNCATE,
		                   L"shadow-%lu-%04u%02u%02u-%02u%02u%02u.log", session, now.wYear,
		                   now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
		(void)SetEnvironmentVariableW(L"WLOG_APPENDER", L"FILE");
		(void)SetEnvironmentVariableW(L"WLOG_FILEAPPENDER_OUTPUT_FILE_PATH", g_logDir);
		(void)SetEnvironmentVariableW(L"WLOG_FILEAPPENDER_OUTPUT_FILE_NAME", name);
	}

	si.cb = sizeof(si);
	si.lpDesktop = (LPWSTR)L"winsta0\\default";
	if (!CreateProcessAsUserW(token, exe, line, NULL, NULL, FALSE,
	                          CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, NULL, folder, &si,
	                          &pi))
	{
		say(L"CreateProcessAsUserW(%ls) failed: 0x%08lX", exe, GetLastError());
		goto out;
	}
	(void)CloseHandle(pi.hThread);
	process = pi.hProcess;
	say(L"started %ls on session %lu, process %lu", SHADOW_CLI, session, pi.dwProcessId);

out:
	if (token)
		(void)CloseHandle(token);
	if (self)
		(void)CloseHandle(self);
	return process;
}

static void stop_shadow(HANDLE* process)
{
	if (!*process)
		return;
	if (WaitForSingleObject(*process, 0) == WAIT_TIMEOUT)
	{
		(void)TerminateProcess(*process, 0);
		(void)WaitForSingleObject(*process, 5000);
	}
	(void)CloseHandle(*process);
	*process = NULL;
}

/* SendSAS lives in sas.dll; FALSE: as a service, not as an application */
static void send_sas(void)
{
	typedef VOID(WINAPI * fnSendSAS)(BOOL AsUser);
	HMODULE sas = LoadLibraryW(L"sas.dll");
	fnSendSAS sendSAS = sas ? (fnSendSAS)(void*)GetProcAddress(sas, "SendSAS") : NULL;

	if (!sendSAS)
	{
		say(L"Ctrl+Alt+Del: no SendSAS in sas.dll");
		if (sas)
			(void)FreeLibrary(sas);
		return;
	}
	sendSAS(FALSE);
	say(L"Ctrl+Alt+Del: secure attention sequence sent (Windows ignores it unless "
	    L"SoftwareSASGeneration allows services: install --allow-sas)");
	(void)FreeLibrary(sas);
}

/* until the stop event: the shadow server on whichever session is the
 * console's */
static void serve(void)
{
	HANDLE process = NULL;
	DWORD session = NO_SESSION;
	DWORD restarts = 0;
	HANDLE sasEvent = CreateEventW(NULL, FALSE, FALSE, WIN_SHADOW_SAS_EVENT);

	if (!sasEvent)
		say(L"CreateEvent(%ls) failed: 0x%08lX; Ctrl+Alt+Del will not work", WIN_SHADOW_SAS_EVENT,
		    GetLastError());

	for (;;)
	{
		const DWORD console = WTSGetActiveConsoleSessionId();

		if (process && (console != session))
		{
			say(L"the console moved from session %lu to %lu", session, console);
			stop_shadow(&process);
		}
		if (!process && (console != NO_SESSION))
		{
			process = start_shadow(console);
			session = console;
			if (process)
				restarts++;
		}

		HANDLE events[4] = { g_stopEvent, g_sessionEvent };
		DWORD count = 2;
		if (sasEvent)
			events[count++] = sasEvent;
		const DWORD processIndex = count;
		if (process)
			events[count++] = process;

		/* every 2 s at least: the console session may change unannounced */
		const DWORD rc = WaitForMultipleObjects(count, events, FALSE, 2000);
		if (rc == WAIT_OBJECT_0)
			break;
		if (sasEvent && (rc == WAIT_OBJECT_0 + 2))
			send_sas();
		if (process && (rc == WAIT_OBJECT_0 + processIndex))
		{
			DWORD code = 0;
			(void)GetExitCodeProcess(process, &code);
			say(L"%ls on session %lu ended (exit code %lu)", SHADOW_CLI, session, code);
			(void)CloseHandle(process);
			process = NULL;
			/* not in a tight loop when it cannot start */
			if (WaitForSingleObject(g_stopEvent, restarts > 3 ? 5000 : 1000) == WAIT_OBJECT_0)
				break;
		}
	}

	stop_shadow(&process);
	if (sasEvent)
		(void)CloseHandle(sasEvent);
}

/* ---- the service */

static void set_state(DWORD state)
{
	g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_status.dwCurrentState = state;
	g_status.dwControlsAccepted =
	    (state == SERVICE_RUNNING)
	        ? (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE)
	        : 0;
	g_status.dwWin32ExitCode = NO_ERROR;
	g_status.dwWaitHint = (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : 10000;
	(void)SetServiceStatus(g_statusHandle, &g_status);
}

static DWORD WINAPI handler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context)
{
	(void)eventData;
	(void)context;
	switch (control)
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			set_state(SERVICE_STOP_PENDING);
			(void)SetEvent(g_stopEvent);
			return NO_ERROR;
		case SERVICE_CONTROL_SESSIONCHANGE:
			/* WTS_CONSOLE_CONNECT 1, WTS_CONSOLE_DISCONNECT 2, WTS_SESSION_LOGON 5,
			 * WTS_SESSION_LOGOFF 6, WTS_SESSION_LOCK 7, WTS_SESSION_UNLOCK 8 */
			say(L"session change %lu", eventType);
			(void)SetEvent(g_sessionEvent);
			return NO_ERROR;
		case SERVICE_CONTROL_INTERROGATE:
			return NO_ERROR;
		default:
			return ERROR_CALL_NOT_IMPLEMENTED;
	}
}

static VOID WINAPI service_main(DWORD argc, LPWSTR* argv)
{
	(void)argc;
	(void)argv;
	g_statusHandle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, handler, NULL);
	if (!g_statusHandle)
		return;
	set_state(SERVICE_START_PENDING);
	say(L"service started; options: %ls", g_options ? g_options : L"");
	set_state(SERVICE_RUNNING);
	serve();
	say(L"service stopped");
	set_state(SERVICE_STOPPED);
}

/* ---- install, uninstall */

#define SAS_POLICY_KEY L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System"
#define SAS_POLICY L"SoftwareSASGeneration"
/* what the policy was before install changed it, kept in the service's own key (removed with
 * the service): its old value, or SAS_WAS_UNSET */
#define SERVICE_KEY L"SYSTEM\\CurrentControlSet\\Services\\" SERVICE_NAME
#define SAS_BEFORE L"SoftwareSASGenerationBefore"
#define SAS_WAS_UNSET 0xFFFFFFFFu

/* Lets services send Ctrl+Alt+Del: SoftwareSASGeneration 1 (services) or 3 (services and Ease of
 * Access applications) allows it. When it is set already, nothing changes and nothing is kept;
 * otherwise the old state is kept for uninstall, unless an earlier install kept it already. */
static BOOL allow_sas(void)
{
	DWORD before = SAS_WAS_UNSET;
	DWORD size = sizeof(before);
	const LSTATUS read = RegGetValueW(HKEY_LOCAL_MACHINE, SAS_POLICY_KEY, SAS_POLICY,
	                                  RRF_RT_REG_DWORD, NULL, &before, &size);
	if (read != ERROR_SUCCESS)
		before = SAS_WAS_UNSET;
	if ((before != SAS_WAS_UNSET) && ((before & 1) != 0))
	{
		wprintf(L"SoftwareSASGeneration = %lu already lets services send Ctrl+Alt+Del\n",
		        (unsigned long)before);
		return TRUE;
	}

	DWORD kept = 0;
	size = sizeof(kept);
	if (RegGetValueW(HKEY_LOCAL_MACHINE, SERVICE_KEY, SAS_BEFORE, RRF_RT_REG_DWORD, NULL, &kept,
	                 &size) != ERROR_SUCCESS)
	{
		const LSTATUS rc = RegSetKeyValueW(HKEY_LOCAL_MACHINE, SERVICE_KEY, SAS_BEFORE, REG_DWORD,
		                                   &before, sizeof(before));
		if (rc != ERROR_SUCCESS)
		{
			fwprintf(stderr, L"%ls: 0x%08lX\n", SAS_BEFORE, (unsigned long)rc);
			return FALSE;
		}
	}

	/* services, and Ease of Access applications too where they were allowed (2) */
	const DWORD services = (before == SAS_WAS_UNSET) ? 1 : (before | 1);
	const LSTATUS rc = RegSetKeyValueW(HKEY_LOCAL_MACHINE, SAS_POLICY_KEY, SAS_POLICY, REG_DWORD,
	                                   &services, sizeof(services));
	if (rc != ERROR_SUCCESS)
	{
		fwprintf(stderr, L"SoftwareSASGeneration: 0x%08lX\n", (unsigned long)rc);
		return FALSE;
	}
	wprintf(
	    L"SoftwareSASGeneration = %lu: services may send Ctrl+Alt+Del (uninstall restores it)\n",
	    (unsigned long)services);
	return TRUE;
}

/* Puts SoftwareSASGeneration back as it was before install changed it */
static void restore_sas(void)
{
	DWORD before = 0;
	DWORD size = sizeof(before);
	if (RegGetValueW(HKEY_LOCAL_MACHINE, SERVICE_KEY, SAS_BEFORE, RRF_RT_REG_DWORD, NULL, &before,
	                 &size) != ERROR_SUCCESS)
		return; /* install did not change it */

	LSTATUS rc = ERROR_SUCCESS;
	if (before == SAS_WAS_UNSET)
	{
		rc = RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, SAS_POLICY_KEY, SAS_POLICY);
		if (rc == ERROR_FILE_NOT_FOUND)
			rc = ERROR_SUCCESS;
	}
	else
		rc = RegSetKeyValueW(HKEY_LOCAL_MACHINE, SAS_POLICY_KEY, SAS_POLICY, REG_DWORD, &before,
		                     sizeof(before));
	if (rc != ERROR_SUCCESS)
		fwprintf(stderr, L"SoftwareSASGeneration: 0x%08lX, not restored\n", (unsigned long)rc);
	else if (before == SAS_WAS_UNSET)
		wprintf(L"SoftwareSASGeneration removed again (it was not set before install)\n");
	else
		wprintf(L"SoftwareSASGeneration = %lu again\n", (unsigned long)before);
}

static int install(BOOL sas)
{
	WCHAR self[MAX_PATH] = { 0 };
	static WCHAR image[32768];
	SC_HANDLE scm = NULL;
	SC_HANDLE service = NULL;
	int rc = 1;

	const DWORD n = GetModuleFileNameW(NULL, self, ARRAYSIZE(self));
	if ((n == 0) || (n >= ARRAYSIZE(self)))
		return 1;
	image[0] = L'\0';
	if (!append_quoted(image, ARRAYSIZE(image), self) ||
	    !append_quoted(image, ARRAYSIZE(image), L"run"))
		return 1;
	if (g_logDir[0] && (!append_quoted(image, ARRAYSIZE(image), L"--log") ||
	                    !append_quoted(image, ARRAYSIZE(image), g_logDir)))
		return 1;
	if (!append_quoted(image, ARRAYSIZE(image), L"--"))
		return 1;
	if (g_options && g_options[0])
	{
		if (wcslen(image) + 1 + wcslen(g_options) + 1 > ARRAYSIZE(image))
			return 1;
		wcscat_s(image, ARRAYSIZE(image), L" ");
		wcscat_s(image, ARRAYSIZE(image), g_options);
	}

	scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE | SC_MANAGER_CONNECT);
	if (!scm)
	{
		fwprintf(stderr, L"OpenSCManager: 0x%08lX (an administrator's prompt is needed)\n",
		         GetLastError());
		return 1;
	}
	service = CreateServiceW(scm, SERVICE_NAME, SERVICE_DISPLAY, SERVICE_ALL_ACCESS,
	                         SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
	                         image, NULL, NULL, NULL, NULL, NULL);
	if (!service && (GetLastError() == ERROR_SERVICE_EXISTS))
	{
		service = OpenServiceW(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
		if (service &&
		    !ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_AUTO_START, SERVICE_NO_CHANGE,
		                          image, NULL, NULL, NULL, NULL, NULL, NULL))
		{
			fwprintf(stderr, L"ChangeServiceConfig: 0x%08lX\n", GetLastError());
			goto out;
		}
	}
	if (!service)
	{
		fwprintf(stderr, L"CreateService: 0x%08lX\n", GetLastError());
		goto out;
	}
	/* after the service exists: its key keeps what the policy was */
	if (sas && !allow_sas())
		goto out;

	SERVICE_DESCRIPTIONW about = { (LPWSTR)SERVICE_ABOUT };
	(void)ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &about);
	/* started again 5 s after a crash */
	SC_ACTION again[3] = { { SC_ACTION_RESTART, 5000 },
		                   { SC_ACTION_RESTART, 5000 },
		                   { SC_ACTION_RESTART, 5000 } };
	SERVICE_FAILURE_ACTIONSW failure = { 86400, NULL, NULL, ARRAYSIZE(again), again };
	(void)ChangeServiceConfig2W(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failure);

	if (!StartServiceW(service, 0, NULL) && (GetLastError() != ERROR_SERVICE_ALREADY_RUNNING))
	{
		fwprintf(stderr, L"StartService: 0x%08lX\n", GetLastError());
		goto out;
	}
	wprintf(L"%ls installed and started: %ls\n", SERVICE_NAME, image);
	rc = 0;

out:
	if (service)
		(void)CloseServiceHandle(service);
	(void)CloseServiceHandle(scm);
	return rc;
}

static int uninstall(void)
{
	SERVICE_STATUS status = { 0 };
	SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
	if (!scm)
	{
		fwprintf(stderr, L"OpenSCManager: 0x%08lX\n", GetLastError());
		return 1;
	}
	SC_HANDLE service =
	    OpenServiceW(scm, SERVICE_NAME, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
	if (!service)
	{
		fwprintf(stderr, L"OpenService: 0x%08lX\n", GetLastError());
		(void)CloseServiceHandle(scm);
		return 1;
	}
	(void)ControlService(service, SERVICE_CONTROL_STOP, &status);
	for (int i = 0;
	     i < 50 && QueryServiceStatus(service, &status) && status.dwCurrentState != SERVICE_STOPPED;
	     i++)
		Sleep(200);
	/* before the service's key goes, with what it kept */
	restore_sas();
	const BOOL deleted = DeleteService(service);
	if (!deleted)
		fwprintf(stderr, L"DeleteService: 0x%08lX\n", GetLastError());
	else
		wprintf(L"%ls stopped and removed\n", SERVICE_NAME);
	(void)CloseServiceHandle(service);
	(void)CloseServiceHandle(scm);
	return deleted ? 0 : 1;
}

/* ---- the command line */

static int usage(void)
{
	fwprintf(stderr, L"usage: freerdp-shadow-service install [--log <dir>] [--allow-sas] -- "
	                 L"<freerdp-shadow-cli options>\n"
	                 L"       freerdp-shadow-service uninstall\n"
	                 L"       freerdp-shadow-service run [--log <dir>] -- <options>\n");
	return 2;
}

int main(void)
{
	int argc = 0;
	BOOL sas = FALSE;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	static WCHAR options[32768];

	if (!argv || argc < 2)
		return usage();

	const WCHAR* command = argv[1];
	int i = 2;
	for (; i < argc; i++)
	{
		if (wcscmp(argv[i], L"--") == 0)
		{
			i++;
			break;
		}
		if ((wcscmp(argv[i], L"--log") == 0) && (i + 1 < argc))
		{
			const DWORD n = GetFullPathNameW(argv[++i], ARRAYSIZE(g_logDir), g_logDir, NULL);
			if ((n == 0) || (n >= ARRAYSIZE(g_logDir)))
				return usage();
			(void)CreateDirectoryW(g_logDir, NULL);
		}
		else if (wcscmp(argv[i], L"--allow-sas") == 0)
			sas = TRUE;
		else
			return usage();
	}
	options[0] = L'\0';
	for (; i < argc; i++)
		if (!append_quoted(options, ARRAYSIZE(options), argv[i]))
			return usage();
	g_options = options;

	if (wcscmp(command, L"install") == 0)
		return install(sas);
	if (wcscmp(command, L"uninstall") == 0)
		return uninstall();
	if (wcscmp(command, L"run") != 0)
		return usage();

	g_stopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	g_sessionEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!g_stopEvent || !g_sessionEvent)
		return 1;
	SERVICE_TABLE_ENTRYW table[] = { { (LPWSTR)SERVICE_NAME, service_main }, { NULL, NULL } };
	if (!StartServiceCtrlDispatcherW(table))
	{
		fwprintf(stderr, L"run is for the service manager (install starts it): 0x%08lX\n",
		         GetLastError());
		return 1;
	}
	return 0;
}
