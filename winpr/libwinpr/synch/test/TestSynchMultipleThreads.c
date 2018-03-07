
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

static DWORD WINAPI test_thread(LPVOID arg)
{
	long timeout = rand();
	timeout %= 1000;
	timeout += 100;
	Sleep(timeout);
	ExitThread(0);
	return 0;
}

static int start_threads(DWORD count, HANDLE *threads)
{
	DWORD i;

	for (i=0; i<count; i++)
	{
		threads[i] = CreateThread(NULL, 0, test_thread, NULL, 0, NULL);

		if (!threads[i])
		{
			printf("CreateThread [%"PRIu32"] failure\n", i);
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
			printf("CloseHandle [%"PRIu32"] failure\n", i);
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
		return 1;

	if (WaitForMultipleObjects(THREADS, threads, TRUE, 50) != WAIT_TIMEOUT)
	{
		printf("WaitForMultipleObjects bWaitAll, timeout 50 failed\n");
		rc = 2;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects bWaitAll, INFINITE failed\n");
		rc = 3;
	}

	if (close_threads(THREADS, threads))
		return 4;

	/* WaitOne, infinite */
	if (rc)
		return rc;

	if (start_threads(THREADS, threads))
		return 5;

	ev = WaitForMultipleObjects(THREADS, threads, FALSE, INFINITE);

	if ((ev < WAIT_OBJECT_0) || (ev > (WAIT_OBJECT_0 + THREADS)))
	{
		printf("WaitForMultipleObjects INFINITE failed\n");
		rc = 6;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects bWaitAll, INFINITE failed\n");
		rc = 7;
	}

	if (close_threads(THREADS, threads))
		return 8;

	if (rc)
		return rc;

	/* WaitOne, timeout */
	if (start_threads(THREADS, threads))
		return 9;

	if (WaitForMultipleObjects(THREADS, threads, FALSE, 50) != WAIT_TIMEOUT)
	{
		printf("WaitForMultipleObjects timeout 50 failed\n");
		rc = 10;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects bWaitAll, INFINITE failed\n");
		rc = 11;
	}

	if (close_threads(THREADS, threads))
		return 12;

	if (rc)
		return 13;

	/* WaitOne, timeout, multiple joins */
	if (start_threads(THREADS, threads))
		return 14;

	for (i = 0; i < THREADS; i++)
	{
		if (WaitForMultipleObjects(THREADS, threads, FALSE, 0) != WAIT_TIMEOUT)
		{
			printf("WaitForMultipleObjects timeout 50 failed\n");
			rc = 15;
		}
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForMultipleObjects bWaitAll, INFINITE failed\n");
		rc = 16;
	}

	if (close_threads(THREADS, threads))
		rc = 17;

	if (rc)
		return rc;

	/* Thread detach test */
	if (start_threads(THREADS, threads))
		return 18;

	if (close_threads(THREADS, threads))
		return 19;

	return 0;
}
