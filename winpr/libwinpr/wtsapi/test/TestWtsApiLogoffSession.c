
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiLogoffSession(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;
	DWORD sessionId;

	sessionId = 123;
	hServer = WTS_CURRENT_SERVER_HANDLE;

	bSuccess = WTSLogoffSession(hServer, sessionId, FALSE);

	if (!bSuccess)
	{
		printf("WTSLogoffSession failed: %d\n", (int) GetLastError());
		//return -1;
	}

	return 0;
}
