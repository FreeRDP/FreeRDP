
#include <winpr/crt.h>
#include <winpr/synch.h>

HANDLE gDoneEvent;

VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	int param;

	if (!lpParam)
		return;

	param = *((int*) lpParam);

	printf("TimerRoutine: Param: %d TimerOrWaitFired: %d\n", param, TimerOrWaitFired);

	SetEvent(gDoneEvent);
}

int TestSynchTimerQueue(int argc, char* argv[])
{
	int arg = 123;
	HANDLE hTimer = NULL;
	HANDLE hTimerQueue = NULL;

	gDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	hTimerQueue = CreateTimerQueue();

	if (!hTimerQueue)
	{
		printf("CreateTimerQueue failed (%d)\n", GetLastError());
		return -1;
	}

	if (!CreateTimerQueueTimer(&hTimer, hTimerQueue, (WAITORTIMERCALLBACK) TimerRoutine, &arg , 1000, 0, 0))
	{
		printf("CreateTimerQueueTimer failed (%d)\n", GetLastError());
		return -1;
	}

	if (WaitForSingleObject(gDoneEvent, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failed (%d)\n", GetLastError());
		return -1;
	}

	CloseHandle(gDoneEvent);

	if (!DeleteTimerQueue(hTimerQueue))
	{
		printf("DeleteTimerQueue failed (%d)\n", GetLastError());
		return -1;
	}

	return 0;
}
