
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include "../synch.h"

static int g_Count;
static HANDLE g_Event;
static CRITICAL_SECTION g_Lock;
static SYNCHRONIZATION_BARRIER g_Barrier;

static void* test_synch_barrier_thread_func(void* arg)
{
	BOOL status;
  int count;

	EnterCriticalSection(&g_Lock);
  count = g_Count++;
	LeaveCriticalSection(&g_Lock);
	status = EnterSynchronizationBarrier(&g_Barrier, 0);

	printf("Thread #%d status: %s\n", count,
			status ? "TRUE" : "FALSE");

	if (status)
	{
		SetEvent(g_Event);
	}

	return NULL;
}

static void* barrier_deleter_thread_func(void* arg)
{
	/* Blocks until all threads are released from the barrier. */
	DeleteSynchronizationBarrier(&g_Barrier);

	return NULL;
}

int TestSynchBarrier(int argc, char* argv[])
{
	int index;
	HANDLE threads[5];
	HANDLE deleter_thread = NULL;

	g_Count = 0;

	if (!(g_Event = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("%s: CreateEvent failed. GetLastError() = 0x%08x", __FUNCTION__, GetLastError());
		return -1;
	}

	if (!InitializeCriticalSectionAndSpinCount(&g_Lock, 4000))
	{
		printf("%s: InitializeCriticalSectionAndSpinCount failed. GetLastError() = 0x%08x", __FUNCTION__, GetLastError());
		CloseHandle(g_Event);
		return -1;
	}

	if (!InitializeSynchronizationBarrier(&g_Barrier, 5, -1))
	{
		printf("%s: InitializeSynchronizationBarrier failed. GetLastError() = 0x%08x", __FUNCTION__, GetLastError());
		DeleteCriticalSection(&g_Lock);
		CloseHandle(g_Event);
		return -1;
	}

	for (index = 0; index < 5; index++)
	{
		if (!(threads[index] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
				test_synch_barrier_thread_func, NULL, 0, NULL)))
		{
			printf("%s: CreateThread failed for thread #%d. GetLastError() = 0x%08x\n", __FUNCTION__, index, GetLastError());
			while (index)
				CloseHandle(threads[--index]);
			CloseHandle(deleter_thread);
			DeleteCriticalSection(&g_Lock);
			CloseHandle(g_Event);
			return -1;
		}

		if (index == 0)
		{
			/* Make sure first thread has already entered the barrier... */
			while (((WINPR_BARRIER*) g_Barrier.Reserved3[0])->count == 0)
				Sleep(100);

			/* Now spawn the deleter thread. */
			if (!(deleter_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
					barrier_deleter_thread_func, NULL, 0, NULL)))
			{
				printf("%s: CreateThread failed for deleter thread. GetLastError() = 0x%08x\n", __FUNCTION__, GetLastError());
				while (index)
					CloseHandle(threads[--index]);
				DeleteCriticalSection(&g_Lock);
				CloseHandle(g_Event);
				return -1;
			}
		}
	}

	WaitForSingleObject(g_Event, INFINITE);

	if (g_Count != 5)
		return -1;

	printf("All threads have reached the barrier\n");

	for (index = 0; index < 5; index++)
	{
		CloseHandle(threads[index]);
	}

	CloseHandle(deleter_thread);

	DeleteCriticalSection(&g_Lock);

	CloseHandle(g_Event);

	return 0;
}

