
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiExtraLogoffSession(int argc, char* argv[])
{

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	HANDLE hServer = WTS_CURRENT_SERVER_HANDLE;
	BOOL bSuccess = WTSLogoffSession(hServer, WTS_CURRENT_SESSION, FALSE);

	if (!bSuccess)
	{
		printf("WTSLogoffSession failed: %" PRIu32 "\n", GetLastError());
		return -1;
	}

	return 0;
}
