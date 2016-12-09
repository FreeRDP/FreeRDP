
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiExtraLogoffSession(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;
	DWORD sessionId;

	sessionId = 123;
	hServer = WTS_CURRENT_SERVER_HANDLE;

	bSuccess = WTSLogoffSession(hServer, WTS_CURRENT_SESSION, FALSE);

	if (!bSuccess)
	{
		printf("WTSLogoffSession failed: %d\n", (int) GetLastError());
		return -1;
	}

	return 0;
}
