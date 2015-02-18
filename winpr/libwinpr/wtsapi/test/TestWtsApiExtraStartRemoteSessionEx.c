
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/input.h>
#include <winpr/environment.h>

int TestWtsApiExtraStartRemoteSessionEx(int argc, char* argv[])
{
	BOOL bSuccess;
	ULONG logonId = 0;
	char logonIdStr[10];

	bSuccess = GetEnvironmentVariable("TEST_SESSION_LOGON_ID", logonIdStr, 10);
	if(bSuccess)
	{
		sscanf(logonIdStr, "%u\n", &logonId);
	}

	bSuccess = WTSStartRemoteControlSessionEx(NULL, logonId, VK_F10, REMOTECONTROL_KBDSHIFT_HOTKEY|REMOTECONTROL_KBDCTRL_HOTKEY, REMOTECONTROL_FLAG_DISABLE_INPUT);

	if (!bSuccess)
	{
		printf("WTSStartRemoteControlSessionEx failed: %d\n", (int) GetLastError());
		return -1;
	}

	return 0;
}
