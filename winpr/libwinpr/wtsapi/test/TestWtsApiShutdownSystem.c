
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiShutdownSystem(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;
	DWORD ShutdownFlag;

	hServer = WTS_CURRENT_SERVER_HANDLE;
	ShutdownFlag = WTS_WSD_SHUTDOWN;

	bSuccess = WTSShutdownSystem(hServer, ShutdownFlag);

	if (!bSuccess)
	{
		printf("WTSShutdownSystem failed: %d\n", (int) GetLastError());
		//return -1;
	}

	return 0;
}
