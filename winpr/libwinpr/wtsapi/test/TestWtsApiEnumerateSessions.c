
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiEnumerateSessions(int argc, char* argv[])
{
	DWORD index;
	DWORD count;
	BOOL bSuccess;
	HANDLE hServer;
	PWTS_SESSION_INFO pSessionInfo;

	hServer = WTS_CURRENT_SERVER_HANDLE;

	count = 0;
	pSessionInfo = NULL;

	bSuccess = WTSEnumerateSessions(hServer, 0, 1, &pSessionInfo, &count);

	if (!bSuccess)
	{
		printf("WTSEnumerateSessions failed: %d\n", (int) GetLastError());
		return 0;
	}

	printf("WTSEnumerateSessions count: %d\n", (int) count);

	for (index = 0; index < count; index++)
	{
		printf("[%d] SessionId: %d State: %d\n", (int) index,
				(int) pSessionInfo[index].SessionId,
				(int) pSessionInfo[index].State);
	}

	WTSFreeMemory(pSessionInfo);

	return 0;
}
