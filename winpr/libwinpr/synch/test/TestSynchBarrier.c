
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>
#include <winpr/sysinfo.h>

#include "../synch.h"

static SYNCHRONIZATION_BARRIER gBarrier;
static HANDLE gStartEvent = NULL;
static LONG gErrorCount  = 0;

#define MAX_SLEEP_MS    32

struct test_params
{
	LONG threadCount;
	LONG trueCount;
	LONG falseCount;
	DWORD loops;
	DWORD flags;
};


static DWORD WINAPI test_synch_barrier_thread(LPVOID lpParam)
{
	BOOL status = FALSE;
	struct test_params* p = (struct test_params*)lpParam;
	DWORD i;

	InterlockedIncrement(&p->threadCount);

	//printf("Thread #%03u entered.\n", tnum);

	/* wait for start event from main */
	if (WaitForSingleObject(gStartEvent, INFINITE) != WAIT_OBJECT_0)
	{
		InterlockedIncrement(&gErrorCount);
		goto out;
	}

	//printf("Thread #%03u unblocked.\n", tnum);

	for (i = 0; i < p->loops && gErrorCount == 0; i++)
	{
		/* simulate different execution times before the barrier */
		Sleep(rand() % MAX_SLEEP_MS);
		status = EnterSynchronizationBarrier(&gBarrier, p->flags);

		//printf("Thread #%03u status: %s\n", tnum, status ? "TRUE" : "FALSE");
		if (status)
			InterlockedIncrement(&p->trueCount);
		else
			InterlockedIncrement(&p->falseCount);
	}

out:
	//printf("Thread #%03u leaving.\n", tnum);
	return 0;
}


static BOOL TestSynchBarrierWithFlags(DWORD dwFlags, DWORD dwThreads, DWORD dwLoops)
{
	HANDLE* threads;
	struct test_params p;
	DWORD dwStatus, expectedTrueCount, expectedFalseCount;
	DWORD i;
	p.threadCount = 0;
	p.trueCount   = 0;
	p.falseCount  = 0;
	p.loops = dwLoops;
	p.flags = dwFlags;
	expectedTrueCount = dwLoops;
	expectedFalseCount = dwLoops * (dwThreads - 1);
	printf("%s: >> Testing with flags 0x%08"PRIx32". Using %"PRIu32" threads performing %"PRIu32" loops\n",
	       __FUNCTION__, dwFlags, dwThreads, dwLoops);

	if (!(threads = calloc(dwThreads, sizeof(HANDLE))))
	{
		printf("%s: error allocatin thread array memory\n", __FUNCTION__);
		return FALSE;
	}

	if (!InitializeSynchronizationBarrier(&gBarrier, dwThreads, -1))
	{
		printf("%s: InitializeSynchronizationBarrier failed. GetLastError() = 0x%08x",
		       __FUNCTION__, GetLastError());
		free(threads);
		DeleteSynchronizationBarrier(&gBarrier);
		return FALSE;
	}

	if (!(gStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("%s: CreateEvent failed with error 0x%08x", __FUNCTION__,
		       GetLastError());
		free(threads);
		DeleteSynchronizationBarrier(&gBarrier);
		return FALSE;
	}

	for (i = 0; i < dwThreads; i++)
	{
		if (!(threads[i] = CreateThread(NULL, 0, test_synch_barrier_thread, &p, 0,
						NULL)))
		{
			printf("%s: CreateThread failed for thread #%"PRIu32" with error 0x%08x\n",
			       __FUNCTION__, i, GetLastError());
			InterlockedIncrement(&gErrorCount);
			break;
		}
	}

	if (i > 0)
	{
		if (!SetEvent(gStartEvent))
		{
			printf("%s: SetEvent(gStartEvent) failed with error = 0x%08x)\n",
			       __FUNCTION__, GetLastError());
			InterlockedIncrement(&gErrorCount);
		}

		while (i--)
		{
			if (WAIT_OBJECT_0 != (dwStatus = WaitForSingleObject(threads[i], INFINITE)))
			{
				printf("%s: WaitForSingleObject(thread[%"PRIu32"] unexpectedly returned %"PRIu32" (error = 0x%08x)\n",
				       __FUNCTION__, i, dwStatus, GetLastError());
				InterlockedIncrement(&gErrorCount);
			}

			if (!CloseHandle(threads[i]))
			{
				printf("%s: CloseHandle(thread[%"PRIu32"]) failed with error = 0x%08x)\n",
				       __FUNCTION__, i, GetLastError());
				InterlockedIncrement(&gErrorCount);
			}
		}
	}

	free(threads);

	if (!CloseHandle(gStartEvent))
	{
		printf("%s: CloseHandle(gStartEvent) failed with error = 0x%08x)\n",
		       __FUNCTION__, GetLastError());
		InterlockedIncrement(&gErrorCount);
	}

	DeleteSynchronizationBarrier(&gBarrier);

	if (p.threadCount != dwThreads)
		InterlockedIncrement(&gErrorCount);

	if (p.trueCount != expectedTrueCount)
		InterlockedIncrement(&gErrorCount);

	if (p.falseCount != expectedFalseCount)
		InterlockedIncrement(&gErrorCount);

	printf("%s: error count:  %"PRId32"\n", __FUNCTION__, gErrorCount);
	printf("%s: thread count: %"PRId32" (expected %"PRIu32")\n", __FUNCTION__, p.threadCount,
	       dwThreads);
	printf("%s: true count:   %"PRId32" (expected %"PRIu32")\n", __FUNCTION__, p.trueCount,
	       expectedTrueCount);
	printf("%s: false count:  %"PRId32" (expected %"PRIu32")\n", __FUNCTION__, p.falseCount,
	       expectedFalseCount);

	if (gErrorCount > 0)
	{
		printf("%s: Error test failed with %"PRId32" reported errors\n", __FUNCTION__,
		       gErrorCount);
		return FALSE;
	}

	return TRUE;
}


int TestSynchBarrier(int argc, char* argv[])
{
	SYSTEM_INFO sysinfo;
	DWORD dwMaxThreads;
	DWORD dwMinThreads;
	DWORD dwNumLoops = 200;
	GetNativeSystemInfo(&sysinfo);
	printf("%s: Number of processors: %"PRIu32"\n", __FUNCTION__,
	       sysinfo.dwNumberOfProcessors);
	dwMinThreads = sysinfo.dwNumberOfProcessors;
	dwMaxThreads = sysinfo.dwNumberOfProcessors * 4;

	if (dwMaxThreads > 32)
		dwMaxThreads = 32;

	/* Test invalid parameters */
	if (InitializeSynchronizationBarrier(&gBarrier, 0, -1))
	{
		printf("%s: InitializeSynchronizationBarrier unecpectedly succeeded with lTotalThreads = 0\n",
		       __FUNCTION__);
		return -1;
	}

	if (InitializeSynchronizationBarrier(&gBarrier, -1, -1))
	{
		printf("%s: InitializeSynchronizationBarrier unecpectedly succeeded with lTotalThreads = -1\n",
		       __FUNCTION__);
		return -1;
	}

	if (InitializeSynchronizationBarrier(&gBarrier, 1, -2))
	{
		printf("%s: InitializeSynchronizationBarrier unecpectedly succeeded with lSpinCount = -2\n",
		       __FUNCTION__);
		return -1;
	}

	/* Functional tests */

	if (!TestSynchBarrierWithFlags(0, dwMaxThreads, dwNumLoops))
		return -1;

	if (!TestSynchBarrierWithFlags(SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY,
				       dwMinThreads, dwNumLoops))
		return -1;

	if (!TestSynchBarrierWithFlags(SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY,
				       dwMaxThreads, dwNumLoops))
		return -1;

	printf("%s: Test successfully completed\n", __FUNCTION__);
	return 0;
}
