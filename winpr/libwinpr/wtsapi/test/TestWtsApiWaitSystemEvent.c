
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiWaitSystemEvent(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;
	DWORD eventMask;
	DWORD eventFlags;

	hServer = WTS_CURRENT_SERVER_HANDLE;

	eventMask = WTS_EVENT_ALL;
	eventFlags = 0;

	bSuccess = WTSWaitSystemEvent(hServer, eventMask, &eventFlags);

	if (!bSuccess)
	{
		printf("WTSWaitSystemEvent failed: %d\n", (int) GetLastError());
		//return -1;
	}

	return 0;
}
