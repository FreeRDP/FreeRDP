// xrdp_chan_test.cpp : Basic test for virtual channel use.

// These headers are required for the windows terminal service calls.
#include "windows.h"
#include "wtsapi32.h"
#include <string>

#define DSIZE 1024

int main()
{
	// Initialize the data for send/receive
	char* data;
	char* data1;
	data = (char*)malloc(DSIZE);
	data1 = (char*)malloc(DSIZE);
	memset(data, 0xca, DSIZE);
	memset(data1, 0, DSIZE);

	// Open the skel channel in current session
	void* channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, "skel", 0);

	unsigned long written = 0;
	// Write the data to the channel
	bool ret = WTSVirtualChannelWrite(channel, data, DSIZE, &written);
	if (!ret)
	{
		long err = GetLastError();
		fprintf(stderr, "error 0x%8.8x\n", err);
		return 1;
	}

	ret = WTSVirtualChannelRead(channel, 100, data1, DSIZE, &written);
	if (!ret)
	{
		long err = GetLastError();
		fprintf(stderr, "error 0x%8.8x\n", err);
		return 1;
	}
	if (written != DSIZE)
	{
		fprintf(stderr, "error read %d\n", written);
		return 1;
	}

	ret = WTSVirtualChannelClose(channel);
	if (memcmp(data, data1, DSIZE) == 0)
	{
	}
	else
	{
		fprintf(stderr, "error data no match\n");
		return 1;
	}

	fprintf(stderr, "Success!\n");

	Sleep(2000);
	return 0;
}
