
#include <winpr/crt.h>
#include <winpr/comm.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/handle.h>

int TestCommMonitor(int argc, char* argv[])
{
	HANDLE hComm = nullptr;
	DWORD dwError = 0;
	BOOL fSuccess = 0;
	DWORD dwEvtMask = 0;
	OVERLAPPED overlapped = WINPR_C_ARRAY_INIT;
	LPCSTR lpFileName = "\\\\.\\COM1";

	hComm = CreateFileA(lpFileName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
	                    FILE_FLAG_OVERLAPPED, nullptr);

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

	if (!(overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr)))
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

	(void)CloseHandle(hComm);

	return 0;
}
