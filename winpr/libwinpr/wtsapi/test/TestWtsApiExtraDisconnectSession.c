
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiExtraDisconnectSession(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;

	hServer = WTS_CURRENT_SERVER_HANDLE;

	bSuccess = WTSDisconnectSession(hServer, WTS_CURRENT_SESSION, FALSE);

	if (!bSuccess)
	{
		printf("WTSDisconnectSession failed: %d\n", (int) GetLastError());
		return -1;
	}

	return 0;
}
