#include <winpr/sysinfo.h>
#include <winpr/path.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/cmdline.h>

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

    if (freerdp_client_load_addins(instance->context->channels, instance->settings) != 1)
        goto finish;

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

    start = GetTickCount();
    thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)testThread,
                &instance, 0, NULL);
    if (!thread)
        return -1;

    freerdp_abort_connect(instance);
    status = WaitForSingleObject(thread, 20000);
    end = GetTickCount();

    CloseHandle(thread);

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
        "/u:foo",
        NULL
    };
    char* path = TESTING_OUTPUT_DIRECTORY;
    char* exe = GetCombinedPath(path, "server");

    if (!exe)
        return -2;

    path = GetCombinedPath(exe, "Sample");
    free(exe);

    if (!path)
        return -2;

    exe = GetCombinedPath(path, "sfreerdp-server");

    if (!exe)
    {
        free(path);
        return -2;
    }

    // Start sample server locally. 
    if (!CreateProcessA(exe, exe, NULL, NULL, FALSE, 0, NULL,
                path, &si, &process))
    {
        free(exe);
        free(path);
        return -2;
    }
   
    free(exe);
    free(path);

    rc = runInstance(4, argv, NULL);

    if (!TerminateProcess(process.hProcess, 0))
        return -2;

    WaitForSingleObject(process.hProcess, INFINITE);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

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

