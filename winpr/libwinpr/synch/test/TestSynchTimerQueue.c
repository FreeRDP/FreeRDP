
#include <winpr/crt.h>
#include <winpr/sysinfo.h>
#include <winpr/file.h>
#include <winpr/synch.h>

#define FIRE_COUNT	5
#define TIMER_COUNT	5

struct apc_data
{
	DWORD TimerId;
	DWORD FireCount;
	DWORD DueTime;
	DWORD Period;
	UINT32 StartTime;
	DWORD MaxFireCount;
	HANDLE CompletionEvent;
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

	printf("TimerRoutine: TimerId: %d FireCount: %d ActualTime: %d ExpectedTime: %d Discrepancy: %d\n",
			apcData->TimerId, apcData->FireCount, TimerTime, expectedTime, TimerTime - expectedTime);

	Sleep(50);

	if (apcData->FireCount == apcData->MaxFireCount)
	{
		SetEvent(apcData->CompletionEvent);
	}
}

int TestSynchTimerQueue(int argc, char* argv[])
{
	DWORD index;
	HANDLE hTimerQueue;
	HANDLE hTimers[TIMER_COUNT];
	APC_DATA apcData[TIMER_COUNT];

	hTimerQueue = CreateTimerQueue();

	if (!hTimerQueue)
	{
		printf("CreateTimerQueue failed (%u)\n", GetLastError());
		return -1;
	}

	for (index = 0; index < TIMER_COUNT; index++)
	{
		apcData[index].TimerId = index;
		apcData[index].StartTime = GetTickCount();
		apcData[index].DueTime = (index * 100) + 500;
		apcData[index].Period = 1000;
		apcData[index].FireCount = 0;
		apcData[index].MaxFireCount = FIRE_COUNT;

		if (!(apcData[index].CompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			printf("Failed to create apcData[%u] event (%u)\n", index, GetLastError());
			return -1;
		}

		if (!CreateTimerQueueTimer(&hTimers[index], hTimerQueue, (WAITORTIMERCALLBACK) TimerRoutine,
				&apcData[index], apcData[index].DueTime, apcData[index].Period, 0))
		{
			printf("CreateTimerQueueTimer failed (%u)\n", GetLastError());
			return -1;
		}
	}

	for (index = 0; index < TIMER_COUNT; index++)
	{
		if (WaitForSingleObject(apcData[index].CompletionEvent, 20000) != WAIT_OBJECT_0)
		{
			printf("Failed to wait for timer queue timer #%u (%u)\n", index, GetLastError());
			return -1;
		}
	}

	for (index = 0; index < TIMER_COUNT; index++)
	{
		/**
		 * Note: If the CompletionEvent parameter is INVALID_HANDLE_VALUE, the function waits
		 * for any running timer callback functions to complete before returning.
		 */
		if (!DeleteTimerQueueTimer(hTimerQueue, hTimers[index], INVALID_HANDLE_VALUE))
		{
			printf("DeleteTimerQueueTimer failed (%u)\n", GetLastError());
			return -1;
		}
		CloseHandle(apcData[index].CompletionEvent);
	}

	if (!DeleteTimerQueue(hTimerQueue))
	{
		printf("DeleteTimerQueue failed (%u)\n", GetLastError());
		return -1;
	}

	return 0;
}
