
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiQuerySessionInformation(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;
	LPSTR pBuffer;
	DWORD sessionId;
	DWORD bytesReturned;

	sessionId = 123;
	hServer = WTS_CURRENT_SERVER_HANDLE;

	pBuffer = NULL;
	bytesReturned = 0;

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSConnectState, &pBuffer, &bytesReturned);

	if (!bSuccess)
	{
		printf("WTSQuerySessionInformation failed: %d\n", (int) GetLastError());
		//return -1;
	}

	return 0;
}
