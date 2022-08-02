
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

static DWORD WINAPI test_thread(LPVOID arg)
{
	WINPR_UNUSED(arg);
	Sleep(100);
	ExitThread(0);
	return 0;
}

int TestSynchThread(int argc, char* argv[])
{
	DWORD rc;
	HANDLE thread;
	int i;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	thread = CreateThread(NULL, 0, test_thread, NULL, 0, NULL);

	if (!thread)
	{
		printf("CreateThread failure\n");
		return -1;
	}

	/* TryJoin should now fail. */
	rc = WaitForSingleObject(thread, 0);

	if (WAIT_TIMEOUT != rc)
	{
		printf("Timed WaitForSingleObject on running thread failed with %" PRIu32 "\n", rc);
		return -3;
	}

	/* Join the thread */
	rc = WaitForSingleObject(thread, INFINITE);

	if (WAIT_OBJECT_0 != rc)
	{
		printf("WaitForSingleObject on thread failed with %" PRIu32 "\n", rc);
		return -2;
	}

	/* TimedJoin should now succeed. */
	rc = WaitForSingleObject(thread, 0);

	if (WAIT_OBJECT_0 != rc)
	{
		printf("Timed WaitForSingleObject on dead thread failed with %" PRIu32 "\n", rc);
		return -5;
	}

	/* check that WaitForSingleObject works multiple times on a terminated thread */
	for (i = 0; i < 4; i++)
	{
		rc = WaitForSingleObject(thread, 0);
		if (WAIT_OBJECT_0 != rc)
		{
			printf("Timed WaitForSingleObject on dead thread failed with %" PRIu32 "\n", rc);
			return -6;
		}
	}

	if (!CloseHandle(thread))
	{
		printf("CloseHandle failed!");
		return -1;
	}

	thread = CreateThread(NULL, 0, test_thread, NULL, 0, NULL);

	if (!thread)
	{
		printf("CreateThread failure\n");
		return -1;
	}

	/* TryJoin should now fail. */
	rc = WaitForSingleObject(thread, 10);

	if (WAIT_TIMEOUT != rc)
	{
		printf("Timed WaitForSingleObject on running thread failed with %" PRIu32 "\n", rc);
		return -3;
	}

	/* Join the thread */
	rc = WaitForSingleObject(thread, INFINITE);

	if (WAIT_OBJECT_0 != rc)
	{
		printf("WaitForSingleObject on thread failed with %" PRIu32 "\n", rc);
		return -2;
	}

	/* TimedJoin should now succeed. */
	rc = WaitForSingleObject(thread, 0);

	if (WAIT_OBJECT_0 != rc)
	{
		printf("Timed WaitForSingleObject on dead thread failed with %" PRIu32 "\n", rc);
		return -5;
	}

	if (!CloseHandle(thread))
	{
		printf("CloseHandle failed!");
		return -1;
	}

	/* Thread detach test */
	thread = CreateThread(NULL, 0, test_thread, NULL, 0, NULL);

	if (!thread)
	{
		printf("CreateThread failure\n");
		return -1;
	}

	if (!CloseHandle(thread))
	{
		printf("CloseHandle failed!");
		return -1;
	}

	return 0;
}
