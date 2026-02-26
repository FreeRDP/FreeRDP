
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

#define BUFFER_SIZE 16

int TestPipeCreatePipe(int argc, char* argv[])
{
	BOOL status = 0;
	DWORD dwRead = 0;
	DWORD dwWrite = 0;
	HANDLE hReadPipe = nullptr;
	HANDLE hWritePipe = nullptr;
	BYTE readBuffer[BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
	BYTE writeBuffer[BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	status = CreatePipe(&hReadPipe, &hWritePipe, nullptr, BUFFER_SIZE * 2);

	if (!status)
	{
		_tprintf(_T("CreatePipe failed\n"));
		return -1;
	}

	FillMemory(writeBuffer, sizeof(writeBuffer), 0xAA);
	status = WriteFile(hWritePipe, &writeBuffer, sizeof(writeBuffer), &dwWrite, nullptr);

	if (!status)
	{
		_tprintf(_T("WriteFile failed\n"));
		return -1;
	}

	if (dwWrite != sizeof(writeBuffer))
	{
		_tprintf(_T("WriteFile: unexpected number of bytes written: Actual: %") _T(
		             PRIu32) _T(", Expected: %") _T(PRIuz) _T("\n"),
		         dwWrite, sizeof(writeBuffer));
		return -1;
	}

	status = ReadFile(hReadPipe, &readBuffer, sizeof(readBuffer), &dwRead, nullptr);

	if (!status)
	{
		_tprintf(_T("ReadFile failed\n"));
		return -1;
	}

	if (dwRead != sizeof(readBuffer))
	{
		_tprintf(_T("ReadFile: unexpected number of bytes read: Actual: %") _T(
		             PRIu32) _T(", Expected: %") _T(PRIuz) _T("\n"),
		         dwWrite, sizeof(readBuffer));
		return -1;
	}

	if (memcmp(readBuffer, writeBuffer, BUFFER_SIZE) != 0)
	{
		_tprintf(_T("ReadFile: read buffer is different from write buffer\n"));
		return -1;
	}

	(void)CloseHandle(hReadPipe);
	(void)CloseHandle(hWritePipe);

	return 0;
}
