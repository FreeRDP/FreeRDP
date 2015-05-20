
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
	int status = -1;
	HANDLE hTimer = NULL;
	BOOL bSuccess;
	LARGE_INTEGER due;
	APC_DATA* apcData = NULL;

	apcData = (APC_DATA*) malloc(sizeof(APC_DATA));
	if (!apcData)
	{
		printf("Memory allocation failed\n");
		goto cleanup;
	}

	g_Event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!g_Event)
	{
		printf("Failed to create event\n");
		goto cleanup;
	}

	hTimer = CreateWaitableTimer(NULL, FALSE, NULL);

	if (!hTimer)
		goto cleanup;

	due.QuadPart = -15000000LL; /* 1.5 seconds */

	apcData->StartTime = GetTickCount();
	bSuccess = SetWaitableTimer(hTimer, &due, 2000, TimerAPCProc, apcData, FALSE);

	if (!bSuccess)
		goto cleanup;

	if (WaitForSingleObject(g_Event, INFINITE) != WAIT_OBJECT_0)
	{
		printf("WaitForSingleObject failed (%d)\n", GetLastError());
		goto cleanup;
	}

	status = 0;

cleanup:
	if (hTimer)
		CloseHandle(hTimer);
	if (g_Event)
		CloseHandle(g_Event);
	free(apcData);

	return status;
}

