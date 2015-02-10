
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>
#include <winpr/user.h>

#define TITLE "thats the title"
#define MESSAGE "thats the message"

int TestWtsApiExtraSendMessage(int argc, char* argv[])
{
	BOOL bSuccess;
	HANDLE hServer;
	DWORD result;

	hServer = WTS_CURRENT_SERVER_HANDLE;

	bSuccess = WTSSendMessage(hServer, WTS_CURRENT_SESSION,TITLE,strlen(TITLE) + 1, MESSAGE, strlen(MESSAGE) + 1, MB_CANCELTRYCONTINUE, 3 , &result,TRUE);

	if (!bSuccess)
	{
		printf("WTSSendMessage failed: %d\n", (int) GetLastError());
		return -1;
	}

	printf("WTSSendMessage got result: %d\n", (int) result);

	return 0;
}
