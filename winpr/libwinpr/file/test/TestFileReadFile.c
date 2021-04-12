
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/windows.h>
#include <winpr/synch.h>

typedef struct
{
	OVERLAPPED overlapped;

	DWORD magic;
	DWORD dwNumberOfBytesTransfered;
	DWORD dwErrorCode;
	HANDLE h;
	BOOL success;
} CustomOverlapped;

VOID CALLBACK readCb1(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	CustomOverlapped* ov = (CustomOverlapped*)lpOverlapped;
	ov->dwNumberOfBytesTransfered = dwNumberOfBytesTransfered;
	ov->dwErrorCode = dwErrorCode;
}

CustomOverlapped ov2;
BYTE buffer2[4];

VOID CALLBACK readCb3(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	CustomOverlapped* ov = (CustomOverlapped*)lpOverlapped;
	ov->success = TRUE;
}

VOID CALLBACK readCb2(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	CustomOverlapped* ov = (CustomOverlapped*)lpOverlapped;
	ov->success = ReadFileEx(ov->h, buffer2, 4, &ov2.overlapped, readCb3);
}

int TestFileReadFile(int argc, char* argv[])
{
	CustomOverlapped ov1;
	HANDLE h;
	BYTE buffer[4];
	DWORD transfered;

	h = CreateFile(__FILE__, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return 1;

	/* test single Read */
	ZeroMemory(&ov1, sizeof(ov1));
	ov1.magic = 0xabcdef;
	ov1.h = h;
	if (!ReadFileEx(h, buffer, 4, &ov1.overlapped, readCb1))
		return 2;

	if (SleepEx(1, TRUE) != WAIT_IO_COMPLETION)
		return 3;

	if (!GetOverlappedResult(h, &ov1.overlapped, &transfered, FALSE) || transfered != 4)
		return 4;

	if (ov1.dwErrorCode != ERROR_SUCCESS || ov1.dwNumberOfBytesTransfered != 4)
		return 5;

	/* test completion routine that recall ReadFileEx() */
	ZeroMemory(&ov1, sizeof(ov1));
	ov1.h = h;
	ZeroMemory(&ov2, sizeof(ov2));
	ov2.h = h;
	ov2.overlapped.Offset = 4;
	if (!ReadFileEx(h, buffer, 4, &ov1.overlapped, readCb2))
		return 10;

	if (SleepEx(1, TRUE) != WAIT_IO_COMPLETION)
		return 11;

	if (!GetOverlappedResult(h, &ov1.overlapped, &transfered, FALSE) || transfered != 4)
		return 12;

	if (GetOverlappedResult(h, &ov2.overlapped, &transfered, FALSE) ||
	    GetLastError() != ERROR_IO_INCOMPLETE)
		return 13;

	if (SleepEx(1, TRUE) != WAIT_IO_COMPLETION)
		return 14;

	if (!GetOverlappedResult(h, &ov2.overlapped, &transfered, FALSE) || transfered != 4 ||
	    !ov2.success)
		return 15;

	CloseHandle(h);
	return 0;
}
