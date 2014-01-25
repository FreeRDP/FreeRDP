
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

static int g_Count = 0;
static HANDLE g_Event = NULL;

struct apc_data
{
	UINT32 StartTime;
};
typedef struct apc_data APC_DATA;

VOID CALLBACK TimerAPCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	APC_DATA* apcData;
	UINT32 CurrentTime = GetTickCount();

	if (!lpArg)
		return;

	apcData = (APC_DATA*) lpArg;

	printf("TimerAPCProc: time: %d\n", CurrentTime - apcData->StartTime);

	g_Count++;

	if (g_Count >= 5)
	{
		SetEvent(g_Event);
	}
}

int TestSynchWaitableTimerAPC(int argc, char* argv[])
{
	HANDLE hTimer;
	BOOL bSuccess;
	LARGE_INTEGER due;
	APC_DATA* apcData;

	apcData = (APC_DATA*) malloc(sizeof(APC_DATA));
	g_Event = CreateEvent(NULL, TRUE, FALSE, NULL);
	hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	if (!hTimer)
		return -1;

	due.QuadPart = -15000000LL; /* 1.5 seconds */

	apcData->StartTime = GetTickCount();
	bSuccess = SetWaitableTimer(hTimer, &due, 2000, TimerAPCProc, apcData, FALSE);

	if (!bSuccess)
		return -1;

	if (WaitForSingleObject(g_Event, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failed (%d)\n", GetLastError());
		return -1;
	}

	CloseHandle(hTimer);
	CloseHandle(g_Event);
	free(apcData);

	return 0;
}

