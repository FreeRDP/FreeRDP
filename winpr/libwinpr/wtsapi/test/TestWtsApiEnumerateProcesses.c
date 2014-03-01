
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiEnumerateProcesses(int argc, char* argv[])
{
	DWORD count;
	BOOL bSuccess;
	HANDLE hServer;
	PWTS_PROCESS_INFO pProcessInfo;

	hServer = WTS_CURRENT_SERVER_HANDLE;

	count = 0;
	pProcessInfo = NULL;

	bSuccess = WTSEnumerateProcesses(hServer, 0, 1, &pProcessInfo, &count);

	if (!bSuccess)
	{
		printf("WTSEnumerateProcesses failed: %d\n", (int) GetLastError());
		//return -1;
	}

	WTSFreeMemory(pProcessInfo);

	return 0;
}
