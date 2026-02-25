
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/input.h>
#include <winpr/environment.h>

int TestWtsApiExtraStartRemoteSessionEx(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	ULONG logonId = 0;
	char logonIdStr[10] = WINPR_C_ARRAY_INIT;

	DWORD bSuccess = GetEnvironmentVariableA("TEST_SESSION_LOGON_ID", logonIdStr, 10);
	if (bSuccess > 0)
	{
		errno = 0;
		logonId = strtoul(logonIdStr, NULL, 0);
		if (errno != 0)
			return -1;
	}

	bSuccess = WTSStartRemoteControlSessionEx(
	    NULL, logonId, VK_F10, REMOTECONTROL_KBDSHIFT_HOTKEY | REMOTECONTROL_KBDCTRL_HOTKEY,
	    REMOTECONTROL_FLAG_DISABLE_INPUT);

	if (!bSuccess)
	{
		printf("WTSStartRemoteControlSessionEx failed: %" PRIu32 "\n", GetLastError());
		return -1;
	}

	return 0;
}
