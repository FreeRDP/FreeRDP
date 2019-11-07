#include <winpr/sysinfo.h>
#include <winpr/path.h>
#include <winpr/crypto.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/cmdline.h>

static HANDLE s_sync = NULL;

static int runInstance(int argc, char* argv[], freerdp** inst)
{
	int rc = -1;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	ZeroMemory(&clientEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;
	clientEntryPoints.ContextSize = sizeof(rdpContext);
	rdpContext* context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		goto finish;

	if (inst)
		*inst = context->instance;

	if (freerdp_client_settings_parse_command_line(context->settings, argc, argv, FALSE) < 0)
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
	return rc;
}

static int testTimeout(int port)
{
	DWORD start, end, diff;
	char arg1[] = "/v:192.0.2.1:XXXXX";
	char* argv[] = { "test", "/v:192.0.2.1:XXXXX", NULL };
	int rc;
	_snprintf(arg1, 18, "/v:192.0.2.1:%d", port);
	argv[1] = arg1;
	start = GetTickCount();
	rc = runInstance(2, argv, NULL);
	end = GetTickCount();

	if (rc != 1)
		return -1;

	diff = end - start;

	if (diff > 16000)
		return -1;

	if (diff < 14000)
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
	char* argv[] = { "test", "/v:192.0.2.1:XXXXX", NULL };
	int rc;
	struct testThreadArgs* args = arg;
	_snprintf(arg1, 18, "/v:192.0.2.1:%d", args->port);
	argv[1] = arg1;
	rc = runInstance(2, argv, args->arg);

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
	Sleep(1000); /* Wait until freerdp_connect has been called */
	freerdp_abort_connect(instance);
	status = WaitForSingleObject(instance->context->abortEvent, 0);

	if (status != WAIT_OBJECT_0)
	{
		CloseHandle(s_sync);
		CloseHandle(thread);
		s_sync = NULL;
		return -1;
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

static int testSuccess(int port)
{
	int r;
	int rc = -2;
	STARTUPINFOA si;
	PROCESS_INFORMATION process;
	char arg1[] = "/v:127.0.0.1:XXXXX";
	char* clientArgs[] = { "test", "/v:127.0.0.1:XXXXX", "/cert-ignore", "/rfx", NULL };
	char* commandLine = NULL;
	size_t commandLineLen;
	int argc = 4;
	char* path = NULL;
	char* wpath = NULL;
	char* exe = GetCombinedPath(TESTING_OUTPUT_DIRECTORY, "server");
	char* wexe = GetCombinedPath(TESTING_SRC_DIRECTORY, "server");
	_snprintf(arg1, 18, "/v:127.0.0.1:%d", port);
	clientArgs[1] = arg1;

	if (!exe || !wexe)
		goto fail;

	path = GetCombinedPath(exe, "Sample");
	wpath = GetCombinedPath(wexe, "Sample");

	if (!path || !wpath)
		goto fail;

	exe = GetCombinedPath(path, "sfreerdp-server");

	if (!exe)
		goto fail;

	printf("Sample Server: %s\n", exe);
	printf("Workspace: %s\n", wpath);

	if (!PathFileExistsA(exe))
		goto fail;

	// Start sample server locally.
	commandLineLen = strlen(exe) + strlen("--local-only --port=XXXXX") + 1;
	commandLine = malloc(commandLineLen);

	if (!commandLine)
		goto fail;

	_snprintf(commandLine, commandLineLen, "%s --local-only --port=%d", exe, port);
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	if (!CreateProcessA(exe, commandLine, NULL, NULL, FALSE, 0, NULL, wpath, &si, &process))
		goto fail;

	Sleep(1 * 1000); /* let the server start */
	r = runInstance(argc, clientArgs, NULL);

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
