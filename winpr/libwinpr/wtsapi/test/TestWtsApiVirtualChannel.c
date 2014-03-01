
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiVirtualChannel(int argc, char* argv[])
{
	BOOL bSuccess;
	ULONG length;
	ULONG bytesRead;
	ULONG bytesWritten;
	BYTE buffer[1024];
	HANDLE hVirtualChannel;

	length = sizeof(buffer);

	hVirtualChannel = WTSVirtualChannelOpen(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, "RDPDBG");

	if (!hVirtualChannel)
	{
		printf("WTSVirtualChannelOpen failed: %d\n", (int) GetLastError());
		//return -1;
	}

	bytesWritten = 0;
	bSuccess = WTSVirtualChannelWrite(hVirtualChannel, (PCHAR) buffer, length, &bytesWritten);

	if (!bSuccess)
	{
		printf("WTSVirtualChannelWrite failed: %d\n", (int) GetLastError());
		//return -1;
	}

	bytesRead = 0;
	bSuccess = WTSVirtualChannelRead(hVirtualChannel, 5000, (PCHAR) buffer, length, &bytesRead);

	if (!bSuccess)
	{
		printf("WTSVirtualChannelRead failed: %d\n", (int) GetLastError());
		//return -1;
	}

	if (!WTSVirtualChannelClose(hVirtualChannel))
	{
		printf("WTSVirtualChannelClose failed\n");
		//return -1;
	}

	return 0;
}

