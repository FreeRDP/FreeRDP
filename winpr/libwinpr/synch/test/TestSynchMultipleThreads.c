
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

static void *test_thread(void *arg)
{
	long timeout = rand();
	timeout %= 1000;
	timeout += 100;
	Sleep(timeout);
	ExitThread(0);
	return NULL;
}

static int start_threads(DWORD count, HANDLE *threads)
{
	DWORD i;

	for (i=0; i<count; i++)
	{
		threads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)test_thread,
								  NULL, 0, NULL);

		if (!threads[i])
		{
			printf("CreateThread [%i] failure\n", i);
			return -1;
		}
	}

	return 0;
}

static int close_threads(DWORD count, HANDLE *threads)
{
	DWORD i;

	for (i=0; i<count; i++)
	{
		if (!CloseHandle(threads[i]))
		{
			printf("CloseHandle [%d] failure\n", i);
			return -1;
		}
	}

	return 0;
}

int TestSynchMultipleThreads(int argc, char *argv[])
{
#define THREADS 24
	DWORD rc = 0, ev, i;
	HANDLE threads[THREADS];

	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
		return -1;

	if (WaitForMultipleObjects(THREADS, threads, TRUE, 50) != WAIT_TIMEOUT)
	{
		printf("WaitForMultipleObjects bWaitAll, timeout 50 failed\n");
		rc = -1;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects bWaitAll, INFINITE failed\n");
		rc = -1;
	}

	if (close_threads(THREADS, threads))
		return -1;

	/* WaitOne, infinite */
	if (rc)
		return rc;

	if (start_threads(THREADS, threads))
		return -1;

	ev = WaitForMultipleObjects(THREADS, threads, FALSE, INFINITE);

	if ((ev < WAIT_OBJECT_0) || (ev > (WAIT_OBJECT_0 + THREADS)))
	{
		printf("WaitForMultipleObjects INFINITE failed\n");
		rc = -1;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects bWaitAll, INFINITE failed\n");
		rc = -1;
	}

	if (close_threads(THREADS, threads))
		return -1;

	if (rc)
		return rc;

	/* WaitOne, timeout */
	if (start_threads(THREADS, threads))
		return -1;

	if (WaitForMultipleObjects(THREADS, threads, FALSE, 50) != WAIT_TIMEOUT)
	{
		printf("WaitForMultipleObjects timeout 50 failed\n");
		rc = -1;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects bWaitAll, INFINITE failed\n");
		rc = -1;
	}

	if (close_threads(THREADS, threads))
		return -1;

	/* WaitOne, timeout, multiple joins */
	if (start_threads(THREADS, threads))
		return -1;

	for (i=0; i<THREADS; i++)
	{
		if (WaitForMultipleObjects(THREADS, threads, FALSE, 0) != WAIT_TIMEOUT)
		{
			printf("WaitForMultipleObjects timeout 50 failed\n");
			rc = -1;
		}
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects bWaitAll, INFINITE failed\n");
		rc = -1;
	}

	if (close_threads(THREADS, threads))
		return -1;

	/* Thread detach test */
	if (start_threads(THREADS, threads))
		return -1;

	if (close_threads(THREADS, threads))
		return -1;

	return 0;
}
