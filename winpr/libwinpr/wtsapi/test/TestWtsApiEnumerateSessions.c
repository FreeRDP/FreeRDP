
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/environment.h>

int TestWtsApiEnumerateSessions(int argc, char* argv[])
{
	DWORD index;
	DWORD count;
	BOOL bSuccess;
	HANDLE hServer;
	PWTS_SESSION_INFOA pSessionInfo;

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
	pSessionInfo = NULL;

	bSuccess = WTSEnumerateSessionsA(hServer, 0, 1, &pSessionInfo, &count);

	if (!bSuccess)
	{
		printf("WTSEnumerateSessions failed: %" PRIu32 "\n", GetLastError());
		return 0;
	}

	printf("WTSEnumerateSessions count: %" PRIu32 "\n", count);

	for (index = 0; index < count; index++)
	{
		printf("[%" PRIu32 "] SessionId: %" PRIu32 " WinstationName: '%s' State: %s (%u)\n", index,
		       pSessionInfo[index].SessionId, pSessionInfo[index].pWinStationName,
		       WTSSessionStateToString(pSessionInfo[index].State), pSessionInfo[index].State);
	}

	WTSFreeMemory(pSessionInfo);

	return 0;
}
