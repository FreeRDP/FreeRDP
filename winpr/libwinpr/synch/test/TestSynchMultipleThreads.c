
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#define THREADS 8

static UINT32 prand(UINT32 max)
{
	UINT32 tmp = 0;
	if (max <= 1)
		return 1;
	winpr_RAND(&tmp, sizeof(tmp));
	return tmp % (max - 1) + 1;
}

static DWORD WINAPI test_thread(LPVOID arg)
{
	UINT32 timeout = 50 + prand(100);
	WINPR_UNUSED(arg);
	Sleep(timeout);
	ExitThread(0);
	return 0;
}

static int start_threads(size_t count, HANDLE* threads)
{
	for (size_t i = 0; i < count; i++)
	{
		threads[i] = CreateThread(NULL, 0, test_thread, NULL, CREATE_SUSPENDED, NULL);

		if (!threads[i])
		{
			(void)fprintf(stderr, "%s: CreateThread [%" PRIuz "] failure\n", __func__, i);
			return -1;
		}
	}

	for (size_t i = 0; i < count; i++)
		ResumeThread(threads[i]);
	return 0;
}

static int close_threads(DWORD count, HANDLE* threads)
{
	int rc = 0;

	for (DWORD i = 0; i < count; i++)
	{
		if (!threads[i])
			continue;

		if (!CloseHandle(threads[i]))
		{
			(void)fprintf(stderr, "%s: CloseHandle [%" PRIu32 "] failure\n", __func__, i);
			rc = -1;
		}
		threads[i] = NULL;
	}

	return rc;
}

static BOOL TestWaitForAll(void)
{
	BOOL rc = FALSE;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: start_threads failed\n", __func__);
		goto fail;
	}

	const DWORD ret = WaitForMultipleObjects(ARRAYSIZE(threads), threads, TRUE, 10);
	if (ret != WAIT_TIMEOUT)
	{
		(void)fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, timeout 10 failed, ret=%d\n",
		              __func__, ret);
		goto fail;
	}

	if (WaitForMultipleObjects(ARRAYSIZE(threads), threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		(void)fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, INFINITE failed\n", __func__);
		goto fail;
	}

	rc = TRUE;
fail:
	if (close_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: close_threads failed\n", __func__);
		return FALSE;
	}

	return rc;
}

static BOOL TestWaitOne(void)
{
	BOOL rc = FALSE;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: start_threads failed\n", __func__);
		goto fail;
	}

	const DWORD ret = WaitForMultipleObjects(ARRAYSIZE(threads), threads, FALSE, INFINITE);
	if (ret > (WAIT_OBJECT_0 + ARRAYSIZE(threads)))
	{
		(void)fprintf(stderr, "%s: WaitForMultipleObjects INFINITE failed\n", __func__);
		goto fail;
	}

	if (WaitForMultipleObjects(ARRAYSIZE(threads), threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		(void)fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, INFINITE failed\n", __func__);
		goto fail;
	}

	rc = TRUE;
fail:
	if (close_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: close_threads failed\n", __func__);
		return FALSE;
	}

	return rc;
}

static BOOL TestWaitOneTimeout(void)
{
	BOOL rc = FALSE;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: start_threads failed\n", __func__);
		goto fail;
	}

	const DWORD ret = WaitForMultipleObjects(ARRAYSIZE(threads), threads, FALSE, 1);
	if (ret != WAIT_TIMEOUT)
	{
		(void)fprintf(stderr, "%s: WaitForMultipleObjects timeout 50 failed, ret=%d\n", __func__,
		              ret);
		goto fail;
	}

	if (WaitForMultipleObjects(ARRAYSIZE(threads), threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		(void)fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, INFINITE failed\n", __func__);
		goto fail;
	}
	rc = TRUE;
fail:
	if (close_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: close_threads failed\n", __func__);
		return FALSE;
	}

	return rc;
}

static BOOL TestWaitOneTimeoutMultijoin(void)
{
	BOOL rc = FALSE;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: start_threads failed\n", __func__);
		goto fail;
	}

	for (size_t i = 0; i < ARRAYSIZE(threads); i++)
	{
		const DWORD ret = WaitForMultipleObjects(ARRAYSIZE(threads), threads, FALSE, 0);
		if (ret != WAIT_TIMEOUT)
		{
			(void)fprintf(stderr, "%s: WaitForMultipleObjects timeout 0 failed, ret=%d\n", __func__,
			              ret);
			goto fail;
		}
	}

	if (WaitForMultipleObjects(ARRAYSIZE(threads), threads, TRUE, INFINITE) != WAIT_OBJECT_0)
	{
		(void)fprintf(stderr, "%s: WaitForMultipleObjects bWaitAll, INFINITE failed\n", __func__);
		goto fail;
	}

	rc = TRUE;
fail:
	if (close_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: close_threads failed\n", __func__);
		return FALSE;
	}

	return rc;
}

static BOOL TestDetach(void)
{
	BOOL rc = FALSE;
	HANDLE threads[THREADS] = { 0 };
	/* WaitForAll, timeout */
	if (start_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: start_threads failed\n", __func__);
		goto fail;
	}

	rc = TRUE;
fail:
	if (close_threads(ARRAYSIZE(threads), threads))
	{
		(void)fprintf(stderr, "%s: close_threads failed\n", __func__);
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
