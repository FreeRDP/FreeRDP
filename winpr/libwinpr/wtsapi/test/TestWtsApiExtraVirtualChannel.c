
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiExtraVirtualChannel(int argc, char* argv[])
{
	BOOL bSuccess;
	ULONG length;
	ULONG bytesRead;
	ULONG bytesWritten;
	BYTE buffer[1024];
	HANDLE hVirtualChannel;

	length = sizeof(buffer);

	hVirtualChannel = WTSVirtualChannelOpen(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, "sample");

	if (hVirtualChannel == INVALID_HANDLE_VALUE)
	{
		printf("WTSVirtualChannelOpen failed: %d\n", (int) GetLastError());
		return -1;
	}
	printf("WTSVirtualChannelOpen opend");
	bytesWritten = 0;
	bSuccess = WTSVirtualChannelWrite(hVirtualChannel, (PCHAR) buffer, length, &bytesWritten);

	if (!bSuccess)
	{
		printf("WTSVirtualChannelWrite failed: %d\n", (int) GetLastError());
		return -1;
	}
	printf("WTSVirtualChannelWrite written");

	bytesRead = 0;
	bSuccess = WTSVirtualChannelRead(hVirtualChannel, 5000, (PCHAR) buffer, length, &bytesRead);

	if (!bSuccess)
	{
		printf("WTSVirtualChannelRead failed: %d\n", (int) GetLastError());
		return -1;
	}
	printf("WTSVirtualChannelRead read");

	if (!WTSVirtualChannelClose(hVirtualChannel))
	{
		printf("WTSVirtualChannelClose failed\n");
		return -1;
	}

	return 0;
}

