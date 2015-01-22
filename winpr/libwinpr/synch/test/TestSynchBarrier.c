
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

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

int TestSynchBarrier(int argc, char* argv[])
{
	int index;
	HANDLE threads[5];

	g_Count = 0;

	g_Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	InitializeCriticalSectionAndSpinCount(&g_Lock, 4000);

	if (!InitializeSynchronizationBarrier(&g_Barrier, 5, -1))
		return -1;

	for (index = 0; index < 5; index++)
	{
		threads[index] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
				test_synch_barrier_thread_func, NULL, 0, NULL);
	}

	WaitForSingleObject(g_Event, INFINITE);

	if (g_Count != 5)
		return -1;

	printf("All threads have reached the barrier\n");

	for (index = 0; index < 5; index++)
	{
		CloseHandle(threads[index]);
	}

	DeleteSynchronizationBarrier(&g_Barrier);

	DeleteCriticalSection(&g_Lock);

	CloseHandle(g_Event);

	return 0;
}

