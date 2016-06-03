
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>

#include "../synch.h"

static SYNCHRONIZATION_BARRIER gBarrier;
static HANDLE gStartEvent = NULL;

static LONG gThreadCount = 0;
static LONG gTrueCount   = 0;
static LONG gFalseCount  = 0;
static LONG gErrorCount  = 0;

#define LOOP_COUNT      200
#define THREAD_COUNT    32
#define MAX_SLEEP_MS    16

#define EXPECTED_TRUE_COUNT  LOOP_COUNT
#define EXPECTED_FALSE_COUNT (LOOP_COUNT * (THREAD_COUNT - 1))


DWORD WINAPI test_synch_barrier_thread(LPVOID lpParam)
{
	BOOL status = FALSE;
	DWORD i, tnum = InterlockedIncrement(&gThreadCount) - 1;

	DWORD dwFlags = *(DWORD*)lpParam;

	//printf("Thread #%03u entered.\n", tnum);

	/* wait for start event from main */
	if (WaitForSingleObject(gStartEvent, INFINITE) != WAIT_OBJECT_0)
	{
		InterlockedIncrement(&gErrorCount);
		goto out;
	}

	//printf("Thread #%03u unblocked.\n", tnum);

	for (i = 0; i < LOOP_COUNT && gErrorCount == 0; i++)
	{
		/* simulate different execution times before the barrier */
		Sleep(rand() % MAX_SLEEP_MS);
		status = EnterSynchronizationBarrier(&gBarrier, dwFlags);
		//printf("Thread #%03u status: %s\n", tnum, status ? "TRUE" : "FALSE");
		if (status)
			InterlockedIncrement(&gTrueCount);
		else
			InterlockedIncrement(&gFalseCount);
	}
out:
	//printf("Thread #%03u leaving.\n", tnum);
	return 0;
}


BOOL TestSynchBarrierWithFlags(DWORD dwFlags)
{
	HANDLE threads[THREAD_COUNT];
	DWORD dwStatus;
	int i;

	gThreadCount = 0;
	gTrueCount   = 0;
	gFalseCount  = 0;

	printf("%s: >> Testing with EnterSynchronizationBarrier flags 0x%08x\n", __FUNCTION__, dwFlags);

	if (!InitializeSynchronizationBarrier(&gBarrier, THREAD_COUNT, -1))
	{
		printf("%s: InitializeSynchronizationBarrier failed. GetLastError() = 0x%08x", __FUNCTION__, GetLastError());
		DeleteSynchronizationBarrier(&gBarrier);
		return FALSE;
	}

	if (!(gStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("%s: CreateEvent failed with error 0x%08x", __FUNCTION__, GetLastError());
		DeleteSynchronizationBarrier(&gBarrier);
		return FALSE;
	}

	for (i = 0; i < THREAD_COUNT; i++)
	{
		if (!(threads[i] = CreateThread(NULL, 0, test_synch_barrier_thread, &dwFlags, 0, NULL)))
		{
			printf("%s: CreateThread failed for thread #%u with error 0x%08x\n", __FUNCTION__, i, GetLastError());
			InterlockedIncrement(&gErrorCount);
			break;
		}
	}

	if (i > 0)
	{
		SetEvent(gStartEvent);

		if (WAIT_OBJECT_0 != (dwStatus = WaitForMultipleObjects(i, threads, TRUE, INFINITE)))
		{
			printf("%s: WaitForMultipleObjects unexpectedly returned %u (error = 0x%08x)\n",
				__FUNCTION__, dwStatus, GetLastError());
			gErrorCount++;
		}
	}

	CloseHandle(gStartEvent);
	DeleteSynchronizationBarrier(&gBarrier);

	if (gTrueCount != EXPECTED_TRUE_COUNT)
		InterlockedIncrement(&gErrorCount);

	if (gFalseCount != EXPECTED_FALSE_COUNT)
		InterlockedIncrement(&gErrorCount);

	printf("%s: gErrorCount: %d\n", __FUNCTION__, gErrorCount);
	printf("%s: gTrueCount:  %d (expected %d)\n", __FUNCTION__, gTrueCount, LOOP_COUNT);
	printf("%s: gFalseCount: %d (expected %d)\n", __FUNCTION__, gFalseCount, LOOP_COUNT * (THREAD_COUNT - 1));

	if (gErrorCount > 0)
	{
		printf("%s: Error test failed with %d reported errors\n", __FUNCTION__, gErrorCount);
		return FALSE;
	}

	return TRUE;
}


int TestSynchBarrier(int argc, char* argv[])
{
	/* Test invalid parameters */
	if (InitializeSynchronizationBarrier(&gBarrier, 0, -1))
	{
		printf("%s: InitializeSynchronizationBarrier unecpectedly succeeded with lTotalThreads = 0\n", __FUNCTION__);
		return -1;
	}

	if (InitializeSynchronizationBarrier(&gBarrier, -1, -1))
	{
		printf("%s: InitializeSynchronizationBarrier unecpectedly succeeded with lTotalThreads = -1\n", __FUNCTION__);
		return -1;
	}

	if (InitializeSynchronizationBarrier(&gBarrier, 1, -2))
	{
		printf("%s: InitializeSynchronizationBarrier unecpectedly succeeded with lSpinCount = -2\n", __FUNCTION__);
		return -1;
	}

	/* Functional tests */

	if (!TestSynchBarrierWithFlags(0))
		return -1;

	if (!TestSynchBarrierWithFlags(SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY))
		return -1;

	if (!TestSynchBarrierWithFlags(SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY))
		return -1;

	printf("%s: Test successfully completed\n", __FUNCTION__);

	return 0;
}
