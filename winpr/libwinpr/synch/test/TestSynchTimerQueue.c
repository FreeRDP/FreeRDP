
#include <winpr/crt.h>
#include <winpr/sysinfo.h>
#include <winpr/file.h>
#include <winpr/synch.h>

#define FIRE_COUNT 5
#define TIMER_COUNT 5

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

static VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	UINT32 TimerTime;
	APC_DATA* apcData;
	UINT32 expectedTime;
	UINT32 CurrentTime = GetTickCount();

	WINPR_UNUSED(TimerOrWaitFired);

	if (!lpParam)
		return;

	apcData = (APC_DATA*)lpParam;

	TimerTime = CurrentTime - apcData->StartTime;
	expectedTime = apcData->DueTime + (apcData->Period * apcData->FireCount);

	apcData->FireCount++;

	printf("TimerRoutine: TimerId: %" PRIu32 " FireCount: %" PRIu32 " ActualTime: %" PRIu32
	       " ExpectedTime: %" PRIu32 " Discrepancy: %" PRIu32 "\n",
	       apcData->TimerId, apcData->FireCount, TimerTime, expectedTime, TimerTime - expectedTime);

	Sleep(11);

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

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	hTimerQueue = CreateTimerQueue();

	if (!hTimerQueue)
	{
		printf("CreateTimerQueue failed (%" PRIu32 ")\n", GetLastError());
		return -1;
	}

	for (index = 0; index < TIMER_COUNT; index++)
	{
		apcData[index].TimerId = index;
		apcData[index].StartTime = GetTickCount();
		apcData[index].DueTime = (index * 10) + 50;
		apcData[index].Period = 100;
		apcData[index].FireCount = 0;
		apcData[index].MaxFireCount = FIRE_COUNT;

		if (!(apcData[index].CompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			printf("Failed to create apcData[%" PRIu32 "] event (%" PRIu32 ")\n", index,
			       GetLastError());
			return -1;
		}

		if (!CreateTimerQueueTimer(&hTimers[index], hTimerQueue, TimerRoutine, &apcData[index],
		                           apcData[index].DueTime, apcData[index].Period, 0))
		{
			printf("CreateTimerQueueTimer failed (%" PRIu32 ")\n", GetLastError());
			return -1;
		}
	}

	for (index = 0; index < TIMER_COUNT; index++)
	{
		if (WaitForSingleObject(apcData[index].CompletionEvent, 2000) != WAIT_OBJECT_0)
		{
			printf("Failed to wait for timer queue timer #%" PRIu32 " (%" PRIu32 ")\n", index,
			       GetLastError());
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
			printf("DeleteTimerQueueTimer failed (%" PRIu32 ")\n", GetLastError());
			return -1;
		}
		CloseHandle(apcData[index].CompletionEvent);
	}

	if (!DeleteTimerQueue(hTimerQueue))
	{
		printf("DeleteTimerQueue failed (%" PRIu32 ")\n", GetLastError());
		return -1;
	}

	return 0;
}
