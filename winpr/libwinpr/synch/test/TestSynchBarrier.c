
#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>
#include <winpr/sysinfo.h>

#include "../synch.h"

static SYNCHRONIZATION_BARRIER gBarrier;
static HANDLE gStartEvent = NULL;
static LONG gErrorCount = 0;

#define MAX_SLEEP_MS 22

struct test_params
{
	LONG threadCount;
	LONG trueCount;
	LONG falseCount;
	DWORD loops;
	DWORD flags;
};

static UINT32 prand(UINT32 max)
{
	UINT32 tmp = 0;
	if (max <= 1)
		return 1;
	winpr_RAND(&tmp, sizeof(tmp));
	return tmp % (max - 1) + 1;
}

static DWORD WINAPI test_synch_barrier_thread(LPVOID lpParam)
{
	BOOL status = FALSE;
	struct test_params* p = (struct test_params*)lpParam;

	InterlockedIncrement(&p->threadCount);

	// printf("Thread #%03u entered.\n", tnum);

	/* wait for start event from main */
	if (WaitForSingleObject(gStartEvent, INFINITE) != WAIT_OBJECT_0)
	{
		InterlockedIncrement(&gErrorCount);
		goto out;
	}

	// printf("Thread #%03u unblocked.\n", tnum);

	for (DWORD i = 0; i < p->loops && gErrorCount == 0; i++)
	{
		/* simulate different execution times before the barrier */
		Sleep(1 + prand(MAX_SLEEP_MS));
		status = EnterSynchronizationBarrier(&gBarrier, p->flags);

		// printf("Thread #%03u status: %s\n", tnum, status ? "TRUE" : "FALSE");
		if (status)
			InterlockedIncrement(&p->trueCount);
		else
			InterlockedIncrement(&p->falseCount);
	}

out:
	// printf("Thread #%03u leaving.\n", tnum);
	return 0;
}

static BOOL TestSynchBarrierWithFlags(DWORD dwFlags, DWORD dwThreads, DWORD dwLoops)
{
	BOOL rc = FALSE;
	DWORD dwStatus = 0;

	struct test_params p = {
		.threadCount = 0, .trueCount = 0, .falseCount = 0, .loops = dwLoops, .flags = dwFlags
	};
	DWORD expectedTrueCount = dwLoops;
	DWORD expectedFalseCount = dwLoops * (dwThreads - 1);

	printf("%s: >> Testing with flags 0x%08" PRIx32 ". Using %" PRIu32
	       " threads performing %" PRIu32 " loops\n",
	       __func__, dwFlags, dwThreads, dwLoops);

	HANDLE* threads = (HANDLE*)calloc(dwThreads, sizeof(HANDLE));
	if (!threads)
	{
		printf("%s: error allocatin thread array memory\n", __func__);
		return FALSE;
	}

	if (dwThreads > INT32_MAX)
		goto fail;

	if (!InitializeSynchronizationBarrier(&gBarrier, (LONG)dwThreads, -1))
	{
		printf("%s: InitializeSynchronizationBarrier failed. GetLastError() = 0x%08x", __func__,
		       GetLastError());
		goto fail;
	}

	if (!(gStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("%s: CreateEvent failed with error 0x%08x", __func__, GetLastError());
		goto fail;
	}

	DWORD i = 0;
	for (; i < dwThreads; i++)
	{
		threads[i] = CreateThread(NULL, 0, test_synch_barrier_thread, &p, 0, NULL);
		if (!threads[i])
		{
			printf("%s: CreateThread failed for thread #%" PRIu32 " with error 0x%08x\n", __func__,
			       i, GetLastError());
			InterlockedIncrement(&gErrorCount);
			break;
		}
	}

	if (i > 0)
	{
		if (!SetEvent(gStartEvent))
		{
			printf("%s: SetEvent(gStartEvent) failed with error = 0x%08x)\n", __func__,
			       GetLastError());
			InterlockedIncrement(&gErrorCount);
		}

		while (i--)
		{
			if (WAIT_OBJECT_0 != (dwStatus = WaitForSingleObject(threads[i], INFINITE)))
			{
				printf("%s: WaitForSingleObject(thread[%" PRIu32 "] unexpectedly returned %" PRIu32
				       " (error = 0x%08x)\n",
				       __func__, i, dwStatus, GetLastError());
				InterlockedIncrement(&gErrorCount);
			}

			if (!CloseHandle(threads[i]))
			{
				printf("%s: CloseHandle(thread[%" PRIu32 "]) failed with error = 0x%08x)\n",
				       __func__, i, GetLastError());
				InterlockedIncrement(&gErrorCount);
			}
		}
	}

	if (!CloseHandle(gStartEvent))
	{
		printf("%s: CloseHandle(gStartEvent) failed with error = 0x%08x)\n", __func__,
		       GetLastError());
		InterlockedIncrement(&gErrorCount);
	}

	if (p.threadCount != (INT64)dwThreads)
		InterlockedIncrement(&gErrorCount);

	if (p.trueCount != (INT64)expectedTrueCount)
		InterlockedIncrement(&gErrorCount);

	if (p.falseCount != (INT64)expectedFalseCount)
		InterlockedIncrement(&gErrorCount);

	printf("%s: error count:  %" PRId32 "\n", __func__, gErrorCount);
	printf("%s: thread count: %" PRId32 " (expected %" PRIu32 ")\n", __func__, p.threadCount,
	       dwThreads);
	printf("%s: true count:   %" PRId32 " (expected %" PRIu32 ")\n", __func__, p.trueCount,
	       expectedTrueCount);
	printf("%s: false count:  %" PRId32 " (expected %" PRIu32 ")\n", __func__, p.falseCount,
	       expectedFalseCount);

	rc = TRUE;
fail:
	free((void*)threads);
	DeleteSynchronizationBarrier(&gBarrier);
	if (gErrorCount > 0)
	{
		printf("%s: Error test failed with %" PRId32 " reported errors\n", __func__, gErrorCount);
		return FALSE;
	}

	return rc;
}

int TestSynchBarrier(int argc, char* argv[])
{
	SYSTEM_INFO sysinfo;
	DWORD dwMaxThreads = 0;
	DWORD dwMinThreads = 0;
	DWORD dwNumLoops = 10;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	GetNativeSystemInfo(&sysinfo);
	printf("%s: Number of processors: %" PRIu32 "\n", __func__, sysinfo.dwNumberOfProcessors);
	dwMinThreads = sysinfo.dwNumberOfProcessors;
	dwMaxThreads = sysinfo.dwNumberOfProcessors * 4;

	if (dwMaxThreads > 32)
		dwMaxThreads = 32;

	/* Test invalid parameters */
	if (InitializeSynchronizationBarrier(&gBarrier, 0, -1))
	{
		(void)fprintf(
		    stderr,
		    "%s: InitializeSynchronizationBarrier unecpectedly succeeded with lTotalThreads = 0\n",
		    __func__);
		return -1;
	}

	if (InitializeSynchronizationBarrier(&gBarrier, -1, -1))
	{
		(void)fprintf(
		    stderr,
		    "%s: InitializeSynchronizationBarrier unecpectedly succeeded with lTotalThreads = -1\n",
		    __func__);
		return -1;
	}

	if (InitializeSynchronizationBarrier(&gBarrier, 1, -2))
	{
		(void)fprintf(
		    stderr,
		    "%s: InitializeSynchronizationBarrier unecpectedly succeeded with lSpinCount = -2\n",
		    __func__);
		return -1;
	}

	/* Functional tests */

	if (!TestSynchBarrierWithFlags(0, dwMaxThreads, dwNumLoops))
	{
		(void)fprintf(
		    stderr,
		    "%s: TestSynchBarrierWithFlags(0) unecpectedly succeeded with lTotalThreads = -1\n",
		    __func__);
		return -1;
	}

	if (!TestSynchBarrierWithFlags(SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY, dwMinThreads,
	                               dwNumLoops))
	{
		(void)fprintf(stderr,
		              "%s: TestSynchBarrierWithFlags(SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY) "
		              "unecpectedly succeeded with lTotalThreads = -1\n",
		              __func__);
		return -1;
	}

	if (!TestSynchBarrierWithFlags(SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY, dwMaxThreads,
	                               dwNumLoops))
	{
		(void)fprintf(stderr,
		              "%s: TestSynchBarrierWithFlags(SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY) "
		              "unecpectedly succeeded with lTotalThreads = -1\n",
		              __func__);
		return -1;
	}

	printf("%s: Test successfully completed\n", __func__);
	return 0;
}
