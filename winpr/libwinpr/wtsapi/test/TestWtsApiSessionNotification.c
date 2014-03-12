
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiSessionNotification(int argc, char* argv[])
{
	HWND hWnd;
	BOOL bSuccess;
	DWORD dwFlags;

	hWnd = NULL;
	dwFlags = NOTIFY_FOR_ALL_SESSIONS;

	bSuccess = WTSRegisterSessionNotification(hWnd, dwFlags);

	if (!bSuccess)
	{
		printf("WTSRegisterSessionNotification failed: %d\n", (int) GetLastError());
		//return -1;
	}

	bSuccess = WTSUnRegisterSessionNotification(hWnd);

	if (!bSuccess)
	{
		printf("WTSUnRegisterSessionNotification failed: %d\n", (int) GetLastError());
		//return -1;
	}

	return 0;
}
