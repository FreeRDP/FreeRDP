
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <winpr/file.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/wlog.h>
#include <winpr/thread.h>
#ifndef _WIN32
#include <signal.h>
#endif
#include "../pipe.h"

#define PIPE_BUFFER_SIZE	32

static HANDLE ReadyEvent;

static LPTSTR lpszPipeNameMt = _T("\\\\.\\pipe\\winpr_test_pipe_mt");
static LPTSTR lpszPipeNameSt = _T("\\\\.\\pipe\\winpr_test_pipe_st");

static BOOL testFailed = FALSE;

static DWORD WINAPI named_pipe_client_thread(LPVOID arg)
{
	HANDLE hNamedPipe = NULL;
	BYTE* lpReadBuffer = NULL;
	BYTE* lpWriteBuffer = NULL;
	BOOL fSuccess = FALSE;
	DWORD nNumberOfBytesToRead;
	DWORD nNumberOfBytesToWrite;
	DWORD lpNumberOfBytesRead;
	DWORD lpNumberOfBytesWritten;
	WaitForSingleObject(ReadyEvent, INFINITE);
	hNamedPipe = CreateFile(lpszPipeNameMt, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("%s: Named Pipe CreateFile failure: INVALID_HANDLE_VALUE\n", __FUNCTION__);
		goto out;
	}

	if (!(lpReadBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE)))
	{
		printf("%s: Error allocating read buffer\n", __FUNCTION__);
		goto out;
	}

	if (!(lpWriteBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE)))
	{
		printf("%s: Error allocating write buffer\n", __FUNCTION__);
		goto out;
	}

	lpNumberOfBytesWritten = 0;
	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;
	FillMemory(lpWriteBuffer, PIPE_BUFFER_SIZE, 0x59);

	if (!WriteFile(hNamedPipe, lpWriteBuffer, nNumberOfBytesToWrite, &lpNumberOfBytesWritten, NULL) ||
			lpNumberOfBytesWritten != nNumberOfBytesToWrite)
	{
		printf("%s: Client NamedPipe WriteFile failure\n", __FUNCTION__);
		goto out;
	}

	lpNumberOfBytesRead = 0;
	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;
	ZeroMemory(lpReadBuffer, PIPE_BUFFER_SIZE);

	if (!ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, &lpNumberOfBytesRead, NULL) ||
			lpNumberOfBytesRead != nNumberOfBytesToRead)
	{
		printf("%s: Client NamedPipe ReadFile failure\n", __FUNCTION__);
		goto out;
	}

	printf("Client ReadFile: %"PRIu32" bytes\n", lpNumberOfBytesRead);
	winpr_HexDump("pipe.test", WLOG_DEBUG, lpReadBuffer, lpNumberOfBytesRead);
	fSuccess = TRUE;
out:
	free(lpReadBuffer);
	free(lpWriteBuffer);
	CloseHandle(hNamedPipe);

	if (!fSuccess)
		testFailed = TRUE;

	ExitThread(0);
	return 0;
}

static DWORD WINAPI named_pipe_server_thread(LPVOID arg)
{
	HANDLE hNamedPipe = NULL;
	BYTE* lpReadBuffer = NULL;
	BYTE* lpWriteBuffer = NULL;
	BOOL fSuccess = FALSE;
	BOOL fConnected = FALSE;
	DWORD nNumberOfBytesToRead;
	DWORD nNumberOfBytesToWrite;
	DWORD lpNumberOfBytesRead;
	DWORD lpNumberOfBytesWritten;
	hNamedPipe = CreateNamedPipe(lpszPipeNameMt,
								 PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
								 PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 0, NULL);

	if (!hNamedPipe)
	{
		printf("%s: CreateNamedPipe failure: NULL handle\n", __FUNCTION__);
		goto out;
	}

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("%s: CreateNamedPipe failure: INVALID_HANDLE_VALUE\n", __FUNCTION__);
		goto out;
	}

	SetEvent(ReadyEvent);

	/**
	 * Note:
	 * If a client connects before ConnectNamedPipe is called, the function returns zero and
	 * GetLastError returns ERROR_PIPE_CONNECTED. This can happen if a client connects in the
	 * interval between the call to CreateNamedPipe and the call to ConnectNamedPipe.
	 * In this situation, there is a good connection between client and server, even though
	 * the function returns zero.
	 */
	fConnected = ConnectNamedPipe(hNamedPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

	if (!fConnected)
	{
		printf("%s: ConnectNamedPipe failure\n", __FUNCTION__);
		goto out;
	}

	if (!(lpReadBuffer = (BYTE*) calloc(1, PIPE_BUFFER_SIZE)))
	{
		printf("%s: Error allocating read buffer\n", __FUNCTION__);
		goto out;
	}

	if (!(lpWriteBuffer = (BYTE*) malloc(PIPE_BUFFER_SIZE)))
	{
		printf("%s: Error allocating write buffer\n", __FUNCTION__);
		goto out;
	}

	lpNumberOfBytesRead = 0;
	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;

	if (!ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, &lpNumberOfBytesRead, NULL) ||
			lpNumberOfBytesRead != nNumberOfBytesToRead)
	{
		printf("%s: Server NamedPipe ReadFile failure\n", __FUNCTION__);
		goto out;
	}

	printf("Server ReadFile: %"PRIu32" bytes\n", lpNumberOfBytesRead);
	winpr_HexDump("pipe.test", WLOG_DEBUG, lpReadBuffer, lpNumberOfBytesRead);
	lpNumberOfBytesWritten = 0;
	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;
	FillMemory(lpWriteBuffer, PIPE_BUFFER_SIZE, 0x45);

	if (!WriteFile(hNamedPipe, lpWriteBuffer, nNumberOfBytesToWrite, &lpNumberOfBytesWritten, NULL) ||
			lpNumberOfBytesWritten != nNumberOfBytesToWrite)
	{
		printf("%s: Server NamedPipe WriteFile failure\n", __FUNCTION__);
		goto out;
	}

	fSuccess = TRUE;
out:
	free(lpReadBuffer);
	free(lpWriteBuffer);
	CloseHandle(hNamedPipe);

	if (!fSuccess)
		testFailed = TRUE;

	ExitThread(0);
	return 0;
}

#define TESTNUMPIPESST 16
static DWORD WINAPI named_pipe_single_thread(LPVOID arg)
{
	HANDLE servers[TESTNUMPIPESST];
	HANDLE clients[TESTNUMPIPESST];
	char sndbuf[PIPE_BUFFER_SIZE];
	char rcvbuf[PIPE_BUFFER_SIZE];
	DWORD dwRead;
	DWORD dwWritten;
	int i;
	int numPipes;
	BOOL bSuccess = FALSE;
	numPipes = TESTNUMPIPESST;
	memset(servers, 0, sizeof(servers));
	memset(clients, 0, sizeof(clients));
	WaitForSingleObject(ReadyEvent, INFINITE);

	for (i = 0; i < numPipes; i++)
	{
		if (!(servers[i] = CreateNamedPipe(lpszPipeNameSt,
										   PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
										   PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 0, NULL)))
		{
			printf("%s: CreateNamedPipe #%d failed\n", __FUNCTION__, i);
			goto out;
		}
	}

#ifndef _WIN32

	for (i = 0; i < numPipes; i++)
	{
		WINPR_NAMED_PIPE* p = (WINPR_NAMED_PIPE*)servers[i];

		if (strcmp(lpszPipeNameSt, p->name))
		{
			printf("%s: Pipe name mismatch for pipe #%d ([%s] instead of [%s])\n",
				   __FUNCTION__, i, p->name, lpszPipeNameSt);
			goto out;
		}

		if (p->clientfd != -1)
		{
			printf("%s: Unexpected client fd value for pipe #%d (%d instead of -1)\n",
				   __FUNCTION__, i, p->clientfd);
			goto out;
		}

		if (p->serverfd < 1)
		{
			printf("%s: Unexpected server fd value for pipe #%d (%d is not > 0)\n",
				   __FUNCTION__, i, p->serverfd);
			goto out;
		}

		if (p->ServerMode == FALSE)
		{
			printf("%s: Unexpected ServerMode value for pipe #%d (0 instead of 1)\n",
				   __FUNCTION__, i);
			goto out;
		}
	}

#endif

	for (i = 0; i < numPipes; i++)
	{
		BOOL fConnected;
		if ((clients[i] = CreateFile(lpszPipeNameSt, GENERIC_READ | GENERIC_WRITE,
									  0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
		{
			printf("%s: CreateFile #%d failed\n", __FUNCTION__, i);
			goto out;
		}

		/**
		 * Note:
		 * If a client connects before ConnectNamedPipe is called, the function returns zero and
		 * GetLastError returns ERROR_PIPE_CONNECTED. This can happen if a client connects in the
		 * interval between the call to CreateNamedPipe and the call to ConnectNamedPipe.
		 * In this situation, there is a good connection between client and server, even though
		 * the function returns zero.
		 */
		fConnected = ConnectNamedPipe(servers[i], NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (!fConnected)
		{
			printf("%s: ConnectNamedPipe #%d failed. (%"PRIu32")\n", __FUNCTION__, i, GetLastError());
			goto out;
		}
	}

#ifndef _WIN32

	for (i = 0; i < numPipes; i++)
	{
		WINPR_NAMED_PIPE* p = servers[i];

		if (p->clientfd < 1)
		{
			printf("%s: Unexpected client fd value for pipe #%d (%d is not > 0)\n",
				   __FUNCTION__, i, p->clientfd);
			goto out;
		}

		if (p->ServerMode)
		{
			printf("%s: Unexpected ServerMode value for pipe #%d (1 instead of 0)\n",
				   __FUNCTION__, i);
			goto out;
		}
	}

	for (i = 0; i < numPipes; i++)
	{
		/* Test writing from clients to servers */
		ZeroMemory(sndbuf, sizeof(sndbuf));
		ZeroMemory(rcvbuf, sizeof(rcvbuf));
		sprintf_s(sndbuf, sizeof(sndbuf), "CLIENT->SERVER ON PIPE #%05d", i);

		if (!WriteFile(clients[i], sndbuf, sizeof(sndbuf), &dwWritten, NULL) ||
				dwWritten != sizeof(sndbuf))
		{
			printf("%s: Error writing to client end of pipe #%d\n",	__FUNCTION__, i);
			goto out;
		}

		if (!ReadFile(servers[i], rcvbuf, dwWritten, &dwRead, NULL) ||
				dwRead != dwWritten)
		{
			printf("%s: Error reading on server end of pipe #%d\n", __FUNCTION__, i);
			goto out;
		}

		if (memcmp(sndbuf, rcvbuf, sizeof(sndbuf)))
		{
			printf("%s: Error data read on server end of pipe #%d is corrupted\n",
				   __FUNCTION__, i);
			goto out;
		}

		/* Test writing from servers to clients */
		ZeroMemory(sndbuf, sizeof(sndbuf));
		ZeroMemory(rcvbuf, sizeof(rcvbuf));
		sprintf_s(sndbuf, sizeof(sndbuf), "SERVER->CLIENT ON PIPE #%05d", i);

		if (!WriteFile(servers[i], sndbuf, sizeof(sndbuf), &dwWritten, NULL) ||
				dwWritten != sizeof(sndbuf))
		{
			printf("%s: Error writing to server end of pipe #%d\n", __FUNCTION__, i);
			goto out;
		}

		if (!ReadFile(clients[i], rcvbuf, dwWritten, &dwRead, NULL) ||
				dwRead != dwWritten)
		{
			printf("%s: Error reading on client end of pipe #%d\n", __FUNCTION__, i);
			goto out;
		}

		if (memcmp(sndbuf, rcvbuf, sizeof(sndbuf)))
		{
			printf("%s: Error data read on client end of pipe #%d is corrupted\n",
				   __FUNCTION__,  i);
			goto out;
		}
	}

#endif
	/**
	 * After DisconnectNamedPipe on server end
	 * ReadFile/WriteFile must fail on client end
	 */
	i = numPipes - 1;
	DisconnectNamedPipe(servers[i]);

	if (ReadFile(clients[i], rcvbuf, sizeof(rcvbuf), &dwRead, NULL))
	{
		printf("%s: Error ReadFile on client should have failed after DisconnectNamedPipe on server\n", __FUNCTION__);
		goto out;
	}

	if (WriteFile(clients[i], sndbuf, sizeof(sndbuf), &dwWritten, NULL))
	{
		printf("%s: Error WriteFile on client end should have failed after DisconnectNamedPipe on server\n", __FUNCTION__);
		goto out;
	}

	CloseHandle(servers[i]);
	CloseHandle(clients[i]);
	numPipes--;
	/**
	 * After CloseHandle (without calling DisconnectNamedPipe first) on server end
	 * ReadFile/WriteFile must fail on client end
	 */
	i = numPipes - 1;
	CloseHandle(servers[i]);

	if (ReadFile(clients[i], rcvbuf, sizeof(rcvbuf), &dwRead, NULL))
	{
		printf("%s: Error ReadFile on client end should have failed after CloseHandle on server\n", __FUNCTION__);
		goto out;
	}

	if (WriteFile(clients[i], sndbuf, sizeof(sndbuf), &dwWritten, NULL))
	{
		printf("%s: Error WriteFile on client end should have failed after CloseHandle on server\n", __FUNCTION__);
		goto out;
	}

	CloseHandle(clients[i]);
	numPipes--;
	/**
	 * After CloseHandle on client end
	 * ReadFile/WriteFile must fail on server end
	 */
	i = numPipes - 1;
	CloseHandle(clients[i]);

	if (ReadFile(servers[i], rcvbuf, sizeof(rcvbuf), &dwRead, NULL))
	{
		printf("%s: Error ReadFile on server end should have failed after CloseHandle on client\n", __FUNCTION__);
		goto out;
	}

	if (WriteFile(servers[i], sndbuf, sizeof(sndbuf), &dwWritten, NULL))
	{
		printf("%s: Error WriteFile on server end should have failed after CloseHandle on client\n", __FUNCTION__);
		goto out;
	}

	DisconnectNamedPipe(servers[i]);
	CloseHandle(servers[i]);
	numPipes--;

	/* Close all remaining pipes */
	for (i = 0; i < numPipes; i++)
	{
		DisconnectNamedPipe(servers[i]);
		CloseHandle(servers[i]);
		CloseHandle(clients[i]);
	}

	bSuccess = TRUE;
out:

	if (!bSuccess)
		testFailed = TRUE;

	return 0;
}


int TestPipeCreateNamedPipe(int argc, char* argv[])
{
	HANDLE SingleThread;
	HANDLE ClientThread;
	HANDLE ServerThread;
	HANDLE hPipe;

	/* Verify that CreateNamedPipe returns INVALID_HANDLE_VALUE on failure */
	hPipe = CreateNamedPipeA(NULL, 0, 0, 0, 0, 0, 0, NULL);
	if (hPipe != INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe unexpectedly returned %p instead of INVALID_HANDLE_VALUE (%p)\n",
			hPipe, INVALID_HANDLE_VALUE);
		return -1;
	}

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	if (!(ReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		printf("CreateEvent failure: (%"PRIu32")\n", GetLastError());
		return -1;
	}
	if (!(SingleThread = CreateThread(NULL, 0, named_pipe_single_thread, NULL, 0, NULL)))
	{
		printf("CreateThread (SingleThread) failure: (%"PRIu32")\n", GetLastError());
		return -1;
	}
	if (!(ClientThread = CreateThread(NULL, 0, named_pipe_client_thread, NULL, 0, NULL)))
	{
		printf("CreateThread (ClientThread) failure: (%"PRIu32")\n", GetLastError());
		return -1;
	}
	if (!(ServerThread = CreateThread(NULL, 0, named_pipe_server_thread, NULL, 0, NULL)))
	{
		printf("CreateThread (ServerThread) failure: (%"PRIu32")\n", GetLastError());
		return -1;
	}
	WaitForSingleObject(SingleThread, INFINITE);
	WaitForSingleObject(ClientThread, INFINITE);
	WaitForSingleObject(ServerThread, INFINITE);
	CloseHandle(SingleThread);
	CloseHandle(ClientThread);
	CloseHandle(ServerThread);
	return testFailed;
}
