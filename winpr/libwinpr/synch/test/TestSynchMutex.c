
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

static BOOL test_mutex_basic(void)
{
	HANDLE mutex = NULL;
	DWORD rc = 0;

	if (!(mutex = CreateMutex(NULL, FALSE, NULL)))
	{
		printf("%s: CreateMutex failed\n", __func__);
		return FALSE;
	}

	rc = WaitForSingleObject(mutex, INFINITE);

	if (rc != WAIT_OBJECT_0)
	{
		printf("%s: WaitForSingleObject on mutex failed with %" PRIu32 "\n", __func__, rc);
		return FALSE;
	}

	if (!ReleaseMutex(mutex))
	{
		printf("%s: ReleaseMutex failed\n", __func__);
		return FALSE;
	}

	if (ReleaseMutex(mutex))
	{
		printf("%s: ReleaseMutex unexpectedly succeeded on released mutex\n", __func__);
		return FALSE;
	}

	if (!CloseHandle(mutex))
	{
		printf("%s: CloseHandle on mutex failed\n", __func__);
		return FALSE;
	}

	return TRUE;
}

static BOOL test_mutex_recursive(void)
{
	HANDLE mutex = NULL;
	DWORD rc = 0;
	DWORD cnt = 50;

	if (!(mutex = CreateMutex(NULL, TRUE, NULL)))
	{
		printf("%s: CreateMutex failed\n", __func__);
		return FALSE;
	}

	for (UINT32 i = 0; i < cnt; i++)
	{
		rc = WaitForSingleObject(mutex, INFINITE);

		if (rc != WAIT_OBJECT_0)
		{
			printf("%s: WaitForSingleObject #%" PRIu32 " on mutex failed with %" PRIu32 "\n",
			       __func__, i, rc);
			return FALSE;
		}
	}

	for (UINT32 i = 0; i < cnt; i++)
	{
		if (!ReleaseMutex(mutex))
		{
			printf("%s: ReleaseMutex #%" PRIu32 " failed\n", __func__, i);
			return FALSE;
		}
	}

	if (!ReleaseMutex(mutex))
	{
		/* Note: The mutex was initially owned ! */
		printf("%s: Final ReleaseMutex failed\n", __func__);
		return FALSE;
	}

	if (ReleaseMutex(mutex))
	{
		printf("%s: ReleaseMutex unexpectedly succeeded on released mutex\n", __func__);
		return FALSE;
	}

	if (!CloseHandle(mutex))
	{
		printf("%s: CloseHandle on mutex failed\n", __func__);
		return FALSE;
	}

	return TRUE;
}

static HANDLE thread1_mutex1 = NULL;
static HANDLE thread1_mutex2 = NULL;
static BOOL thread1_failed = TRUE;

static DWORD WINAPI test_mutex_thread1(LPVOID lpParam)
{
	HANDLE hStartEvent = (HANDLE)lpParam;
	DWORD rc = 0;

	if (WaitForSingleObject(hStartEvent, INFINITE) != WAIT_OBJECT_0)
	{
		(void)fprintf(stderr, "%s: failed to wait for start event\n", __func__);
		return 0;
	}

	/**
	 * at this point:
	 * thread1_mutex1 is expected to be locked
	 * thread1_mutex2 is expected to be unlocked
	 * defined task:
	 * try to lock thread1_mutex1 (expected to fail)
	 * lock and unlock thread1_mutex2 (expected to work)
	 */
	rc = WaitForSingleObject(thread1_mutex1, 10);

	if (rc != WAIT_TIMEOUT)
	{
		(void)fprintf(stderr,
		              "%s: WaitForSingleObject on thread1_mutex1 unexpectedly returned %" PRIu32
		              " instead of WAIT_TIMEOUT (%u)\n",
		              __func__, rc, WAIT_TIMEOUT);
		return 0;
	}

	rc = WaitForSingleObject(thread1_mutex2, 10);

	if (rc != WAIT_OBJECT_0)
	{
		(void)fprintf(stderr,
		              "%s: WaitForSingleObject on thread1_mutex2 unexpectedly returned %" PRIu32
		              " instead of WAIT_OBJECT_0\n",
		              __func__, rc);
		return 0;
	}

	if (!ReleaseMutex(thread1_mutex2))
	{
		(void)fprintf(stderr, "%s: ReleaseMutex failed on thread1_mutex2\n", __func__);
		return 0;
	}

	thread1_failed = FALSE;
	return 0;
}

static BOOL test_mutex_threading(void)
{
	HANDLE hThread = NULL;
	HANDLE hStartEvent = NULL;

	if (!(thread1_mutex1 = CreateMutex(NULL, TRUE, NULL)))
	{
		printf("%s: CreateMutex thread1_mutex1 failed\n", __func__);
		goto fail;
	}

	if (!(thread1_mutex2 = CreateMutex(NULL, FALSE, NULL)))
	{
		printf("%s: CreateMutex thread1_mutex2 failed\n", __func__);
		goto fail;
	}

	if (!(hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		(void)fprintf(stderr, "%s: error creating start event\n", __func__);
		goto fail;
	}

	thread1_failed = TRUE;

	if (!(hThread = CreateThread(NULL, 0, test_mutex_thread1, (LPVOID)hStartEvent, 0, NULL)))
	{
		(void)fprintf(stderr, "%s: error creating test_mutex_thread_1\n", __func__);
		goto fail;
	}

	Sleep(100);

	if (!thread1_failed)
	{
		(void)fprintf(stderr, "%s: thread1 premature success\n", __func__);
		goto fail;
	}

	(void)SetEvent(hStartEvent);

	if (WaitForSingleObject(hThread, 2000) != WAIT_OBJECT_0)
	{
		(void)fprintf(stderr, "%s: thread1 premature success\n", __func__);
		goto fail;
	}

	if (thread1_failed)
	{
		(void)fprintf(stderr, "%s: thread1 has not reported success\n", __func__);
		goto fail;
	}

	/**
	 * - thread1 must not have succeeded to lock thread1_mutex1
	 * - thread1 must have locked and unlocked thread1_mutex2
	 */

	if (!ReleaseMutex(thread1_mutex1))
	{
		printf("%s: ReleaseMutex unexpectedly failed on thread1_mutex1\n", __func__);
		goto fail;
	}

	if (ReleaseMutex(thread1_mutex2))
	{
		printf("%s: ReleaseMutex unexpectedly succeeded on thread1_mutex2\n", __func__);
		goto fail;
	}

	(void)CloseHandle(hThread);
	(void)CloseHandle(hStartEvent);
	(void)CloseHandle(thread1_mutex1);
	(void)CloseHandle(thread1_mutex2);
	return TRUE;
fail:
	(void)ReleaseMutex(thread1_mutex1);
	(void)ReleaseMutex(thread1_mutex2);
	(void)CloseHandle(thread1_mutex1);
	(void)CloseHandle(thread1_mutex2);
	(void)CloseHandle(hStartEvent);
	(void)CloseHandle(hThread);
	return FALSE;
}

int TestSynchMutex(int argc, char* argv[])
{
	int rc = 0;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_mutex_basic())
		rc += 1;

	if (!test_mutex_recursive())
		rc += 2;

	if (!test_mutex_threading())
		rc += 4;

	printf("TestSynchMutex result %d\n", rc);
	return rc;
}
