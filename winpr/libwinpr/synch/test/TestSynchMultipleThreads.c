
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#define THREADS 24

static DWORD WINAPI test_thread(LPVOID arg)
{
	long timeout = 300 + (rand() % 1000);
	WINPR_UNUSED(arg);
	Sleep(timeout);
	ExitThread(0);
	return 0;
}

static int start_threads(DWORD count, HANDLE* threads)
{
	DWORD i;

	for (i = 0; i < count; i++)
	{
		threads[i] = CreateThread(NULL, 0, test_thread, NULL, 0, NULL);

		if (!threads[i])
		{
			fprintf(stderr, "%s: CreateThread [%" PRIu32 "] failure\n", __FUNCTION__, i);
			return -1;
		}
	}

	return 0;
}

static int close_threads(DWORD count, HANDLE* threads)
{
	DWORD i;

	for (i = 0; i < count; i++)
	{
		if (!CloseHandle(threads[i]))
		{
			fprintf(stderr, "%s: CloseHandle [%" PRIu32 "] failure\n", __FUNCTION__, i);
			return -1;
		}
	}

	return 0;
}

static BOOL TestWaitForAll(void)
{
	BOOL rc = FALSE;
	DWORD ret;
	HANDLE threads[THREADS];
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	ret = WaitForMultipleObjects(THREADS, threads, TRUE, 50);
	if (ret != WAIT_TIMEOUT)
	{
		fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, timeout 50 failed, ret=%d\n",
		        __FUNCTION__, ret);
		goto fail;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, INFINITE failed\n", __FUNCTION__);
		goto fail;
	}

	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	rc = TRUE;
fail:
	return rc;
}

static BOOL TestWaitOne(void)
{
	BOOL rc = FALSE;
	DWORD ret;
	HANDLE threads[THREADS];
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	ret = WaitForMultipleObjects(THREADS, threads, FALSE, INFINITE);
	if (ret > (WAIT_OBJECT_0 + THREADS))
	{
		fprintf(stderr, "%s: WaitForMultipleObjects INFINITE failed\n", __FUNCTION__);
		goto fail;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, INFINITE failed\n", __FUNCTION__);
		goto fail;
	}

	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	rc = TRUE;
fail:
	return rc;
}

static BOOL TestWaitOneTimeout(void)
{
	BOOL rc = FALSE;
	DWORD ret;
	HANDLE threads[THREADS];
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	ret = WaitForMultipleObjects(THREADS, threads, FALSE, 50);
	if (ret != WAIT_TIMEOUT)
	{
		fprintf(stderr, "%s: WaitForMultipleObjects timeout 50 failed, ret=%d\n", __FUNCTION__,
		        ret);
		goto fail;
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, INFINITE failed\n", __FUNCTION__);
		goto fail;
	}

	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	rc = TRUE;
fail:
	return rc;
}

static BOOL TestWaitOneTimeoutMultijoin(void)
{
	BOOL rc = FALSE;
	DWORD ret, i;
	HANDLE threads[THREADS];
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	for (i = 0; i < THREADS; i++)
	{
		ret = WaitForMultipleObjects(THREADS, threads, FALSE, 0);
		if (ret != WAIT_TIMEOUT)
		{
			fprintf(stderr, "%s: WaitForMultipleObjects timeout 0 failed, ret=%d\n", __FUNCTION__,
			        ret);
			goto fail;
		}
	}

	if (WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, INFINITE failed\n", __FUNCTION__);
		goto fail;
	}

	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	rc = TRUE;
fail:
	return rc;
}

static BOOL TestDetach(void)
{
	HANDLE threads[THREADS];
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}

int TestSynchMultipleThreads(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!TestWaitForAll())
		return -1;

	if (!TestWaitOne())
		return -2;

	if (!TestWaitOneTimeout())
		return -3;

	if (!TestWaitOneTimeoutMultijoin())
		return -4;

	if (!TestDetach())
		return -5;

	return 0;
}
