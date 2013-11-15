
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

static void *test_thread(void *arg)
{
	Sleep(1000);

	ExitThread(0);
	return NULL;
}

int TestSynchThread(int argc, char* argv[])
{
	DWORD rc;
	HANDLE thread;

	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)test_thread,
			NULL, 0, NULL);
	if (!thread)
	{
		printf("CreateThread failure\n");
		return -1;
	}

	/* TryJoin should now fail. */
	rc = WaitForSingleObject(thread, 0);
	if (WAIT_TIMEOUT != rc)
	{
		printf("Timed WaitForSingleObject on running thread failed with %d\n", rc);
		return -3;
	}

	/* Join the thread */
	rc = WaitForSingleObject(thread, INFINITE);
	if (WAIT_OBJECT_0 != rc)
	{
		printf("WaitForSingleObject on thread failed with %d\n", rc);
		return -2;
	}

	/* TimedJoin should now succeed. */
	rc = WaitForSingleObject(thread, 0);
	if (WAIT_OBJECT_0 != rc)
	{
		printf("Timed WaitForSingleObject on dead thread failed with %d\n", rc);
		return -5;
	}

	CloseHandle(thread);

	return 0;
}

