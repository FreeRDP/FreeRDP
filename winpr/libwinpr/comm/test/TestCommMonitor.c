
#include <winpr/crt.h>
#include <winpr/comm.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/handle.h>

int TestCommMonitor(int argc, char* argv[])
{
	HANDLE hComm;
	DWORD dwError;
	BOOL fSuccess;
	DWORD dwEvtMask;
	OVERLAPPED overlapped;
	LPCSTR lpFileName = "\\\\.\\COM1";

	hComm = CreateFileA(lpFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
	                    FILE_FLAG_OVERLAPPED, NULL);

	if (!hComm || (hComm == INVALID_HANDLE_VALUE))
	{
		printf("CreateFileA failure: %s\n", lpFileName);
		return -1;
	}

	fSuccess = SetCommMask(hComm, EV_CTS | EV_DSR);

	if (!fSuccess)
	{
		printf("SetCommMask failure: GetLastError() = %" PRIu32 "\n", GetLastError());
		return -1;
	}

	ZeroMemory(&overlapped, sizeof(OVERLAPPED));
	if (!(overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("CreateEvent failed: GetLastError() = %" PRIu32 "\n", GetLastError());
		return -1;
	}

	if (WaitCommEvent(hComm, &dwEvtMask, &overlapped))
	{
		if (dwEvtMask & EV_DSR)
		{
			printf("EV_DSR\n");
		}

		if (dwEvtMask & EV_CTS)
		{
			printf("EV_CTS\n");
		}
	}
	else
	{
		dwError = GetLastError();

		if (dwError == ERROR_IO_PENDING)
		{
			printf("ERROR_IO_PENDING\n");
		}
		else
		{
			printf("WaitCommEvent failure: GetLastError() = %" PRIu32 "\n", dwError);
			return -1;
		}
	}

	CloseHandle(hComm);

	return 0;
}
