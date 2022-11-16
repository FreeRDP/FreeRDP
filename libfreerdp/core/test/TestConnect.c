#include <winpr/sysinfo.h>
#include <winpr/path.h>
#include <winpr/crypto.h>
#include <winpr/pipe.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/cmdline.h>

static HANDLE s_sync = NULL;

static int runInstance(int argc, char* argv[], freerdp** inst, DWORD timeout)
{
	int rc = -1;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = { 0 };
	rdpContext* context;

	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;
	clientEntryPoints.ContextSize = sizeof(rdpContext);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		goto finish;

	if (inst)
		*inst = context->instance;

	if (!freerdp_settings_set_bool(context->settings, FreeRDP_DeactivateClientDecoding, TRUE))
		return FALSE;

	if (freerdp_client_settings_parse_command_line(context->settings, argc, argv, FALSE) < 0)
		goto finish;

	if (!freerdp_settings_set_uint32(context->settings, FreeRDP_TcpConnectTimeout, timeout))
		goto finish;

	if (!freerdp_client_load_addins(context->channels, context->settings))
		goto finish;

	if (s_sync)
	{
		if (!SetEvent(s_sync))
			goto finish;
	}

	rc = 1;

	if (!freerdp_connect(context->instance))
		goto finish;

	rc = 2;

	if (!freerdp_disconnect(context->instance))
		goto finish;

	rc = 0;
finish:
	freerdp_client_context_free(context);
	if (inst)
		*inst = NULL;
	return rc;
}

static int testTimeout(int port)
{
	const DWORD timeout = 200;
	DWORD start, end, diff;
	char arg1[] = "/v:192.0.2.1:XXXXX";
	char* argv[] = { "test", "/v:192.0.2.1:XXXXX" };
	int rc;
	_snprintf(arg1, 18, "/v:192.0.2.1:%d", port);
	argv[1] = arg1;
	start = GetTickCount();
	rc = runInstance(ARRAYSIZE(argv), argv, NULL, timeout);
	end = GetTickCount();

	if (rc != 1)
		return -1;

	diff = end - start;

	if (diff > 4 * timeout)
		return -1;

	if (diff < timeout)
		return -1;

	printf("%s: Success!\n", __FUNCTION__);
	return 0;
}

struct testThreadArgs
{
	int port;
	freerdp** arg;
};

static DWORD WINAPI testThread(LPVOID arg)
{
	char arg1[] = "/v:192.0.2.1:XXXXX";
	char* argv[] = { "test", "/v:192.0.2.1:XXXXX" };
	int rc;
	struct testThreadArgs* args = arg;
	_snprintf(arg1, 18, "/v:192.0.2.1:%d", args->port);
	argv[1] = arg1;
	rc = runInstance(ARRAYSIZE(argv), argv, args->arg, 5000);

	if (rc != 1)
		ExitThread(-1);

	ExitThread(0);
	return 0;
}

static int testAbort(int port)
{
	DWORD status;
	DWORD start, end, diff;
	HANDLE thread;
	struct testThreadArgs args;
	freerdp* instance = NULL;
	s_sync = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!s_sync)
		return -1;

	args.port = port;
	args.arg = &instance;
	start = GetTickCount();
	thread = CreateThread(NULL, 0, testThread, &args, 0, NULL);

	if (!thread)
	{
		CloseHandle(s_sync);
		s_sync = NULL;
		return -1;
	}

	WaitForSingleObject(s_sync, INFINITE);
	Sleep(100); /* Wait until freerdp_connect has been called */
	if (instance)
	{
		freerdp_abort_connect_context(instance->context);

		if (!freerdp_shall_disconnect_context(instance->context))
		{
			CloseHandle(s_sync);
			CloseHandle(thread);
			s_sync = NULL;
			return -1;
		}
	}

	status = WaitForSingleObject(thread, 20000);
	end = GetTickCount();
	CloseHandle(s_sync);
	CloseHandle(thread);
	s_sync = NULL;
	diff = end - start;

	if (diff > 5000)
	{
		printf("%s required %" PRIu32 "ms for the test\n", __FUNCTION__, diff);
		return -1;
	}

	if (WAIT_OBJECT_0 != status)
		return -1;

	printf("%s: Success!\n", __FUNCTION__);
	return 0;
}

static char* concatenate(size_t count, ...)
{
	size_t x;
	char* rc;
	va_list ap;
	va_start(ap, count);
	rc = _strdup(va_arg(ap, char*));
	for (x = 1; x < count; x++)
	{
		const char* cur = va_arg(ap, const char*);
		char* tmp = GetCombinedPath(rc, cur);
		free(rc);
		rc = tmp;
	}
	va_end(ap);
	return rc;
}

static BOOL prepare_certificates(const char* path)
{
	BOOL rc = FALSE;
	char* exe = NULL;
	DWORD status;
	STARTUPINFOA si = { 0 };
	SECURITY_ATTRIBUTES saAttr = { 0 };
	PROCESS_INFORMATION process = { 0 };
	char commandLine[8192] = { 0 };

	if (!path)
		return FALSE;

	exe = concatenate(5, TESTING_OUTPUT_DIRECTORY, "winpr", "tools", "makecert-cli",
	                  "winpr-makecert");
	if (!exe)
		return FALSE;
	_snprintf(commandLine, sizeof(commandLine), "%s -format crt -path . -n server", exe);
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	rc = CreateProcessA(exe, commandLine, NULL, NULL, TRUE, 0, NULL, path, &si, &process);
	free(exe);
	if (!rc)
		goto fail;
	status = WaitForSingleObject(process.hProcess, 30000);
	if (status != WAIT_OBJECT_0)
		goto fail;
	rc = TRUE;
fail:
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);
	return rc;
}

static int testSuccess(int port)
{
	int r;
	int rc = -2;
	STARTUPINFOA si = { 0 };
	PROCESS_INFORMATION process = { 0 };
	char arg1[] = "/v:127.0.0.1:XXXXX";
	char* clientArgs[] = { "test", "/v:127.0.0.1:XXXXX", "/cert:ignore", "/rfx", NULL };
	char* commandLine = NULL;
	size_t commandLineLen;
	int argc = 4;
	char* path = NULL;
	char* wpath = NULL;
	char* exe = GetCombinedPath(TESTING_OUTPUT_DIRECTORY, "server");
	_snprintf(arg1, 18, "/v:127.0.0.1:%d", port);
	clientArgs[1] = arg1;

	if (!exe)
		goto fail;

	path = GetCombinedPath(exe, "Sample");
	wpath = GetCombinedPath(exe, "Sample");
	free(exe);
	exe = NULL;

	if (!path || !wpath)
		goto fail;

	exe = GetCombinedPath(path, "sfreerdp-server");

	if (!exe)
		goto fail;

	printf("Sample Server: %s\n", exe);
	printf("Workspace: %s\n", wpath);

	if (!winpr_PathFileExists(exe))
		goto fail;

	if (!prepare_certificates(wpath))
		goto fail;

	// Start sample server locally.
	commandLineLen = strlen(exe) + strlen("--port=XXXXX") + 1;
	commandLine = malloc(commandLineLen);

	if (!commandLine)
		goto fail;

	_snprintf(commandLine, commandLineLen, "%s --port=%d", exe, port);
	si.cb = sizeof(si);

	if (!CreateProcessA(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, wpath, &si, &process))
		goto fail;

	Sleep(600); /* let the server start */
	r = runInstance(argc, clientArgs, NULL, 5000);

	if (!TerminateProcess(process.hProcess, 0))
		goto fail;

	WaitForSingleObject(process.hProcess, INFINITE);
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);
	printf("%s: returned %d!\n", __FUNCTION__, r);
	rc = r;

	if (rc == 0)
		printf("%s: Success!\n", __FUNCTION__);

fail:
	free(exe);
	free(path);
	free(wpath);
	free(commandLine);
	return rc;
}

int TestConnect(int argc, char* argv[])
{
	int randomPort;
	int random;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	winpr_RAND((BYTE*)&random, sizeof(random));
	randomPort = 3389 + (random % 200);

	/* Test connect to not existing server,
	 * check if timeout is honored. */
	if (testTimeout(randomPort))
		return -1;

	/* Test connect to not existing server,
	 * check if connection abort is working. */
	if (testAbort(randomPort))
		return -1;

	/* Test connect to existing server,
	 * check if connection is working. */
	if (testSuccess(randomPort))
		return -1;

	return 0;
}
