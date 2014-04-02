
#include <winpr/crt.h>
#include <winpr/comm.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/handle.h>

int TestCommConfig(int argc, char* argv[])
{
	DCB dcb;
	HANDLE hComm;
	BOOL fSuccess;
	LPCSTR lpFileName = "\\\\.\\COM1";

	hComm = CreateFileA(lpFileName,
			GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);

	if (!hComm || (hComm == INVALID_HANDLE_VALUE))
	{
		printf("CreateFileA failure: %s\n", lpFileName);
		return 0;
	}

	ZeroMemory(&dcb, sizeof(DCB));

	fSuccess = GetCommState(hComm, &dcb);

	if (!fSuccess)
	{
		printf("GetCommState failure: GetLastError() = %d\n", (int) GetLastError());
		return 0;
	}

	printf("BaudRate: %d ByteSize: %d Parity: %d StopBits: %d\n",
			(int) dcb.BaudRate, (int) dcb.ByteSize, (int) dcb.Parity, (int) dcb.StopBits);

	dcb.BaudRate = CBR_57600;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	fSuccess = SetCommState(hComm, &dcb);

	if (!fSuccess)
	{
		printf("SetCommState failure: GetLastError() = %d\n", (int) GetLastError());
		return 0;
	}

	fSuccess = GetCommState(hComm, &dcb);

	if (!fSuccess)
	{
		printf("GetCommState failure: GetLastError() = %d\n", (int) GetLastError());
		return 0;
	}

	printf("BaudRate: %d ByteSize: %d Parity: %d StopBits: %d\n",
			(int) dcb.BaudRate, (int) dcb.ByteSize, (int) dcb.Parity, (int) dcb.StopBits);

	CloseHandle(hComm);

	return 0;
}

