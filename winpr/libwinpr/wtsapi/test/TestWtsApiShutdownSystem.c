#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/environment.h>

int TestWtsApiShutdownSystem(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;
	DWORD ShutdownFlag;

#ifndef _WIN32
	if (!GetEnvironmentVariableA("WTSAPI_LIBRARY", NULL, 0))
	{
		printf("%s: No RDS environment detected, skipping test\n", __FUNCTION__);
		return 0;
	}
#endif

	hServer = WTS_CURRENT_SERVER_HANDLE;
	ShutdownFlag = WTS_WSD_SHUTDOWN;

	bSuccess = WTSShutdownSystem(hServer, ShutdownFlag);

	if (!bSuccess)
	{
		printf("WTSShutdownSystem failed: %d\n", (int) GetLastError());
		return -1;
	}

	return 0;
}
