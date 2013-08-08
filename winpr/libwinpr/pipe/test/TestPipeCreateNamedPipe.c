
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <winpr/file.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#define PIPE_BUFFER_SIZE	32

static HANDLE ReadyEvent;

static LPTSTR lpszPipeName = _T("\\\\.\\pipe\\winpr_test_pipe");

static void* named_pipe_client_thread(void* arg)
{
	HANDLE hNamedPipe;
	BYTE* lpReadBuffer;
	BYTE* lpWriteBuffer;
	BOOL fSuccess = FALSE;
	DWORD nNumberOfBytesToRead;
	DWORD nNumberOfBytesToWrite;
	DWORD lpNumberOfBytesRead;
	DWORD lpNumberOfBytesWritten;

	WaitForSingleObject(ReadyEvent, INFINITE);

	hNamedPipe = CreateFile(lpszPipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (!hNamedPipe)
	{
		printf("Named Pipe CreateFile failure: NULL handle\n");
		return NULL;
	}

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("Named Pipe CreateFile failure: INVALID_HANDLE_VALUE\n");
		return NULL;
	}

	lpReadBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE);
	lpWriteBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE);

	lpNumberOfBytesWritten = 0;
	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;

	FillMemory(lpWriteBuffer, PIPE_BUFFER_SIZE, 0x59);

	fSuccess = WriteFile(hNamedPipe, lpWriteBuffer, nNumberOfBytesToWrite, &lpNumberOfBytesWritten, NULL);

	if (!fSuccess || (lpNumberOfBytesWritten == 0))
	{
		printf("Client NamedPipe WriteFile failure\n");
		return NULL;
	}

	lpNumberOfBytesRead = 0;
	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;

	ZeroMemory(lpReadBuffer, PIPE_BUFFER_SIZE);

	fSuccess = ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, &lpNumberOfBytesRead, NULL);

	if (!fSuccess || (lpNumberOfBytesRead == 0))
	{
		printf("Client NamedPipe ReadFile failure\n");
		return NULL;
	}

	printf("Client ReadFile (%d):\n", lpNumberOfBytesRead);
	winpr_HexDump(lpReadBuffer, lpNumberOfBytesRead);

	free(lpReadBuffer);
	free(lpWriteBuffer);

	CloseHandle(hNamedPipe);

	return NULL;
}

static void* named_pipe_server_thread(void* arg)
{
	HANDLE hNamedPipe;
	BYTE* lpReadBuffer;
	BYTE* lpWriteBuffer;
	BOOL fSuccess = FALSE;
	BOOL fConnected = FALSE;
	DWORD nNumberOfBytesToRead;
	DWORD nNumberOfBytesToWrite;
	DWORD lpNumberOfBytesRead;
	DWORD lpNumberOfBytesWritten;

	hNamedPipe = CreateNamedPipe(lpszPipeName,
			PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 0, NULL);

	if (!hNamedPipe)
	{
		printf("CreateNamedPipe failure: NULL handle\n");
		return NULL;
	}

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe failure: INVALID_HANDLE_VALUE\n");
		return NULL;
	}

	SetEvent(ReadyEvent);

	fConnected = ConnectNamedPipe(hNamedPipe, NULL);

	if (!fConnected)
		fConnected = (GetLastError() == ERROR_PIPE_CONNECTED);

	if (!fConnected)
	{
		printf("ConnectNamedPipe failure\n");
		return NULL;
	}

	lpReadBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE);
	lpWriteBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE);

	lpNumberOfBytesRead = 0;
	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;

	ZeroMemory(lpReadBuffer, PIPE_BUFFER_SIZE);

	fSuccess = ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, &lpNumberOfBytesRead, NULL);

	if (!fSuccess || (lpNumberOfBytesRead == 0))
	{
		printf("Server NamedPipe ReadFile failure\n");
		return NULL;
	}

	printf("Server ReadFile (%d):\n", lpNumberOfBytesRead);
	winpr_HexDump(lpReadBuffer, lpNumberOfBytesRead);

	lpNumberOfBytesWritten = 0;
	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;

	FillMemory(lpWriteBuffer, PIPE_BUFFER_SIZE, 0x45);

	fSuccess = WriteFile(hNamedPipe, lpWriteBuffer, nNumberOfBytesToWrite, &lpNumberOfBytesWritten, NULL);

	if (!fSuccess || (lpNumberOfBytesWritten == 0))
	{
		printf("Server NamedPipe WriteFile failure\n");
		return NULL;
	}

	free(lpReadBuffer);
	free(lpWriteBuffer);

	CloseHandle(hNamedPipe);

	return NULL;
}

int TestPipeCreateNamedPipe(int argc, char* argv[])
{
	HANDLE ClientThread;
	HANDLE ServerThread;

	ReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	ClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) named_pipe_client_thread, NULL, 0, NULL);
	ServerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) named_pipe_server_thread, NULL, 0, NULL);

	WaitForSingleObject(ClientThread, INFINITE);
	WaitForSingleObject(ServerThread, INFINITE);

	return 0;
}

