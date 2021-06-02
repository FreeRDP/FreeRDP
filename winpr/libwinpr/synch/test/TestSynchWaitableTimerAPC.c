
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

static VOID CALLBACK TimerAPCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	APC_DATA* apcData;
	UINT32 CurrentTime = GetTickCount();
	WINPR_UNUSED(dwTimerLowValue);
	WINPR_UNUSED(dwTimerHighValue);

	if (!lpArg)
		return;

	apcData = (APC_DATA*)lpArg;
	printf("TimerAPCProc: time: %" PRIu32 "\n", CurrentTime - apcData->StartTime);
	g_Count++;

	if (g_Count >= 5)
	{
		SetEvent(g_Event);
	}
}

int TestSynchWaitableTimerAPC(int argc, char* argv[])
{
	int status = -1;
	DWORD rc;
	HANDLE hTimer = NULL;
	BOOL bSuccess;
	LARGE_INTEGER due;
	APC_DATA apcData = { 0 };
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	g_Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!g_Event)
	{
		printf("Failed to create event\n");
		goto cleanup;
	}

	hTimer = CreateWaitableTimer(NULL, FALSE, NULL);
	if (!hTimer)
		goto cleanup;

	due.QuadPart = -1000 * 100LL; /* 0.1 seconds */
	apcData.StartTime = GetTickCount();
	bSuccess = SetWaitableTimer(hTimer, &due, 10, TimerAPCProc, &apcData, FALSE);

	if (!bSuccess)
		goto cleanup;

	/* nothing shall happen after 0.12 second, because thread is not in alertable state */
	rc = WaitForSingleObject(g_Event, 120);
	if (rc != WAIT_TIMEOUT)
		goto cleanup;

	for (;;)
	{
		rc = WaitForSingleObjectEx(g_Event, INFINITE, TRUE);
		if (rc == WAIT_OBJECT_0)
			break;

		if (rc == WAIT_IO_COMPLETION)
			continue;

		printf("Failed to wait for completion event (%" PRIu32 ")\n", GetLastError());
		goto cleanup;
	}

	status = 0;
cleanup:

	if (hTimer)
		CloseHandle(hTimer);

	if (g_Event)
		CloseHandle(g_Event);

	return status;
}
