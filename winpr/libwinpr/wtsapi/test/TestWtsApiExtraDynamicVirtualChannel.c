
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/wtsapi.h>

int TestWtsApiExtraDynamicVirtualChannel(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	HANDLE hVirtualChannel =
	    WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "ECHO", WTS_CHANNEL_OPTION_DYNAMIC);

	if (hVirtualChannel == INVALID_HANDLE_VALUE)
	{
		printf("WTSVirtualChannelOpen failed: %" PRIu32 "\n", GetLastError());
		return -1;
	}
	printf("WTSVirtualChannelOpen opend");
	ULONG bytesWritten = 0;
	char buffer[1024] = { 0 };
	size_t length = sizeof(buffer);
	BOOL bSuccess = WTSVirtualChannelWrite(hVirtualChannel, buffer, length, &bytesWritten);

	if (!bSuccess)
	{
		printf("WTSVirtualChannelWrite failed: %" PRIu32 "\n", GetLastError());
		return -1;
	}
	printf("WTSVirtualChannelWrite written");

	ULONG bytesRead = 0;
	bSuccess = WTSVirtualChannelRead(hVirtualChannel, 5000, (PCHAR)buffer, length, &bytesRead);

	if (!bSuccess)
	{
		printf("WTSVirtualChannelRead failed: %" PRIu32 "\n", GetLastError());
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
