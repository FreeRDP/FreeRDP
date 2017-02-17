#include <winpr/sysinfo.h>
#include <winpr/path.h>
#include <winpr/crypto.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/cmdline.h>

static HANDLE s_sync = NULL;

static int runInstance(int argc, char* argv[], freerdp** inst)
{
	int rc = -1;
	freerdp* instance = freerdp_new();

	if (!instance)
		goto finish;

	if (inst)
		*inst = instance;

	if (!freerdp_context_new(instance))
		goto finish;

	if (freerdp_client_settings_parse_command_line(instance->settings, argc, argv, FALSE) < 0)
		goto finish;

	if (!freerdp_client_load_addins(instance->context->channels, instance->settings))
		goto finish;

	if (s_sync)
	{
		if (!SetEvent(s_sync))
			goto finish;
	}

	rc = 1;

	if (!freerdp_connect(instance))
		goto finish;

	rc = 2;

	if (!freerdp_disconnect(instance))
		goto finish;

	rc = 0;
finish:
	freerdp_context_free(instance);
	freerdp_free(instance);
	return rc;
}

static int testTimeout(int port)
{
	DWORD start, end, diff;
	char arg1[] = "/v:192.0.2.1:XXXXX";
	char* argv[] =
	{
		"test",
		"/v:192.0.2.1:XXXXX",
		NULL
	};
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

static void* testThread(void* arg)
{
	char arg1[] = "/v:192.0.2.1:XXXXX";
	char* argv[] =
	{
		"test",
		"/v:192.0.2.1:XXXXX",
		NULL
	};
	int rc;
	struct testThreadArgs* args = arg;
	_snprintf(arg1, 18, "/v:192.0.2.1:%d", args->port);
	argv[1] = arg1;
	rc = runInstance(2, argv, args->arg);

	if (rc != 1)
		ExitThread(-1);

	ExitThread(0);
	return NULL;
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
	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)testThread,
	                      &args, 0, NULL);

	if (!thread)
	{
		CloseHandle(s_sync);
		s_sync = NULL;
		return -1;
	}

	WaitForSingleObject(s_sync, INFINITE);
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

	if (diff > 1000)
		return -1;

	if (WAIT_OBJECT_0 != status)
		return -1;

	printf("%s: Success!\n", __FUNCTION__);
	return 0;
}

static int testSuccess(int port)
{
	int rc;
	STARTUPINFOA si;
	PROCESS_INFORMATION process;
	char arg1[] = "/v:127.0.0.1:XXXXX";
	char* clientArgs[] =
	{
		"test",
		"/v:127.0.0.1:XXXXX",
		"/cert-ignore",
		"/rfx",
		NULL
	};
	char* commandLine;
	int commandLineLen;
	int argc = 4;
	char* path = TESTING_OUTPUT_DIRECTORY;
	char* wpath = TESTING_SRC_DIRECTORY;
	char* exe = GetCombinedPath(path, "server");
	char* wexe = GetCombinedPath(wpath, "server");
	_snprintf(arg1, 18, "/v:127.0.0.1:%d", port);
	clientArgs[1] = arg1;

	if (!exe || !wexe)
	{
		free(exe);
		free(wexe);
		return -2;
	}

	path = GetCombinedPath(exe, "Sample");
	wpath = GetCombinedPath(wexe, "Sample");
	free(exe);
	free(wexe);

	if (!path || !wpath)
	{
		free(path);
		free(wpath);
		return -2;
	}

	exe = GetCombinedPath(path, "sfreerdp-server");

	if (!exe)
	{
		free(path);
		free(wpath);
		return -2;
	}

	printf("Sample Server: %s\n", exe);
	printf("Workspace: %s\n", wpath);

	if (!PathFileExistsA(exe))
	{
		free(path);
		free(wpath);
		free(exe);
		return -2;
	}

	// Start sample server locally.
	commandLineLen = strlen(exe) + strlen(" --port=XXXXX") + 1;
	commandLine = malloc(commandLineLen);

	if (!commandLine)
	{
		free(path);
		free(wpath);
		free(exe);
		return -2;
	}

	_snprintf(commandLine, commandLineLen, "%s --port=%d", exe, port);
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	if (!CreateProcessA(exe, commandLine, NULL, NULL, FALSE, 0, NULL,
	                    wpath, &si, &process))
	{
		free(exe);
		free(path);
		free(wpath);
		return -2;
	}

	free(exe);
	free(path);
	free(wpath);
	free(commandLine);
	Sleep(1 * 1000); /* let the server start */
	rc = runInstance(argc, clientArgs, NULL);

	if (!TerminateProcess(process.hProcess, 0))
		return -2;

	WaitForSingleObject(process.hProcess, INFINITE);
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);
	printf("%s: returned %d!\n", __FUNCTION__, rc);

	if (rc)
		return -1;

	printf("%s: Success!\n", __FUNCTION__);
	return 0;
}

int TestConnect(int argc, char* argv[])
{
	int randomPort;
	int random;
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

