
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/environment.h>

int TestWtsApiWaitSystemEvent(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;
	DWORD eventMask;
	DWORD eventFlags;

#ifndef _WIN32
	if (!GetEnvironmentVariableA("WTSAPI_LIBRARY", NULL, 0))
	{
		printf("%s: No RDS environment detected, skipping test\n", __FUNCTION__);
		return 0;
	}
#endif

	hServer = WTS_CURRENT_SERVER_HANDLE;

	eventMask = WTS_EVENT_ALL;
	eventFlags = 0;

	bSuccess = WTSWaitSystemEvent(hServer, eventMask, &eventFlags);

	if (!bSuccess)
	{
		printf("WTSWaitSystemEvent failed: %d\n", (int) GetLastError());
		return -1;
	}

	return 0;
}
