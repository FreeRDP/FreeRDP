
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiExtraDisconnectSession(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	HANDLE hServer = WTS_CURRENT_SERVER_HANDLE;
	BOOL bSuccess = WTSDisconnectSession(hServer, WTS_CURRENT_SESSION, FALSE);

	if (!bSuccess)
	{
		printf("WTSDisconnectSession failed: %" PRIu32 "\n", GetLastError());
		return -1;
	}

	return 0;
}
