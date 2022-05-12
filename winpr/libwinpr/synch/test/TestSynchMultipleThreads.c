
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#define THREADS 8

static DWORD WINAPI test_thread(LPVOID arg)
{
	long timeout = 30 + (rand() % 100);
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
		threads[i] = CreateThread(NULL, 0, test_thread, NULL, CREATE_SUSPENDED, NULL);

		if (!threads[i])
		{
			fprintf(stderr, "%s: CreateThread [%" PRIu32 "] failure\n", __FUNCTION__, i);
			return -1;
		}
	}

	for (i = 0; i < count; i++)
		ResumeThread(threads[i]);
	return 0;
}

static int close_threads(DWORD count, HANDLE* threads)
{
	int rc = 0;
	DWORD i;

	for (i = 0; i < count; i++)
	{
		if (!threads[i])
			continue;

		if (!CloseHandle(threads[i]))
		{
			fprintf(stderr, "%s: CloseHandle [%" PRIu32 "] failure\n", __FUNCTION__, i);
			rc = -1;
		}
		threads[i] = NULL;
	}

	return rc;
}

static BOOL TestWaitForAll(void)
{
	BOOL rc = FALSE;
	DWORD ret;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		goto fail;
	}

	ret = WaitForMultipleObjects(THREADS, threads, TRUE, 10);
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

	rc = TRUE;
fail:
	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	return rc;
}

static BOOL TestWaitOne(void)
{
	BOOL rc = FALSE;
	DWORD ret;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		goto fail;
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

	rc = TRUE;
fail:
	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	return rc;
}

static BOOL TestWaitOneTimeout(void)
{
	BOOL rc = FALSE;
	DWORD ret;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		goto fail;
	}

	ret = WaitForMultipleObjects(THREADS, threads, FALSE, 1);
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
	rc = TRUE;
fail:
	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	return rc;
}

static BOOL TestWaitOneTimeoutMultijoin(void)
{
	BOOL rc = FALSE;
	DWORD ret, i;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		goto fail;
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

	rc = TRUE;
fail:
	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	return rc;
}

static BOOL TestDetach(void)
{
	BOOL rc = FALSE;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: start_threads failed\n", __FUNCTION__);
		goto fail;
	}

	rc = TRUE;
fail:
	if (close_threads(THREADS, threads))
	{
		fprintf(stderr, "%s: close_threads failed\n", __FUNCTION__);
		return FALSE;
	}

	return rc;
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
