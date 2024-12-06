
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/windows.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>

#define TEST_SYNC_CRITICAL_TEST1_RUNTIME_MS 50
#define TEST_SYNC_CRITICAL_TEST1_RUNS 4

static CRITICAL_SECTION critical;
static LONG gTestValueVulnerable = 0;
static LONG gTestValueSerialized = 0;

static BOOL TestSynchCritical_TriggerAndCheckRaceCondition(HANDLE OwningThread, LONG RecursionCount)
{
	/* if called unprotected this will hopefully trigger a race condition ... */
	gTestValueVulnerable++;

	if (critical.OwningThread != OwningThread)
	{
		printf("CriticalSection failure: OwningThread is invalid\n");
		return FALSE;
	}
	if (critical.RecursionCount != RecursionCount)
	{
		printf("CriticalSection failure: RecursionCount is invalid\n");
		return FALSE;
	}

	/* ... which we try to detect using the serialized counter */
	if (gTestValueVulnerable != InterlockedIncrement(&gTestValueSerialized))
	{
		printf("CriticalSection failure: Data corruption detected\n");
		return FALSE;
	}

	return TRUE;
}

static UINT32 prand(UINT32 max)
{
	UINT32 tmp = 0;
	if (max <= 1)
		return 1;
	winpr_RAND(&tmp, sizeof(tmp));
	return tmp % (max - 1) + 1;
}

/* this thread function shall increment the global dwTestValue until the PBOOL passed in arg is
 * FALSE */
static DWORD WINAPI TestSynchCritical_Test1(LPVOID arg)
{
	int rc = 0;
	HANDLE hThread = (HANDLE)(ULONG_PTR)GetCurrentThreadId();

	PBOOL pbContinueRunning = (PBOOL)arg;

	while (*pbContinueRunning)
	{
		EnterCriticalSection(&critical);

		rc = 1;

		if (!TestSynchCritical_TriggerAndCheckRaceCondition(hThread, rc))
			return 1;

		/* add some random recursion level */
		UINT32 j = prand(5);
		for (UINT32 i = 0; i < j; i++)
		{
			if (!TestSynchCritical_TriggerAndCheckRaceCondition(hThread, rc++))
				return 2;
			EnterCriticalSection(&critical);
		}
		for (UINT32 i = 0; i < j; i++)
		{
			if (!TestSynchCritical_TriggerAndCheckRaceCondition(hThread, rc--))
				return 2;
			LeaveCriticalSection(&critical);
		}

		if (!TestSynchCritical_TriggerAndCheckRaceCondition(hThread, rc))
			return 3;

		LeaveCriticalSection(&critical);
	}

	return 0;
}

/* this thread function tries to call TryEnterCriticalSection while the main thread holds the lock
 */
static DWORD WINAPI TestSynchCritical_Test2(LPVOID arg)
{
	WINPR_UNUSED(arg);
	if (TryEnterCriticalSection(&critical) == TRUE)
	{
		LeaveCriticalSection(&critical);
		return 1;
	}
	return 0;
}

static DWORD WINAPI TestSynchCritical_Main(LPVOID arg)
{
	SYSTEM_INFO sysinfo;
	DWORD dwPreviousSpinCount = 0;
	DWORD dwSpinCount = 0;
	DWORD dwSpinCountExpected = 0;
	HANDLE hMainThread = NULL;
	HANDLE* hThreads = NULL;
	HANDLE hThread = NULL;
	DWORD dwThreadCount = 0;
	DWORD dwThreadExitCode = 0;
	BOOL bTest1Running = 0;

	PBOOL pbThreadTerminated = (PBOOL)arg;

	GetNativeSystemInfo(&sysinfo);

	hMainThread = (HANDLE)(ULONG_PTR)GetCurrentThreadId();

	/**
	 * Test SpinCount in SetCriticalSectionSpinCount, InitializeCriticalSectionEx and
	 * InitializeCriticalSectionAndSpinCount SpinCount must be forced to be zero on on uniprocessor
	 * systems and on systems where WINPR_CRITICAL_SECTION_DISABLE_SPINCOUNT is defined
	 */

	dwSpinCount = 100;
	InitializeCriticalSectionEx(&critical, dwSpinCount, 0);
	while (--dwSpinCount)
	{
		dwPreviousSpinCount = SetCriticalSectionSpinCount(&critical, dwSpinCount);
		dwSpinCountExpected = 0;
#if !defined(WINPR_CRITICAL_SECTION_DISABLE_SPINCOUNT)
		if (sysinfo.dwNumberOfProcessors > 1)
			dwSpinCountExpected = dwSpinCount + 1;
#endif
		if (dwPreviousSpinCount != dwSpinCountExpected)
		{
			printf("CriticalSection failure: SetCriticalSectionSpinCount returned %" PRIu32
			       " (expected: %" PRIu32 ")\n",
			       dwPreviousSpinCount, dwSpinCountExpected);
			goto fail;
		}

		DeleteCriticalSection(&critical);

		if (dwSpinCount % 2 == 0)
			InitializeCriticalSectionAndSpinCount(&critical, dwSpinCount);
		else
			InitializeCriticalSectionEx(&critical, dwSpinCount, 0);
	}
	DeleteCriticalSection(&critical);

	/**
	 * Test single-threaded recursive
	 * TryEnterCriticalSection/EnterCriticalSection/LeaveCriticalSection
	 *
	 */

	InitializeCriticalSection(&critical);

	int i = 0;
	for (; i < 10; i++)
	{
		if (critical.RecursionCount != i)
		{
			printf("CriticalSection failure: RecursionCount field is %" PRId32 " instead of %d.\n",
			       critical.RecursionCount, i);
			goto fail;
		}
		if (i % 2 == 0)
		{
			EnterCriticalSection(&critical);
		}
		else
		{
			if (TryEnterCriticalSection(&critical) == FALSE)
			{
				printf("CriticalSection failure: TryEnterCriticalSection failed where it should "
				       "not.\n");
				goto fail;
			}
		}
		if (critical.OwningThread != hMainThread)
		{
			printf("CriticalSection failure: Could not verify section ownership (loop index=%d).\n",
			       i);
			goto fail;
		}
	}
	while (--i >= 0)
	{
		LeaveCriticalSection(&critical);
		if (critical.RecursionCount != i)
		{
			printf("CriticalSection failure: RecursionCount field is %" PRId32 " instead of %d.\n",
			       critical.RecursionCount, i);
			goto fail;
		}
		if (critical.OwningThread != (i ? hMainThread : NULL))
		{
			printf("CriticalSection failure: Could not verify section ownership (loop index=%d).\n",
			       i);
			goto fail;
		}
	}
	DeleteCriticalSection(&critical);

	/**
	 * Test using multiple threads modifying the same value
	 */

	dwThreadCount = sysinfo.dwNumberOfProcessors > 1 ? sysinfo.dwNumberOfProcessors : 2;

	hThreads = (HANDLE*)calloc(dwThreadCount, sizeof(HANDLE));
	if (!hThreads)
	{
		printf("Problem allocating memory\n");
		goto fail;
	}

	for (int j = 0; j < TEST_SYNC_CRITICAL_TEST1_RUNS; j++)
	{
		dwSpinCount = j * 100;
		InitializeCriticalSectionAndSpinCount(&critical, dwSpinCount);

		gTestValueVulnerable = 0;
		gTestValueSerialized = 0;

		/* the TestSynchCritical_Test1 threads shall run until bTest1Running is FALSE */
		bTest1Running = TRUE;
		for (int i = 0; i < (int)dwThreadCount; i++)
		{
			if (!(hThreads[i] =
			          CreateThread(NULL, 0, TestSynchCritical_Test1, &bTest1Running, 0, NULL)))
			{
				printf("CriticalSection failure: Failed to create test_1 thread #%d\n", i);
				goto fail;
			}
		}

		/* let it run for TEST_SYNC_CRITICAL_TEST1_RUNTIME_MS ... */
		Sleep(TEST_SYNC_CRITICAL_TEST1_RUNTIME_MS);
		bTest1Running = FALSE;

		for (int i = 0; i < (int)dwThreadCount; i++)
		{
			if (WaitForSingleObject(hThreads[i], INFINITE) != WAIT_OBJECT_0)
			{
				printf("CriticalSection failure: Failed to wait for thread #%d\n", i);
				goto fail;
			}
			GetExitCodeThread(hThreads[i], &dwThreadExitCode);
			if (dwThreadExitCode != 0)
			{
				printf("CriticalSection failure: Thread #%d returned error code %" PRIu32 "\n", i,
				       dwThreadExitCode);
				goto fail;
			}
			(void)CloseHandle(hThreads[i]);
		}

		if (gTestValueVulnerable != gTestValueSerialized)
		{
			printf("CriticalSection failure: unexpected test value %" PRId32 " (expected %" PRId32
			       ")\n",
			       gTestValueVulnerable, gTestValueSerialized);
			goto fail;
		}

		DeleteCriticalSection(&critical);
	}

	free((void*)hThreads);

	/**
	 * TryEnterCriticalSection in thread must fail if we hold the lock in the main thread
	 */

	InitializeCriticalSection(&critical);

	if (TryEnterCriticalSection(&critical) == FALSE)
	{
		printf("CriticalSection failure: TryEnterCriticalSection unexpectedly failed.\n");
		goto fail;
	}
	/* This thread tries to call TryEnterCriticalSection which must fail */
	if (!(hThread = CreateThread(NULL, 0, TestSynchCritical_Test2, NULL, 0, NULL)))
	{
		printf("CriticalSection failure: Failed to create test_2 thread\n");
		goto fail;
	}
	if (WaitForSingleObject(hThread, INFINITE) != WAIT_OBJECT_0)
	{
		printf("CriticalSection failure: Failed to wait for thread\n");
		goto fail;
	}
	GetExitCodeThread(hThread, &dwThreadExitCode);
	if (dwThreadExitCode != 0)
	{
		printf("CriticalSection failure: Thread returned error code %" PRIu32 "\n",
		       dwThreadExitCode);
		goto fail;
	}
	(void)CloseHandle(hThread);

	*pbThreadTerminated = TRUE; /* requ. for winpr issue, see below */
	return 0;

fail:
	*pbThreadTerminated = TRUE; /* requ. for winpr issue, see below */
	return 1;
}

int TestSynchCritical(int argc, char* argv[])
{
	BOOL bThreadTerminated = FALSE;
	HANDLE hThread = NULL;
	DWORD dwThreadExitCode = 0;
	DWORD dwDeadLockDetectionTimeMs = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	dwDeadLockDetectionTimeMs =
	    2 * TEST_SYNC_CRITICAL_TEST1_RUNTIME_MS * TEST_SYNC_CRITICAL_TEST1_RUNS;

	printf("Deadlock will be assumed after %" PRIu32 " ms.\n", dwDeadLockDetectionTimeMs);

	if (!(hThread = CreateThread(NULL, 0, TestSynchCritical_Main, &bThreadTerminated, 0, NULL)))
	{
		printf("CriticalSection failure: Failed to create main thread\n");
		return -1;
	}

	/**
	 * We have to be able to detect dead locks in this test.
	 * At the time of writing winpr's WaitForSingleObject has not implemented timeout for thread
	 * wait
	 *
	 * Workaround checking the value of bThreadTerminated which is passed in the thread arg
	 */

	for (DWORD i = 0; i < dwDeadLockDetectionTimeMs; i += 10)
	{
		if (bThreadTerminated)
			break;

		Sleep(10);
	}

	if (!bThreadTerminated)
	{
		printf("CriticalSection failure: Possible dead lock detected\n");
		return -1;
	}

	GetExitCodeThread(hThread, &dwThreadExitCode);
	(void)CloseHandle(hThread);

	if (dwThreadExitCode != 0)
	{
		return -1;
	}

	return 0;
}
