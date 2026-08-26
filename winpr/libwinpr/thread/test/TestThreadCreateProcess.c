
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/environment.h>
#include <winpr/pipe.h>
#include <winpr/library.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
/* whitebox: a WinPR HANDLE for a pipe is a pointer to a heap object that does not survive
 * exec() (the address space is replaced), so a *raw fd* - not the HANDLE value - is what needs
 * to be handed to a re-exec'd child for the inheritance probe below. winpr_Handle_getFd() is
 * WinPR-internal (not part of the public API) but this test lives in the same source tree and
 * is deliberately testing that internal fd-inheritance behavior, so reaching in here is fair
 * game. */
#include "../../handle/handle.h"
#endif

#define TESTENV_A "HELLO=WORLD"
#define TESTENV_T _T(TESTENV_A)

#ifdef _WIN32
WINPR_ATTR_NODISCARD
static unsigned long long handle_to_probe_value(HANDLE h)
{
	return (unsigned long long)(UINT_PTR)h;
}
#else
WINPR_ATTR_NODISCARD
static unsigned long long handle_to_probe_value(HANDLE h)
{
	return (unsigned long long)winpr_Handle_getFd(h);
}
#endif

/* Child-mode entry point for the handle-inheritance tests below (see run_inherit_case()): the
 * parent re-execs this same test binary with "--probe-handle <value>", where <value> identifies
 * the handle/fd under test. This process tries to write to it and reports OPEN/CLOSED on its own
 * stdout - which is always wired via STARTF_USESTDHANDLES regardless of the inheritance logic
 * under test - so the parent can read the result back. */
WINPR_ATTR_NODISCARD
static int probe_handle_and_report(const char* valueStr)
{
	BOOL ok = FALSE;

#ifdef _WIN32
	HANDLE h = (HANDLE)(UINT_PTR)strtoull(valueStr, nullptr, 10);
	DWORD written = 0;
	ok = WriteFile(h, "PING", 4, &written, nullptr) && (written == 4);
#else
	const long fd = strtol(valueStr, nullptr, 0);
	if ((fd >= INT32_MIN) || (fd <= INT32_MAX))
		ok = (write(WINPR_ASSERTING_INT_CAST(int, fd), "PING", 4) == 4);
#endif

	printf(ok ? "OPEN\n" : "CLOSED\n");
	(void)fflush(stdout);
	return 0;
}

typedef enum
{
	MODE_NO_INHERIT,               /* bInheritHandles = FALSE */
	MODE_INHERIT_NO_LIST,          /* bInheritHandles = TRUE, no handle list: broad inherit */
	MODE_HANDLE_LIST_WITH_PROBE,   /* bInheritHandles = TRUE, list = [probe handle] */
	MODE_HANDLE_LIST_WITHOUT_PROBE /* bInheritHandles = TRUE, list = [unrelated handle] */
} InheritTestMode;

/* Spawns a child (this same test binary, re-invoked in probe mode) and checks whether
 * `probeHandle` survived into it, per `mode`, matching it against `expectOpen`. This exercises
 * the CreateProcess() code path end to end - so it validates identical behavior on Linux, macOS
 * (both going through winpr/libwinpr/thread/process.c) and Windows (going through the real Win32
 * CreateProcess, which this is a conformance check of). */
WINPR_ATTR_NODISCARD
static int run_inherit_case(const char* exePath, const char* label, HANDLE probeHandle,
                            InheritTestMode mode, BOOL expectOpen)
{
	SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
	HANDLE outRead = nullptr;
	HANDLE outWrite = nullptr;
	int result = 1;

	if (!CreatePipe(&outRead, &outWrite, &saAttr, 0))
	{
		printf("[%s] CreatePipe(out) failed\n", label);
		return 1;
	}

	char cmdline[1024] = WINPR_C_ARRAY_INIT;
	(void)snprintf(cmdline, sizeof(cmdline), "\"%s\" TestThreadCreateProcess --probe-handle %llu",
	               exePath, handle_to_probe_value(probeHandle));

	const BOOL bInheritHandles = (mode != MODE_NO_INHERIT);
	const BOOL useExtended =
	    (mode == MODE_HANDLE_LIST_WITH_PROBE) || (mode == MODE_HANDLE_LIST_WITHOUT_PROBE);

	PROCESS_INFORMATION pi = WINPR_C_ARRAY_INIT;
	BOOL created = FALSE;
	LPPROC_THREAD_ATTRIBUTE_LIST attrList = nullptr;
	STARTUPINFOEXA siEx = WINPR_C_ARRAY_INIT;
	STARTUPINFOA si = WINPR_C_ARRAY_INIT;

	STARTUPINFOA* siPtr = &si;
	DWORD createFlags = 0;
	HANDLE handles[2] = { outWrite, probeHandle };

	if (useExtended)
	{
		SIZE_T size = 0;
		(void)InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
		attrList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(size);
		if (!attrList || !InitializeProcThreadAttributeList(attrList, 1, 0, &size))
		{
			printf("[%s] InitializeProcThreadAttributeList failed\n", label);
			free(attrList);
			CloseHandle(outRead);
			CloseHandle(outWrite);
			return 1;
		}

		/* real Windows treats the handle list as exclusive even for hStdOutput/hStdError: if
		 * outWrite isn't in it too, the child wouldn't get a usable stdout handle at all, and
		 * this test's own OPEN/CLOSED result-capture mechanism would break. Only the probe
		 * handle's presence is what actually varies between the two list-based cases. */
		const size_t handleCount = (mode == MODE_HANDLE_LIST_WITH_PROBE) ? 2 : 1;
		if (!UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
		                               (void*)handles, handleCount * sizeof(HANDLE), nullptr,
		                               nullptr))
		{
			printf("[%s] UpdateProcThreadAttribute failed\n", label);
			DeleteProcThreadAttributeList(attrList);
			free(attrList);
			CloseHandle(outRead);
			CloseHandle(outWrite);
			return 1;
		}

		siEx.StartupInfo.cb = sizeof(siEx);
		siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
		siEx.StartupInfo.hStdOutput = outWrite;
		siEx.StartupInfo.hStdError = outWrite;
		siEx.lpAttributeList = attrList;

		siPtr = (LPSTARTUPINFOA)&siEx;
		createFlags = EXTENDED_STARTUPINFO_PRESENT;
	}
	else
	{
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = outWrite;
		si.hStdError = outWrite;
	}

	created = CreateProcessA(nullptr, cmdline, nullptr, nullptr, bInheritHandles, createFlags,
	                         nullptr, nullptr, siPtr, &pi);

	if (attrList)
	{
		DeleteProcThreadAttributeList(attrList);
		free(attrList);
	}

	CloseHandle(outWrite);

	if (!created)
	{
		printf("[%s] CreateProcessA failed, error=%" PRIu32 "\n", label, GetLastError());
		CloseHandle(outRead);
		return 1;
	}

	if (WaitForSingleObject(pi.hProcess, 5000) != WAIT_OBJECT_0)
	{
		printf("[%s] child did not exit in time\n", label);
	}
	else
	{
		char buf[64] = WINPR_C_ARRAY_INIT;
		DWORD read_bytes = 0;
		(void)ReadFile(outRead, buf, sizeof(buf) - 1, &read_bytes, nullptr);

		const BOOL open = strstr(buf, "OPEN") != nullptr;
		result = (open != expectOpen);
		printf("[%s] expected %s, got '%s' -> %s\n", label, expectOpen ? "OPEN" : "CLOSED", buf,
		       result ? "FAIL" : "OK");
	}

	CloseHandle(outRead);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return result;
}

/* Covers the bInheritHandles / PROC_THREAD_ATTRIBUTE_HANDLE_LIST matrix documented for
 * CreateProcess() on real Windows, which winpr/libwinpr/thread/process.c replicates on
 * Linux/macOS:
 *  - bInheritHandles=FALSE: nothing inherits, even a marked-inheritable handle is closed.
 *  - bInheritHandles=TRUE, no handle list: every inheritable handle is inherited.
 *  - bInheritHandles=TRUE, handle list present: only the listed handles are inherited, even
 *    other inheritable handles are not. */
WINPR_ATTR_NODISCARD
static int TestHandleInheritance(void)
{
	char exePath[4096] = WINPR_C_ARRAY_INIT;
	if (GetModuleFileNameA(nullptr, exePath, sizeof(exePath)) == 0)
	{
		printf("GetModuleFileNameA failed\n");
		return 1;
	}

	SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
	HANDLE probeRead = nullptr;
	HANDLE probeWrite = nullptr;
	if (!CreatePipe(&probeRead, &probeWrite, &saAttr, 0))
	{
		printf("CreatePipe(probe) failed\n");
		return 1;
	}

	int rc = 0;
	rc |= run_inherit_case(exePath, "bInheritHandles=FALSE", probeWrite, MODE_NO_INHERIT, FALSE);
	rc |= run_inherit_case(exePath, "bInheritHandles=TRUE, no list", probeWrite,
	                       MODE_INHERIT_NO_LIST, TRUE);
	rc |= run_inherit_case(exePath, "handle list CONTAINS probe", probeWrite,
	                       MODE_HANDLE_LIST_WITH_PROBE, TRUE);
	rc |= run_inherit_case(exePath, "handle list does NOT contain probe", probeWrite,
	                       MODE_HANDLE_LIST_WITHOUT_PROBE, FALSE);

	CloseHandle(probeRead);
	CloseHandle(probeWrite);
	return rc;
}

int TestThreadCreateProcess(int argc, char* argv[])
{
	if ((argc >= 3) && (strcmp(argv[1], "--probe-handle") == 0))
		return probe_handle_and_report(argv[2]);

	BOOL status = 0;
	DWORD exitCode = 0;
	LPCTSTR lpApplicationName = nullptr;

#ifdef _WIN32
	TCHAR lpCommandLine[200] = _T("cmd /C set");
#else
	TCHAR lpCommandLine[200] = _T("printenv");
#endif

	// LPTSTR lpCommandLine;
	LPSECURITY_ATTRIBUTES lpProcessAttributes = nullptr;
	LPSECURITY_ATTRIBUTES lpThreadAttributes = nullptr;
	BOOL bInheritHandles = 0;
	DWORD dwCreationFlags = 0;
	LPVOID lpEnvironment = nullptr;
	LPCTSTR lpCurrentDirectory = nullptr;
	STARTUPINFO StartupInfo = WINPR_C_ARRAY_INIT;
	PROCESS_INFORMATION ProcessInformation = WINPR_C_ARRAY_INIT;
	LPTCH lpszEnvironmentBlock = nullptr;
	HANDLE pipe_read = nullptr;
	HANDLE pipe_write = nullptr;
	char buf[1024] = WINPR_C_ARRAY_INIT;
	DWORD read_bytes = 0;
	int ret = 0;
	SECURITY_ATTRIBUTES saAttr;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	lpszEnvironmentBlock = GetEnvironmentStrings();

	lpApplicationName = nullptr;

	lpProcessAttributes = nullptr;
	lpThreadAttributes = nullptr;
	bInheritHandles = FALSE;
	dwCreationFlags = 0;
#ifdef _UNICODE
	dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
#endif
	lpEnvironment = lpszEnvironmentBlock;
	lpCurrentDirectory = nullptr;
	StartupInfo.cb = sizeof(STARTUPINFO);

	status = CreateProcess(lpApplicationName, lpCommandLine, lpProcessAttributes,
	                       lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
	                       lpCurrentDirectory, &StartupInfo, &ProcessInformation);

	if (!status)
	{
		printf("CreateProcess failed. error=%" PRIu32 "\n", GetLastError());
		return 1;
	}

	if (WaitForSingleObject(ProcessInformation.hProcess, 5000) != WAIT_OBJECT_0)
	{
		printf("Failed to wait for first process. error=%" PRIu32 "\n", GetLastError());
		return 1;
	}

	exitCode = 0;
	status = GetExitCodeProcess(ProcessInformation.hProcess, &exitCode);

	printf("GetExitCodeProcess status: %" PRId32 "\n", status);
	printf("Process exited with code: 0x%08" PRIX32 "\n", exitCode);

	(void)CloseHandle(ProcessInformation.hProcess);
	(void)CloseHandle(ProcessInformation.hThread);
	FreeEnvironmentStrings(lpszEnvironmentBlock);

	/* Test stdin,stdout,stderr redirection */

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = nullptr;

	if (!CreatePipe(&pipe_read, &pipe_write, &saAttr, 0))
	{
		printf("Pipe creation failed. error=%" PRIu32 "\n", GetLastError());
		return 1;
	}

	bInheritHandles = TRUE;

	ZeroMemory(&StartupInfo, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	StartupInfo.hStdOutput = pipe_write;
	StartupInfo.hStdError = pipe_write;
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;

	ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));

	if (!(lpEnvironment = calloc(1, sizeof(TESTENV_T) + sizeof(TCHAR))))
	{
		printf("Failed to allocate environment buffer. error=%" PRIu32 "\n", GetLastError());
		return 1;
	}
	memcpy(lpEnvironment, (void*)TESTENV_T, sizeof(TESTENV_T));

	status = CreateProcess(lpApplicationName, lpCommandLine, lpProcessAttributes,
	                       lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
	                       lpCurrentDirectory, &StartupInfo, &ProcessInformation);

	free(lpEnvironment);

	if (!status)
	{
		(void)CloseHandle(pipe_read);
		(void)CloseHandle(pipe_write);
		printf("CreateProcess failed. error=%" PRIu32 "\n", GetLastError());
		return 1;
	}

	if (WaitForSingleObject(ProcessInformation.hProcess, 5000) != WAIT_OBJECT_0)
	{
		printf("Failed to wait for second process. error=%" PRIu32 "\n", GetLastError());
		return 1;
	}

	ZeroMemory(buf, sizeof(buf));
	ReadFile(pipe_read, buf, sizeof(buf) - 1, &read_bytes, nullptr);
	if (!strstr((const char*)buf, TESTENV_A))
	{
		printf("No or unexpected data read from pipe\n");
		ret = 1;
	}

	(void)CloseHandle(pipe_read);
	(void)CloseHandle(pipe_write);

	exitCode = 0;
	status = GetExitCodeProcess(ProcessInformation.hProcess, &exitCode);

	printf("GetExitCodeProcess status: %" PRId32 "\n", status);
	printf("Process exited with code: 0x%08" PRIX32 "\n", exitCode);

	(void)CloseHandle(ProcessInformation.hProcess);
	(void)CloseHandle(ProcessInformation.hThread);

	if (ret == 0)
		ret = TestHandleInheritance();

	return ret;
}
