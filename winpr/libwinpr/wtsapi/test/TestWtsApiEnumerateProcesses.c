
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/environment.h>

int TestWtsApiEnumerateProcesses(int argc, char* argv[])
{
	DWORD count;
	BOOL bSuccess;
	HANDLE hServer;
	PWTS_PROCESS_INFOA pProcessInfo;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#ifndef _WIN32
	if (!GetEnvironmentVariableA("WTSAPI_LIBRARY", NULL, 0))
	{
		printf("%s: No RDS environment detected, skipping test\n", __FUNCTION__);
		return 0;
	}
#endif

	hServer = WTS_CURRENT_SERVER_HANDLE;

	count = 0;
	pProcessInfo = NULL;

	bSuccess = WTSEnumerateProcessesA(hServer, 0, 1, &pProcessInfo, &count);

	if (!bSuccess)
	{
		printf("WTSEnumerateProcesses failed: %" PRIu32 "\n", GetLastError());
		return -1;
	}

#if 0
	{
		DWORD i;
		printf("WTSEnumerateProcesses enumerated %"PRIu32" processs:\n", count);
		for (i = 0; i < count; i++)
			printf("\t[%"PRIu32"]: %s (%"PRIu32")\n", i, pProcessInfo[i].pProcessName, pProcessInfo[i].ProcessId);
	}
#endif

	WTSFreeMemory(pProcessInfo);

	return 0;
}
