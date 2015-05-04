
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

#include <winpr/synch.h>

#define FIRE_COUNT	5
#define TIMER_COUNT	5

static int g_Count = 0;
static HANDLE g_Event = NULL;

struct apc_data
{
	int TimerId;
	int FireCount;
	DWORD DueTime;
	DWORD Period;
	UINT32 StartTime;
};
typedef struct apc_data APC_DATA;

VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	UINT32 TimerTime;
	APC_DATA* apcData;
	UINT32 expectedTime;
	UINT32 CurrentTime = GetTickCount();

	if (!lpParam)
		return;

	apcData = (APC_DATA*) lpParam;

	TimerTime = CurrentTime - apcData->StartTime;
	expectedTime = apcData->DueTime + (apcData->Period * apcData->FireCount);

	apcData->FireCount++;
	g_Count++;

	printf("TimerRoutine: TimerId: %d FireCount: %d ActualTime: %d ExpectedTime: %d Discrepancy: %d\n",
			apcData->TimerId, apcData->FireCount, TimerTime, expectedTime, TimerTime - expectedTime);

	if (g_Count >= (TIMER_COUNT * FIRE_COUNT))
	{
		SetEvent(g_Event);
	}

	Sleep(50);
}

int TestSynchTimerQueue(int argc, char* argv[])
{
	int index;
	HANDLE hTimerQueue;
	HANDLE hTimers[TIMER_COUNT];
	APC_DATA apcData[TIMER_COUNT];

	g_Event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!g_Event)
	{
		printf("CreateEvent failed (%d)\n", (int) GetLastError());
		return -1;
	}

	hTimerQueue = CreateTimerQueue();

	if (!hTimerQueue)
	{
		printf("CreateTimerQueue failed (%d)\n", (int) GetLastError());
		return -1;
	}

	for (index = 0; index < TIMER_COUNT; index++)
	{
		apcData[index].TimerId = index;
		apcData[index].StartTime = GetTickCount();
		apcData[index].DueTime = (index * 100) + 500;
		apcData[index].Period = 1000;
		apcData[index].FireCount = 0;

		if (!CreateTimerQueueTimer(&hTimers[index], hTimerQueue, (WAITORTIMERCALLBACK) TimerRoutine,
				&apcData[index], apcData[index].DueTime, apcData[index].Period, 0))
		{
			printf("CreateTimerQueueTimer failed (%d)\n", (int) GetLastError());
			return -1;
		}
	}

	if (WaitForSingleObject(g_Event, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failed (%d)\n", (int) GetLastError());
		return -1;
	}

	for (index = 0; index < TIMER_COUNT; index++)
	{
		if (!DeleteTimerQueueTimer(hTimerQueue, hTimers[index], NULL))
		{
			printf("DeleteTimerQueueTimer failed (%d)\n", (int) GetLastError());
			return -1;
		}
	}
	
	if (!DeleteTimerQueue(hTimerQueue))
	{
		printf("DeleteTimerQueue failed (%d)\n", (int) GetLastError());
		return -1;
	}

	CloseHandle(g_Event);

	return 0;
}
