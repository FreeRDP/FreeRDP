
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/environment.h>

int TestWtsApiSessionNotification(int argc, char* argv[])
{
	HWND hWnd = nullptr;
	BOOL bSuccess = 0;
	DWORD dwFlags = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#ifndef _WIN32
	if (!GetEnvironmentVariableA("WTSAPI_LIBRARY", nullptr, 0))
	{
		printf("%s: No RDS environment detected, skipping test\n", __func__);
		return 0;
	}
#else
	/* We create a message-only window and use the predefined class name "STATIC" for simplicity */
	hWnd = CreateWindowA("STATIC", "TestWtsApiSessionNotification", 0, 0, 0, 0, 0, HWND_MESSAGE,
	                     nullptr, nullptr, nullptr);
	if (!hWnd)
	{
		printf("%s: error creating message-only window: %" PRIu32 "\n", __func__, GetLastError());
		return -1;
	}
#endif

	dwFlags = NOTIFY_FOR_ALL_SESSIONS;

	bSuccess = WTSRegisterSessionNotification(hWnd, dwFlags);

	if (!bSuccess)
	{
		printf("%s: WTSRegisterSessionNotification failed: %" PRIu32 "\n", __func__,
		       GetLastError());
		return -1;
	}

	bSuccess = WTSUnRegisterSessionNotification(hWnd);

#ifdef _WIN32
	if (hWnd)
	{
		DestroyWindow(hWnd);
		hWnd = nullptr;
	}
#endif

	if (!bSuccess)
	{
		printf("%s: WTSUnRegisterSessionNotification failed: %" PRIu32 "\n", __func__,
		       GetLastError());
		return -1;
	}

	return 0;
}
