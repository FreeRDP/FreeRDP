#include <winpr/sysinfo.h>
#include <winpr/path.h>
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

static int testTimeout(void)
{
	DWORD start, end, diff;
	char* argv[] =
	{
		"test",
		"/v:192.0.2.1",
		NULL
	};

	int rc;

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

static void* testThread(void* arg)
{
	char* argv[] =
	{
		"test",
		"/v:192.0.2.1",
		NULL
	};

	int rc;

	rc = runInstance(2, argv, arg);

	if (rc != 1)
		ExitThread(-1);

	ExitThread(0);
	return NULL;
}

static int testAbort(void)
{
	DWORD status;
	DWORD start, end, diff;
	HANDLE thread;
	freerdp* instance = NULL;

	s_sync = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!s_sync)
		return -1;

	start = GetTickCount();
	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)testThread,
				&instance, 0, NULL);
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

static int testSuccess(void)
{
	int rc;
	STARTUPINFO si;
	PROCESS_INFORMATION process;
	char* argv[] =
	{
		"test",
		"/v:127.0.0.1",
		"/cert-ignore",
		"/rfx",
		NULL
	};
	int argc = 4;
	char* path = TESTING_OUTPUT_DIRECTORY;
	char* wpath = TESTING_SRC_DIRECTORY;
	char* exe = GetCombinedPath(path, "server");
	char* wexe = GetCombinedPath(wpath, "server");

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
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
	if (!CreateProcessA(exe, exe, NULL, NULL, FALSE, 0, NULL,
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

	Sleep(1000);
	rc = runInstance(argc, argv, NULL);

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
	/* Test connect to not existing server,
	 * check if timeout is honored. */
	if (testTimeout())
		return -1;

	/* Test connect to not existing server,
	 * check if connection abort is working. */
	if (testAbort())
		return -1;

	/* Test connect to existing server,
	 * check if connection is working. */
	if (testSuccess())
		return -1;

	return 0;
}

