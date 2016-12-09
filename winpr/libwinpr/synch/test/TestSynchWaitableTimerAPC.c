
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

	/**
	 * See Remarks at https://msdn.microsoft.com/en-us/library/windows/desktop/ms686786(v=vs.85).aspx
	 * The SetWaitableTimer completion routine is executed by the thread that activates the timer
	 * using SetWaitableTimer. However, the thread must be in an ALERTABLE state.
	 */


	/**
	 * Note: On WIN32 we need to use WaitForSingleObjectEx with parameter bAlertable = TRUE
	 * However, WinPR currently (May 2016) does not have a working WaitForSingleObjectEx implementation
	 * but its non-WIN32 WaitForSingleObject implementations seem to be alertable by WinPR's
	 * timer implementations.
	 **/

	for(;;)
	{
		DWORD rc;
#ifdef _WIN32
		rc = WaitForSingleObjectEx(g_Event, INFINITE, TRUE);
#else
		rc = WaitForSingleObject(g_Event, INFINITE);
#endif
		if (rc == WAIT_OBJECT_0)
			break;

		if (rc == 0x000000C0L) /* WAIT_IO_COMPLETION */
			continue;

		printf("Failed to wait for completion event (%u)\n", GetLastError());
		goto cleanup;
	}

	status = 0;

cleanup:
	if (hTimer)
		CloseHandle(hTimer);
	if (g_Event)
		CloseHandle(g_Event);
	free(apcData);

#ifdef __APPLE__
	if (status == 0)
	{
		printf("%s: Error, this test is currently expected not to succeed on this platform.\n",
			__FUNCTION__);
		status = -1;
	}
	else
	{
		printf("%s: This test is currently expected to fail on this platform.\n",
			__FUNCTION__);
		status = 0;
	}
#endif
	return status;
}

