
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
#define PIPE_TIMEOUT_MS		20000	// 20 seconds

BYTE SERVER_MESSAGE[PIPE_BUFFER_SIZE];
BYTE CLIENT_MESSAGE[PIPE_BUFFER_SIZE];

BOOL bClientSuccess = FALSE;
BOOL bServerSuccess = FALSE;

static HANDLE serverReadyEvent;

static LPTSTR lpszPipeName = _T("\\\\.\\pipe\\winpr_test_pipe_overlapped");

static void* named_pipe_client_thread(void* arg)
{
	DWORD status;
	HANDLE hEvent = NULL;
	HANDLE hNamedPipe = NULL;
	BYTE* lpReadBuffer = NULL;
	BOOL fSuccess = FALSE;
	OVERLAPPED overlapped;
	DWORD nNumberOfBytesToRead;
	DWORD nNumberOfBytesToWrite;
	DWORD NumberOfBytesTransferred;

	status = WaitForSingleObject(serverReadyEvent, PIPE_TIMEOUT_MS);
	if (status != WAIT_OBJECT_0)
	{
		printf("client: failed to wait for server ready event: %u\n", status);
		goto finish;
	}


	/* 1: initialize overlapped structure */

	ZeroMemory(&overlapped, sizeof(OVERLAPPED));
	if (!(hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("client: CreateEvent failure: %u\n", GetLastError());
		goto finish;
	}
	overlapped.hEvent = hEvent;


	/* 2: connect to server named pipe */

	hNamedPipe = CreateFile(lpszPipeName, GENERIC_READ | GENERIC_WRITE,
	                 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("client: Named Pipe CreateFile failure: %u\n", GetLastError());
		goto finish;
	}


	/* 3: write to named pipe */

	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;
	NumberOfBytesTransferred = 0;

	fSuccess = WriteFile(hNamedPipe, CLIENT_MESSAGE, nNumberOfBytesToWrite, NULL, &overlapped);

	if (!fSuccess)
		fSuccess = (GetLastError() == ERROR_IO_PENDING);

	if (!fSuccess)
	{
		printf("client: NamedPipe WriteFile failure (initial): %u\n", GetLastError());
		goto finish;
	}

	status = WaitForSingleObject(hEvent, PIPE_TIMEOUT_MS);
	if (status != WAIT_OBJECT_0)
	{
		printf("client: failed to wait for overlapped event (write): %u\n", status);
		goto finish;
	}

	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, FALSE);
	if (!fSuccess)
	{
		printf("client: NamedPipe WriteFile failure (final): %u\n", GetLastError());
		goto finish;
	}
	printf("client: WriteFile transferred %u bytes:\n", NumberOfBytesTransferred);


	/* 4: read from named pipe */

	if (!(lpReadBuffer = (BYTE*)calloc(1, PIPE_BUFFER_SIZE)))
	{
		printf("client: Error allocating read buffer\n");
		goto finish;
	}

	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;
	NumberOfBytesTransferred = 0;

	fSuccess = ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, NULL, &overlapped);

	if (!fSuccess)
		fSuccess = (GetLastError() == ERROR_IO_PENDING);

	if (!fSuccess)
	{
		printf("client: NamedPipe ReadFile failure (initial): %u\n", GetLastError());
		goto finish;
	}

	status = WaitForMultipleObjects(1, &hEvent, FALSE, PIPE_TIMEOUT_MS);
	if (status != WAIT_OBJECT_0)
	{
		printf("client: failed to wait for overlapped event (read): %u\n", status);
		goto finish;
	}

	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, TRUE);
	if (!fSuccess)
	{
		printf("client: NamedPipe ReadFile failure (final): %u\n", GetLastError());
		goto finish;
	}

	printf("client: ReadFile transferred %u bytes:\n", NumberOfBytesTransferred);
	winpr_HexDump("pipe.test", WLOG_DEBUG, lpReadBuffer, NumberOfBytesTransferred);

	if (NumberOfBytesTransferred != PIPE_BUFFER_SIZE || memcmp(lpReadBuffer, SERVER_MESSAGE, PIPE_BUFFER_SIZE))
	{
		printf("client: received unexpected data from server\n");
		goto finish;
	}

	printf("client: finished successfully\n");
	bClientSuccess = TRUE;

finish:
	free(lpReadBuffer);
	if (hNamedPipe)
		CloseHandle(hNamedPipe);
	if (hEvent)
		CloseHandle(hEvent);

	return NULL;
}

static void* named_pipe_server_thread(void* arg)
{
	DWORD status;
	HANDLE hEvent = NULL;
	HANDLE hNamedPipe = NULL;
	BYTE* lpReadBuffer = NULL;
	OVERLAPPED overlapped;
	BOOL fSuccess = FALSE;
	BOOL fConnected = FALSE;
	DWORD nNumberOfBytesToRead;
	DWORD nNumberOfBytesToWrite;
	DWORD NumberOfBytesTransferred;

	/* 1: initialize overlapped structure */

	ZeroMemory(&overlapped, sizeof(OVERLAPPED));
	if (!(hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("server: CreateEvent failure: %u\n", GetLastError());
		SetEvent(serverReadyEvent); /* unblock client thread */
		goto finish;
	}
	overlapped.hEvent = hEvent;


	/* 2: create named pipe and set ready event */

	hNamedPipe = CreateNamedPipe(lpszPipeName,
	                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
	                PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 0, NULL);

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("server: CreateNamedPipe failure: %u\n", GetLastError());
		SetEvent(serverReadyEvent); /* unblock client thread */
		goto finish;
	}

	SetEvent(serverReadyEvent);

	/* 3: connect named pipe */

#if 0
	/* This sleep will most certainly cause ERROR_PIPE_CONNECTED below */
	Sleep(2000);
#endif

	fConnected = ConnectNamedPipe(hNamedPipe, &overlapped);
	status = GetLastError();

	/**
	 * At this point if fConnected is FALSE, we have to check GetLastError() for:
	 * ERROR_PIPE_CONNECTED:
	 *     client has already connected before we have called ConnectNamedPipe.
	 *     this is quite common depending on the timings and indicates success
	 * ERROR_IO_PENDING:
	 *     Since we're using ConnectNamedPipe asynchronously here, the function returns
	 *     immediately and this error code simply indicates that the operation is
	 *     still in progress. Hence we have to wait for the completion event and use
	 *     GetOverlappedResult to query the actual result of the operation (note that
	 *     the lpNumberOfBytesTransferred parameter is undefined/useless for a
	 *     ConnectNamedPipe operation)
	 */

	if (!fConnected)
		fConnected = (status == ERROR_PIPE_CONNECTED);

	printf("server: ConnectNamedPipe status: %u\n", status);

	if (!fConnected && status == ERROR_IO_PENDING)
	{
		DWORD dwDummy;
		printf("server: waiting up to %u ms for connection ...\n", PIPE_TIMEOUT_MS);
		status = WaitForSingleObject(hEvent, PIPE_TIMEOUT_MS);
		if (status == WAIT_OBJECT_0)
			fConnected = GetOverlappedResult(hNamedPipe, &overlapped, &dwDummy, FALSE);
		else
			printf("server: failed to wait for overlapped event (connect): %u\n", status);
	}

	if (!fConnected)
	{
		printf("server: ConnectNamedPipe failed: %u\n", status);
		goto finish;
	}

	printf("server: named pipe successfully connected\n");


	/* 4: read from named pipe */

	if (!(lpReadBuffer = (BYTE*)calloc(1, PIPE_BUFFER_SIZE)))
	{
		printf("server: Error allocating read buffer\n");
		goto finish;
	}

	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;
	NumberOfBytesTransferred = 0;

	fSuccess = ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, NULL, &overlapped);

	if (!fSuccess)
		fSuccess = (GetLastError() == ERROR_IO_PENDING);

	if (!fSuccess)
	{
		printf("server: NamedPipe ReadFile failure (initial): %u\n", GetLastError());
		goto finish;
	}

	status = WaitForSingleObject(hEvent, PIPE_TIMEOUT_MS);
	if (status != WAIT_OBJECT_0)
	{
		printf("server: failed to wait for overlapped event (read): %u\n", status);
		goto finish;
	}

	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, FALSE);
	if (!fSuccess)
	{
		printf("server: NamedPipe ReadFile failure (final): %u\n", GetLastError());
		goto finish;
	}

	printf("server: ReadFile transferred %u bytes:\n", NumberOfBytesTransferred);
	winpr_HexDump("pipe.test", WLOG_DEBUG, lpReadBuffer, NumberOfBytesTransferred);

	if (NumberOfBytesTransferred != PIPE_BUFFER_SIZE || memcmp(lpReadBuffer, CLIENT_MESSAGE, PIPE_BUFFER_SIZE))
	{
		printf("server: received unexpected data from client\n");
		goto finish;
	}


	/* 5: write to named pipe */

	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;
	NumberOfBytesTransferred = 0;

	fSuccess = WriteFile(hNamedPipe, SERVER_MESSAGE, nNumberOfBytesToWrite, NULL, &overlapped);

	if (!fSuccess)
		fSuccess = (GetLastError() == ERROR_IO_PENDING);

	if (!fSuccess)
	{
		printf("server: NamedPipe WriteFile failure (initial): %u\n", GetLastError());
		goto finish;
	}

	status = WaitForSingleObject(hEvent, PIPE_TIMEOUT_MS);
	if (status != WAIT_OBJECT_0)
	{
		printf("server: failed to wait for overlapped event (write): %u\n", status);
		goto finish;
	}

	fSuccess = GetOverlappedResult(hNamedPipe, &overlapped, &NumberOfBytesTransferred, FALSE);
	if (!fSuccess)
	{
		printf("server: NamedPipe WriteFile failure (final): %u\n", GetLastError());
		goto finish;
	}

	printf("server: WriteFile transferred %u bytes:\n", NumberOfBytesTransferred);
	//winpr_HexDump("pipe.test", WLOG_DEBUG, lpWriteBuffer, NumberOfBytesTransferred);

	bServerSuccess = TRUE;
	printf("server: finished successfully\n");

finish:
	CloseHandle(hNamedPipe);
	CloseHandle(hEvent);
	free(lpReadBuffer);
	return NULL;
}

int TestPipeCreateNamedPipeOverlapped(int argc, char* argv[])
{
	HANDLE ClientThread;
	HANDLE ServerThread;
	int result = -1;

	FillMemory(SERVER_MESSAGE, PIPE_BUFFER_SIZE, 0xAA);
	FillMemory(CLIENT_MESSAGE, PIPE_BUFFER_SIZE, 0xBB);

	if (!(serverReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("CreateEvent failed: %d\n", GetLastError());
		goto out;
	}
	if (!(ClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) named_pipe_client_thread, NULL, 0, NULL)))
	{
		printf("CreateThread (client) failed: %d\n", GetLastError());
		goto out;
	}
	if (!(ServerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) named_pipe_server_thread, NULL, 0, NULL)))
	{
		printf("CreateThread (server) failed: %d\n", GetLastError());
		goto out;
	}

	if (WAIT_OBJECT_0 != WaitForSingleObject(ClientThread, INFINITE))
	{
		printf("%s: Failed to wait for client thread: %u\n",
			__FUNCTION__,  GetLastError());
		goto out;
	}
	if (WAIT_OBJECT_0 != WaitForSingleObject(ServerThread, INFINITE))
	{
		printf("%s: Failed to wait for server thread: %u\n",
			__FUNCTION__,  GetLastError());
		goto out;
	}

	if (bClientSuccess && bServerSuccess)
		result = 0;

out:

#ifndef _WIN32
	if (result == 0)
	{
		printf("%s: Error, this test is currently expected not to succeed on this platform.\n",
			__FUNCTION__);
		result = -1;
	}
	else
	{
		printf("%s: This test is currently expected to fail on this platform.\n",
			__FUNCTION__);
		result = 0;
	}
#endif

	return result;
}
