
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>

#define TEST_SYNC_CRITICAL_TEST1_RUNTIME_MS 500
#define TEST_SYNC_CRITICAL_TEST1_RUNS 4

CRITICAL_SECTION critical;
LONG gTestValueVulnerable = 0;
LONG gTestValueSerialized = 0;

BOOL TestSynchCritical_TriggerAndCheckRaceCondition(HANDLE OwningThread, LONG RecursionCount)
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

/* this thread function shall increment the global dwTestValue until the PBOOL passsed in arg is FALSE */
static PVOID TestSynchCritical_Test1(PVOID arg)
{
	int i, j, rc;
	HANDLE hThread = (HANDLE) (ULONG_PTR) GetCurrentThreadId();

	PBOOL pbContinueRunning = (PBOOL)arg;

	while(*pbContinueRunning)
	{
		EnterCriticalSection(&critical);

		rc = 1;

		if (!TestSynchCritical_TriggerAndCheckRaceCondition(hThread, rc))
			return (PVOID)1;

		/* add some random recursion level */
		j = rand()%5;
		for (i=0; i<j; i++)
		{
			if (!TestSynchCritical_TriggerAndCheckRaceCondition(hThread, rc++))
				return (PVOID)2;
			EnterCriticalSection(&critical);
		}
		for (i=0; i<j; i++)
		{
			if (!TestSynchCritical_TriggerAndCheckRaceCondition(hThread, rc--))
				return (PVOID)2;
			LeaveCriticalSection(&critical);
		}

		if (!TestSynchCritical_TriggerAndCheckRaceCondition(hThread, rc))
			return (PVOID)3;

		LeaveCriticalSection(&critical);
	}

	return 0;
}

/* this thread function tries to call TryEnterCriticalSection while the main thread holds the lock */
static PVOID TestSynchCritical_Test2(PVOID arg)
{
	if (TryEnterCriticalSection(&critical)==TRUE)
	{
		LeaveCriticalSection(&critical);
		return (PVOID)1;
	}
	return (PVOID)0;
}


static PVOID TestSynchCritical_Main(PVOID arg)
{
	int i, j;
	SYSTEM_INFO sysinfo;
	DWORD dwPreviousSpinCount;
	DWORD dwSpinCount;
	DWORD dwSpinCountExpected;
	HANDLE hMainThread;
	HANDLE* hThreads;
	HANDLE hThread;
	DWORD dwThreadCount;
	DWORD dwThreadExitCode;
	BOOL bTest1Running;

	PBOOL pbThreadTerminated = (PBOOL)arg;

	GetNativeSystemInfo(&sysinfo);

	hMainThread = (HANDLE) (ULONG_PTR) GetCurrentThreadId();

	/**
	 * Test SpinCount in SetCriticalSectionSpinCount, InitializeCriticalSectionEx and InitializeCriticalSectionAndSpinCount
	 * SpinCount must be forced to be zero on on uniprocessor systems and on systems
	 * where WINPR_CRITICAL_SECTION_DISABLE_SPINCOUNT is defined
	 */

	dwSpinCount = 100;
	InitializeCriticalSectionEx(&critical, dwSpinCount, 0);
	while(--dwSpinCount)
	{
		dwPreviousSpinCount = SetCriticalSectionSpinCount(&critical, dwSpinCount);
		dwSpinCountExpected = 0;
#if !defined(WINPR_CRITICAL_SECTION_DISABLE_SPINCOUNT)
		if (sysinfo.dwNumberOfProcessors > 1)
			dwSpinCountExpected = dwSpinCount+1;
#endif
		if (dwPreviousSpinCount != dwSpinCountExpected)
		{
			printf("CriticalSection failure: SetCriticalSectionSpinCount returned %u (expected: %u)\n", dwPreviousSpinCount, dwSpinCountExpected);
			goto fail;
		}

		DeleteCriticalSection(&critical);

		if (dwSpinCount%2==0)
			InitializeCriticalSectionAndSpinCount(&critical, dwSpinCount);
		else
			InitializeCriticalSectionEx(&critical, dwSpinCount, 0);
	}
	DeleteCriticalSection(&critical);


	/**
	 * Test single-threaded recursive TryEnterCriticalSection/EnterCriticalSection/LeaveCriticalSection
	 *
	 */

	InitializeCriticalSection(&critical);

	for (i=0; i<1000; i++)
	{
		if (critical.RecursionCount != i)
		{
			printf("CriticalSection failure: RecursionCount field is %d instead of %d.\n", critical.RecursionCount, i);
			goto fail;
		}
		if (i%2==0)
		{
			EnterCriticalSection(&critical);
		}
		else
		{
			if (TryEnterCriticalSection(&critical) == FALSE)
			{
				printf("CriticalSection failure: TryEnterCriticalSection failed where it should not.\n");
				goto fail;
			}
		}
		if (critical.OwningThread != hMainThread)
		{
			printf("CriticalSection failure: Could not verify section ownership (loop index=%d).\n", i);
			goto fail;
		}
	}
	while (--i >= 0)
	{
		LeaveCriticalSection(&critical);
		if (critical.RecursionCount != i)
		{
			printf("CriticalSection failure: RecursionCount field is %d instead of %d.\n", critical.RecursionCount, i);
			goto fail;
		}
		if (critical.OwningThread != (HANDLE)(i ? hMainThread : NULL))
		{
			printf("CriticalSection failure: Could not verify section ownership (loop index=%d).\n", i);
			goto fail;
		}
	}
	DeleteCriticalSection(&critical);


	/**
	 * Test using multiple threads modifying the same value
	 */

	dwThreadCount = sysinfo.dwNumberOfProcessors > 1 ? sysinfo.dwNumberOfProcessors : 2;

	hThreads = (HANDLE*)calloc(dwThreadCount, sizeof(HANDLE));

	for (j=0; j < TEST_SYNC_CRITICAL_TEST1_RUNS; j++)
	{
		dwSpinCount = j * 1000;
		InitializeCriticalSectionAndSpinCount(&critical, dwSpinCount);

		gTestValueVulnerable = 0;
		gTestValueSerialized = 0;

		/* the TestSynchCritical_Test1 threads shall run until bTest1Running is FALSE */
		bTest1Running = TRUE;
		for (i=0; i<dwThreadCount; i++)	{
			hThreads[i] = CreateThread(NULL, 0,  (LPTHREAD_START_ROUTINE) TestSynchCritical_Test1, &bTest1Running, 0, NULL);
		}
		/* let it run for TEST_SYNC_CRITICAL_TEST1_RUNTIME_MS ... */
		Sleep(TEST_SYNC_CRITICAL_TEST1_RUNTIME_MS);
		bTest1Running = FALSE;

		for (i=0; i<dwThreadCount; i++)
		{
			if (WaitForSingleObject(hThreads[i], INFINITE) != WAIT_OBJECT_0)
			{
				printf("CriticalSection failure: Failed to wait for thread #%d\n", i);
				goto fail;
			}
			GetExitCodeThread(hThreads[i], &dwThreadExitCode);
			if(dwThreadExitCode != 0)
			{
				printf("CriticalSection failure: Thread #%d returned error code %u\n", i, dwThreadExitCode);
				goto fail;
			}
			CloseHandle(hThreads[i]);
		}

		if (gTestValueVulnerable != gTestValueSerialized)
		{
			printf("CriticalSection failure: unexpected test value %d (expected %d)\n", gTestValueVulnerable, gTestValueSerialized);
			goto fail;
		}

		DeleteCriticalSection(&critical);
	}

	free(hThreads);


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
	hThread = CreateThread(NULL, 0,  (LPTHREAD_START_ROUTINE) TestSynchCritical_Test2, NULL, 0, NULL);
	if (WaitForSingleObject(hThread, INFINITE) != WAIT_OBJECT_0)
	{
		printf("CriticalSection failure: Failed to wait for thread\n");
		goto fail;
	}
	GetExitCodeThread(hThread, &dwThreadExitCode);
	if(dwThreadExitCode != 0)
	{
		printf("CriticalSection failure: Thread returned error code %u\n", dwThreadExitCode);
		goto fail;
	}
	CloseHandle(hThread);

	*pbThreadTerminated = TRUE; /* requ. for winpr issue, see below */
	return (PVOID)0;

fail:
	*pbThreadTerminated = TRUE; /* requ. for winpr issue, see below */
	return (PVOID)1;
}


int TestSynchCritical(int argc, char* argv[])
{
	BOOL bThreadTerminated = FALSE;
	HANDLE hThread;
	DWORD dwThreadExitCode;
	DWORD dwDeadLockDetectionTimeMs;
	int i;

	dwDeadLockDetectionTimeMs = 2 * TEST_SYNC_CRITICAL_TEST1_RUNTIME_MS * TEST_SYNC_CRITICAL_TEST1_RUNS;

	printf("Deadlock will be assumed after %u ms.\n", dwDeadLockDetectionTimeMs);

	hThread = CreateThread(NULL, 0,  (LPTHREAD_START_ROUTINE) TestSynchCritical_Main, &bThreadTerminated, 0, NULL);

	/**
	 * We have to be able to detect dead locks in this test.
	 * At the time of writing winpr's WaitForSingleObject has not implemented timeout for thread wait
	 *
	 * Workaround checking the value of bThreadTerminated which is passed in the thread arg
	 */

	for (i=0; i<dwDeadLockDetectionTimeMs; i+=100)
	{
		if (bThreadTerminated)
			break;
		Sleep(100);
	}

	if (!bThreadTerminated)
	{
		printf("CriticalSection failure: Possible dead lock detected\n");
		return -1;
	}

	GetExitCodeThread(hThread, &dwThreadExitCode);
	CloseHandle(hThread);

	if(dwThreadExitCode != 0)
	{
		return -1;
	}

	return 0;
}
