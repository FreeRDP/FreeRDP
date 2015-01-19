
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

#define BUFFER_SIZE	16

int TestPipeCreatePipe(int argc, char* argv[])
{
	BOOL status;
	DWORD dwRead;
	DWORD dwWrite;
	HANDLE hReadPipe;
	HANDLE hWritePipe;
	BYTE readBuffer[BUFFER_SIZE];
	BYTE writeBuffer[BUFFER_SIZE];

	status = CreatePipe(&hReadPipe, &hWritePipe, NULL, BUFFER_SIZE * 2);

	if (!status)
	{
		_tprintf(_T("CreatePipe failed\n"));
		return -1;
	}

	FillMemory(writeBuffer, sizeof(writeBuffer), 0xAA);
	status = WriteFile(hWritePipe, &writeBuffer, sizeof(writeBuffer), &dwWrite, NULL);

	if (!status)
	{
		_tprintf(_T("WriteFile failed\n"));
		return -1;
	}

	if (dwWrite != sizeof(writeBuffer))
	{
		_tprintf(_T("WriteFile: unexpected number of bytes written: Actual: %d, Expected: %d\n"),
			dwWrite, (int) sizeof(writeBuffer));
		return -1;
	}

	ZeroMemory(readBuffer, sizeof(readBuffer));
	status = ReadFile(hReadPipe, &readBuffer, sizeof(readBuffer), &dwRead, NULL);

	if (!status)
	{
		_tprintf(_T("ReadFile failed\n"));
		return -1;
	}

	if (dwRead != sizeof(readBuffer))
	{
		_tprintf(_T("ReadFile: unexpected number of bytes read: Actual: %d, Expected: %d\n"),
			dwWrite, (int) sizeof(readBuffer));
		return -1;
	}

	if (memcmp(readBuffer, writeBuffer, BUFFER_SIZE) != 0)
	{
		_tprintf(_T("ReadFile: read buffer is different from write buffer\n"));
		return -1;
	}

	CloseHandle(hReadPipe);
	CloseHandle(hWritePipe);

	return 0;
}
