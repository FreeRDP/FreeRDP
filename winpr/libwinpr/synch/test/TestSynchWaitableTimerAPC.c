
#include <winpr/crt.h>
#include <winpr/synch.h>

HANDLE gDoneEvent;

VOID CALLBACK TimerAPCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	int param;

	if (!lpArg)
		return;

	param = *((int*) lpArg);

	printf("TimerAPCProc: %d\n", param);

	SetEvent(gDoneEvent);
}

int TestSynchWaitableTimerAPC(int argc, char* argv[])
{
	int arg = 123;
	HANDLE hTimer;
	BOOL bSuccess;
	INT64 qwDueTime;
	LARGE_INTEGER liDueTime;

	hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	if (!hTimer)
		return -1;

	qwDueTime = -5 * 10000000;
	liDueTime.LowPart = (DWORD) (qwDueTime & 0xFFFFFFFF);
	liDueTime.HighPart = (LONG) (qwDueTime >> 32);

	bSuccess = SetWaitableTimer(hTimer, &liDueTime, 2000, TimerAPCProc, &arg, FALSE);

	if (!bSuccess)
		return -1;

	if (WaitForSingleObject(gDoneEvent, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failed (%d)\n", GetLastError());
		return -1;
	}

	CloseHandle(gDoneEvent);

	CloseHandle(hTimer);

	return 0;
}

