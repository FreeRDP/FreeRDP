
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <winpr/file.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>
#include <winpr/wlog.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#define PIPE_BUFFER_SIZE	32

static HANDLE ReadyEvent;

static LPTSTR lpszPipeName = _T("\\\\.\\pipe\\winpr_test_pipe_overlapped");

static void* named_pipe_client_thread(void* arg)
{
	DWORD status;
	HANDLE hEvent = NULL;
	HANDLE hNamedPipe = NULL;
	BYTE* lpReadBuffer = NULL;
	BYTE* lpWriteBuffer = NULL;
	BOOL fSuccess = FALSE;
	OVERLAPPED overlapped;
	DWORD nNumberOfBytesToRead;
	DWORD nNumberOfBytesToWrite;
	DWORD NumberOfBytesTransferred;

	WaitForSingleObject(ReadyEvent, INFINITE);
	hNamedPipe = CreateFile(lpszPipeName, GENERIC_READ | GENERIC_WRITE,
				0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (!hNamedPipe)
	{
		printf("Named Pipe CreateFile failure: NULL handle\n");
		goto finish;
	}

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("Named Pipe CreateFile failure: INVALID_HANDLE_VALUE\n");
		goto finish;
	}

	lpReadBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE);
	lpWriteBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE);
	if (!lpReadBuffer || !lpWriteBuffer)
	{
		printf("Error allocating memory\n");
		goto finish;
	}
	ZeroMemory(&overlapped, sizeof(OVERLAPPED));

	if (!(hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("CreateEvent failure: (%d)\n", GetLastError());
		goto finish;
	}

	overlapped.hEvent = hEvent;
	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;
	FillMemory(lpWriteBuffer, PIPE_BUFFER_SIZE, 0x59);
	fSuccess = WriteFile(hNamedPipe, lpWriteBuffer, nNumberOfBytesToWrite, NULL, &overlapped);

	if (!fSuccess)
		fSuccess = (GetLastError() == ERROR_IO_PENDING);

	if (!fSuccess)
	{
		printf("Client NamedPipe WriteFile failure: %d\n", GetLastError());
		goto finish;
	}

	status = WaitForMultipleObjects(1, &hEvent, FALSE, INFINITE);
	NumberOfBytesTransferred = 0;
	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, TRUE);
	printf("Client GetOverlappedResult: fSuccess: %d NumberOfBytesTransferred: %d\n",
		fSuccess, NumberOfBytesTransferred);
	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;
	ZeroMemory(lpReadBuffer, PIPE_BUFFER_SIZE);
	fSuccess = ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, NULL, &overlapped);

	if (!fSuccess)
		fSuccess = (GetLastError() == ERROR_IO_PENDING);

	if (!fSuccess)
	{
		printf("Client NamedPipe ReadFile failure: %d\n", GetLastError());
		goto finish;
	}

	status = WaitForMultipleObjects(1, &hEvent, FALSE, INFINITE);
	NumberOfBytesTransferred = 0;
	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, TRUE);
	printf("Client GetOverlappedResult: fSuccess: %d NumberOfBytesTransferred: %d\n",
		fSuccess, NumberOfBytesTransferred);
	printf("Client ReadFile (%d):\n", NumberOfBytesTransferred);
	winpr_HexDump("pipe.test", WLOG_DEBUG, lpReadBuffer, NumberOfBytesTransferred);

finish:
	free(lpReadBuffer);
	free(lpWriteBuffer);
	if (hNamedPipe)
		CloseHandle(hNamedPipe);
	if (hEvent)
		CloseHandle(hEvent);

	return NULL;
}

static void* named_pipe_server_thread(void* arg)
{
	DWORD status;
	HANDLE hEvent;
	HANDLE hNamedPipe;
	BYTE* lpReadBuffer;
	BYTE* lpWriteBuffer;
	OVERLAPPED overlapped;
	BOOL fSuccess = FALSE;
	BOOL fConnected = FALSE;
	DWORD nNumberOfBytesToRead;
	DWORD nNumberOfBytesToWrite;
	DWORD NumberOfBytesTransferred;
	hNamedPipe = CreateNamedPipe(lpszPipeName,
								 PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
								 PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 0, NULL);

	if (!hNamedPipe)
	{
		printf("CreateNamedPipe failure: NULL handle\n");
		return NULL;
	}

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe failure: INVALID_HANDLE_VALUE (%d)\n", GetLastError());
		return NULL;
	}

	SetEvent(ReadyEvent);
	ZeroMemory(&overlapped, sizeof(OVERLAPPED));

	if (!(hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("CreateEvent failure: (%d)\n", GetLastError());
		return NULL;
	}

	overlapped.hEvent = hEvent;
	fConnected = ConnectNamedPipe(hNamedPipe, &overlapped);
	printf("ConnectNamedPipe status: %d\n", GetLastError());

	if (!fConnected)
		fConnected = (GetLastError() == ERROR_IO_PENDING);

	status = WaitForMultipleObjects(1, &hEvent, FALSE, INFINITE);
	NumberOfBytesTransferred = 0;
	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, TRUE);
	printf("Server GetOverlappedResult: fSuccess: %d NumberOfBytesTransferred: %d\n", fSuccess, NumberOfBytesTransferred);

	if (!fConnected)
	{
		printf("ConnectNamedPipe failure: %d\n", GetLastError());
		CloseHandle(hNamedPipe);
		CloseHandle(hEvent);
		return NULL;
	}

	lpReadBuffer = (BYTE*) calloc(1, PIPE_BUFFER_SIZE);
	lpWriteBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE);
	if (!lpReadBuffer || !lpWriteBuffer)
	{
		printf("Error allocating memory\n");
		free(lpReadBuffer);
		free(lpWriteBuffer);
		return NULL;
	}
	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;
	fSuccess = ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, NULL, &overlapped);

	if (!fSuccess)
		fSuccess = (GetLastError() == ERROR_IO_PENDING);

	if (!fSuccess)
	{
		printf("Server NamedPipe ReadFile failure: %d\n", GetLastError());
		free(lpReadBuffer);
		free(lpWriteBuffer);
		CloseHandle(hNamedPipe);
		CloseHandle(hEvent);
		return NULL;
	}

	status = WaitForMultipleObjects(1, &hEvent, FALSE, INFINITE);
	NumberOfBytesTransferred = 0;
	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, TRUE);
	printf("Server GetOverlappedResult: fSuccess: %d NumberOfBytesTransferred: %d\n", fSuccess, NumberOfBytesTransferred);
	printf("Server ReadFile (%d):\n", NumberOfBytesTransferred);
	winpr_HexDump("pipe.test", WLOG_DEBUG, lpReadBuffer, NumberOfBytesTransferred);
	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;
	FillMemory(lpWriteBuffer, PIPE_BUFFER_SIZE, 0x45);
	fSuccess = WriteFile(hNamedPipe, lpWriteBuffer, nNumberOfBytesToWrite, NULL, &overlapped);

	if (!fSuccess)
		fSuccess = (GetLastError() == ERROR_IO_PENDING);

	if (!fSuccess)
	{
		printf("Server NamedPipe WriteFile failure: %d\n", GetLastError());
		free(lpReadBuffer);
		free(lpWriteBuffer);
		CloseHandle(hNamedPipe);
		CloseHandle(hEvent);
		return NULL;
	}

	status = WaitForMultipleObjects(1, &hEvent, FALSE, INFINITE);
	NumberOfBytesTransferred = 0;
	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, TRUE);
	printf("Server GetOverlappedResult: fSuccess: %d NumberOfBytesTransferred: %d\n", fSuccess, NumberOfBytesTransferred);
	free(lpReadBuffer);
	free(lpWriteBuffer);
	CloseHandle(hNamedPipe);
	CloseHandle(hEvent);
	return NULL;
}

int TestPipeCreateNamedPipeOverlapped(int argc, char* argv[])
{
	HANDLE ClientThread;
	HANDLE ServerThread;

	if (!(ReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("CreateEvent failed: %d\n", GetLastError());
		return -1;
	}
	if (!(ClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) named_pipe_client_thread, NULL, 0, NULL)))
	{
		printf("CreateThread (client) failed: %d\n", GetLastError());
		return -1;
	}
	if (!(ServerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) named_pipe_server_thread, NULL, 0, NULL)))
	{
		printf("CreateThread (server) failed: %d\n", GetLastError());
		return -1;
	}

	WaitForSingleObject(ClientThread, INFINITE);
	WaitForSingleObject(ServerThread, INFINITE);

	/* FIXME: Since this function always returns 0 this test is very much useless */
	return 0;
}
