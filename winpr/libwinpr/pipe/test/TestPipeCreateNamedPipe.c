
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

#define PIPE_BUFFER_SIZE 32

static HANDLE ReadyEvent;

static LPTSTR lpszPipeNameMt = _T("\\\\.\\pipe\\winpr_test_pipe_mt");
static LPTSTR lpszPipeNameSt = _T("\\\\.\\pipe\\winpr_test_pipe_st");

static BOOL testFailed = FALSE;

static DWORD WINAPI named_pipe_client_thread(LPVOID arg)
{
	HANDLE hNamedPipe = nullptr;
	BYTE* lpReadBuffer = nullptr;
	BYTE* lpWriteBuffer = nullptr;
	BOOL fSuccess = FALSE;
	DWORD nNumberOfBytesToRead = 0;
	DWORD nNumberOfBytesToWrite = 0;
	DWORD lpNumberOfBytesRead = 0;
	DWORD lpNumberOfBytesWritten = 0;
	(void)WaitForSingleObject(ReadyEvent, INFINITE);
	hNamedPipe = CreateFile(lpszPipeNameMt, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
	                        0, nullptr);

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("%s: Named Pipe CreateFile failure: INVALID_HANDLE_VALUE\n", __func__);
		goto out;
	}

	if (!(lpReadBuffer = (BYTE*)malloc(PIPE_BUFFER_SIZE)))
	{
		printf("%s: Error allocating read buffer\n", __func__);
		goto out;
	}

	if (!(lpWriteBuffer = (BYTE*)malloc(PIPE_BUFFER_SIZE)))
	{
		printf("%s: Error allocating write buffer\n", __func__);
		goto out;
	}

	lpNumberOfBytesWritten = 0;
	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;
	FillMemory(lpWriteBuffer, PIPE_BUFFER_SIZE, 0x59);

	if (!WriteFile(hNamedPipe, lpWriteBuffer, nNumberOfBytesToWrite, &lpNumberOfBytesWritten,
	               nullptr) ||
	    lpNumberOfBytesWritten != nNumberOfBytesToWrite)
	{
		printf("%s: Client NamedPipe WriteFile failure\n", __func__);
		goto out;
	}

	lpNumberOfBytesRead = 0;
	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;
	ZeroMemory(lpReadBuffer, PIPE_BUFFER_SIZE);

	if (!ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, &lpNumberOfBytesRead, nullptr) ||
	    lpNumberOfBytesRead != nNumberOfBytesToRead)
	{
		printf("%s: Client NamedPipe ReadFile failure\n", __func__);
		goto out;
	}

	printf("Client ReadFile: %" PRIu32 " bytes\n", lpNumberOfBytesRead);
	winpr_HexDump("pipe.test", WLOG_DEBUG, lpReadBuffer, lpNumberOfBytesRead);
	fSuccess = TRUE;
out:
	free(lpReadBuffer);
	free(lpWriteBuffer);
	(void)CloseHandle(hNamedPipe);

	if (!fSuccess)
		testFailed = TRUE;

	ExitThread(0);
	return 0;
}

static DWORD WINAPI named_pipe_server_thread(LPVOID arg)
{
	HANDLE hNamedPipe = nullptr;
	BYTE* lpReadBuffer = nullptr;
	BYTE* lpWriteBuffer = nullptr;
	BOOL fSuccess = FALSE;
	BOOL fConnected = FALSE;
	DWORD nNumberOfBytesToRead = 0;
	DWORD nNumberOfBytesToWrite = 0;
	DWORD lpNumberOfBytesRead = 0;
	DWORD lpNumberOfBytesWritten = 0;
	hNamedPipe = CreateNamedPipe(
	    lpszPipeNameMt, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
	    PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 0, nullptr);

	if (!hNamedPipe)
	{
		printf("%s: CreateNamedPipe failure: nullptr handle\n", __func__);
		goto out;
	}

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		printf("%s: CreateNamedPipe failure: INVALID_HANDLE_VALUE\n", __func__);
		goto out;
	}

	(void)SetEvent(ReadyEvent);

	/**
	 * Note:
	 * If a client connects before ConnectNamedPipe is called, the function returns zero and
	 * GetLastError returns ERROR_PIPE_CONNECTED. This can happen if a client connects in the
	 * interval between the call to CreateNamedPipe and the call to ConnectNamedPipe.
	 * In this situation, there is a good connection between client and server, even though
	 * the function returns zero.
	 */
	fConnected =
	    ConnectNamedPipe(hNamedPipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

	if (!fConnected)
	{
		printf("%s: ConnectNamedPipe failure\n", __func__);
		goto out;
	}

	if (!(lpReadBuffer = (BYTE*)calloc(1, PIPE_BUFFER_SIZE)))
	{
		printf("%s: Error allocating read buffer\n", __func__);
		goto out;
	}

	if (!(lpWriteBuffer = (BYTE*)malloc(PIPE_BUFFER_SIZE)))
	{
		printf("%s: Error allocating write buffer\n", __func__);
		goto out;
	}

	lpNumberOfBytesRead = 0;
	nNumberOfBytesToRead = PIPE_BUFFER_SIZE;

	if (!ReadFile(hNamedPipe, lpReadBuffer, nNumberOfBytesToRead, &lpNumberOfBytesRead, nullptr) ||
	    lpNumberOfBytesRead != nNumberOfBytesToRead)
	{
		printf("%s: Server NamedPipe ReadFile failure\n", __func__);
		goto out;
	}

	printf("Server ReadFile: %" PRIu32 " bytes\n", lpNumberOfBytesRead);
	winpr_HexDump("pipe.test", WLOG_DEBUG, lpReadBuffer, lpNumberOfBytesRead);
	lpNumberOfBytesWritten = 0;
	nNumberOfBytesToWrite = PIPE_BUFFER_SIZE;
	FillMemory(lpWriteBuffer, PIPE_BUFFER_SIZE, 0x45);

	if (!WriteFile(hNamedPipe, lpWriteBuffer, nNumberOfBytesToWrite, &lpNumberOfBytesWritten,
	               nullptr) ||
	    lpNumberOfBytesWritten != nNumberOfBytesToWrite)
	{
		printf("%s: Server NamedPipe WriteFile failure\n", __func__);
		goto out;
	}

	fSuccess = TRUE;
out:
	free(lpReadBuffer);
	free(lpWriteBuffer);
	(void)CloseHandle(hNamedPipe);

	if (!fSuccess)
		testFailed = TRUE;

	ExitThread(0);
	return 0;
}

#define TESTNUMPIPESST 16
static DWORD WINAPI named_pipe_single_thread(LPVOID arg)
{
	HANDLE servers[TESTNUMPIPESST] = WINPR_C_ARRAY_INIT;
	HANDLE clients[TESTNUMPIPESST] = WINPR_C_ARRAY_INIT;
	DWORD dwRead = 0;
	DWORD dwWritten = 0;
	int numPipes = 0;
	BOOL bSuccess = FALSE;
	numPipes = TESTNUMPIPESST;
	(void)WaitForSingleObject(ReadyEvent, INFINITE);

	for (int i = 0; i < numPipes; i++)
	{
		if (!(servers[i] = CreateNamedPipe(lpszPipeNameSt, PIPE_ACCESS_DUPLEX,
		                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		                                   PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE,
		                                   PIPE_BUFFER_SIZE, 0, nullptr)))
		{
			printf("%s: CreateNamedPipe #%d failed\n", __func__, i);
			goto out;
		}
	}

#ifndef _WIN32

	for (int i = 0; i < numPipes; i++)
	{
		WINPR_NAMED_PIPE* p = (WINPR_NAMED_PIPE*)servers[i];

		if (strcmp(lpszPipeNameSt, p->name) != 0)
		{
			printf("%s: Pipe name mismatch for pipe #%d ([%s] instead of [%s])\n", __func__, i,
			       p->name, lpszPipeNameSt);
			goto out;
		}

		if (p->clientfd != -1)
		{
			printf("%s: Unexpected client fd value for pipe #%d (%d instead of -1)\n", __func__, i,
			       p->clientfd);
			goto out;
		}

		if (p->serverfd < 1)
		{
			printf("%s: Unexpected server fd value for pipe #%d (%d is not > 0)\n", __func__, i,
			       p->serverfd);
			goto out;
		}

		if (p->ServerMode == FALSE)
		{
			printf("%s: Unexpected ServerMode value for pipe #%d (0 instead of 1)\n", __func__, i);
			goto out;
		}
	}

#endif

	for (int i = 0; i < numPipes; i++)
	{
		BOOL fConnected = 0;
		if ((clients[i] = CreateFile(lpszPipeNameSt, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		                             OPEN_EXISTING, 0, nullptr)) == INVALID_HANDLE_VALUE)
		{
			printf("%s: CreateFile #%d failed\n", __func__, i);
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
		fConnected =
		    ConnectNamedPipe(servers[i], nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (!fConnected)
		{
			printf("%s: ConnectNamedPipe #%d failed. (%" PRIu32 ")\n", __func__, i, GetLastError());
			goto out;
		}
	}

#ifndef _WIN32

	for (int i = 0; i < numPipes; i++)
	{
		WINPR_NAMED_PIPE* p = servers[i];

		if (p->clientfd < 1)
		{
			printf("%s: Unexpected client fd value for pipe #%d (%d is not > 0)\n", __func__, i,
			       p->clientfd);
			goto out;
		}

		if (p->ServerMode)
		{
			printf("%s: Unexpected ServerMode value for pipe #%d (1 instead of 0)\n", __func__, i);
			goto out;
		}
	}

	for (int i = 0; i < numPipes; i++)
	{
		{
			char sndbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
			char rcvbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
			/* Test writing from clients to servers */
			(void)sprintf_s(sndbuf, sizeof(sndbuf), "CLIENT->SERVER ON PIPE #%05d", i);

			if (!WriteFile(clients[i], sndbuf, sizeof(sndbuf), &dwWritten, nullptr) ||
			    dwWritten != sizeof(sndbuf))
			{
				printf("%s: Error writing to client end of pipe #%d\n", __func__, i);
				goto out;
			}

			if (!ReadFile(servers[i], rcvbuf, dwWritten, &dwRead, nullptr) || dwRead != dwWritten)
			{
				printf("%s: Error reading on server end of pipe #%d\n", __func__, i);
				goto out;
			}

			if (memcmp(sndbuf, rcvbuf, sizeof(sndbuf)) != 0)
			{
				printf("%s: Error data read on server end of pipe #%d is corrupted\n", __func__, i);
				goto out;
			}
		}
		{

			char sndbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
			char rcvbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
			/* Test writing from servers to clients */

			(void)sprintf_s(sndbuf, sizeof(sndbuf), "SERVER->CLIENT ON PIPE #%05d", i);

			if (!WriteFile(servers[i], sndbuf, sizeof(sndbuf), &dwWritten, nullptr) ||
			    dwWritten != sizeof(sndbuf))
			{
				printf("%s: Error writing to server end of pipe #%d\n", __func__, i);
				goto out;
			}

			if (!ReadFile(clients[i], rcvbuf, dwWritten, &dwRead, nullptr) || dwRead != dwWritten)
			{
				printf("%s: Error reading on client end of pipe #%d\n", __func__, i);
				goto out;
			}

			if (memcmp(sndbuf, rcvbuf, sizeof(sndbuf)) != 0)
			{
				printf("%s: Error data read on client end of pipe #%d is corrupted\n", __func__, i);
				goto out;
			}
		}
	}

#endif
	/**
	 * After DisconnectNamedPipe on server end
	 * ReadFile/WriteFile must fail on client end
	 */
	int i = numPipes - 1;
	DisconnectNamedPipe(servers[i]);
	{
		char sndbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
		char rcvbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
		if (ReadFile(clients[i], rcvbuf, sizeof(rcvbuf), &dwRead, nullptr))
		{
			printf("%s: Error ReadFile on client should have failed after DisconnectNamedPipe on "
			       "server\n",
			       __func__);
			goto out;
		}

		if (WriteFile(clients[i], sndbuf, sizeof(sndbuf), &dwWritten, nullptr))
		{
			printf(
			    "%s: Error WriteFile on client end should have failed after DisconnectNamedPipe on "
			    "server\n",
			    __func__);
			goto out;
		}
	}
	(void)CloseHandle(servers[i]);
	(void)CloseHandle(clients[i]);
	numPipes--;
	/**
	 * After CloseHandle (without calling DisconnectNamedPipe first) on server end
	 * ReadFile/WriteFile must fail on client end
	 */
	i = numPipes - 1;
	(void)CloseHandle(servers[i]);

	{
		char sndbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
		char rcvbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;

		if (ReadFile(clients[i], rcvbuf, sizeof(rcvbuf), &dwRead, nullptr))
		{
			printf(
			    "%s: Error ReadFile on client end should have failed after CloseHandle on server\n",
			    __func__);
			goto out;
		}

		if (WriteFile(clients[i], sndbuf, sizeof(sndbuf), &dwWritten, nullptr))
		{
			printf("%s: Error WriteFile on client end should have failed after CloseHandle on "
			       "server\n",
			       __func__);
			goto out;
		}
	}
	(void)CloseHandle(clients[i]);
	numPipes--;
	/**
	 * After CloseHandle on client end
	 * ReadFile/WriteFile must fail on server end
	 */
	i = numPipes - 1;
	(void)CloseHandle(clients[i]);

	{
		char sndbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;
		char rcvbuf[PIPE_BUFFER_SIZE] = WINPR_C_ARRAY_INIT;

		if (ReadFile(servers[i], rcvbuf, sizeof(rcvbuf), &dwRead, nullptr))
		{
			printf(
			    "%s: Error ReadFile on server end should have failed after CloseHandle on client\n",
			    __func__);
			goto out;
		}

		if (WriteFile(servers[i], sndbuf, sizeof(sndbuf), &dwWritten, nullptr))
		{
			printf("%s: Error WriteFile on server end should have failed after CloseHandle on "
			       "client\n",
			       __func__);
			goto out;
		}
	}

	DisconnectNamedPipe(servers[i]);
	(void)CloseHandle(servers[i]);
	numPipes--;

	/* Close all remaining pipes */
	for (int i = 0; i < numPipes; i++)
	{
		DisconnectNamedPipe(servers[i]);
		(void)CloseHandle(servers[i]);
		(void)CloseHandle(clients[i]);
	}

	bSuccess = TRUE;
out:

	if (!bSuccess)
		testFailed = TRUE;

	return 0;
}

int TestPipeCreateNamedPipe(int argc, char* argv[])
{
	HANDLE SingleThread = nullptr;
	HANDLE ClientThread = nullptr;
	HANDLE ServerThread = nullptr;
	HANDLE hPipe = nullptr;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	/* Verify that CreateNamedPipe returns INVALID_HANDLE_VALUE on failure */
	hPipe = CreateNamedPipeA(nullptr, 0, 0, 0, 0, 0, 0, nullptr);
	if (hPipe != INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe unexpectedly returned %p instead of INVALID_HANDLE_VALUE (%p)\n",
		       hPipe, INVALID_HANDLE_VALUE);
		return -1;
	}

#ifndef _WIN32
	(void)signal(SIGPIPE, SIG_IGN);
#endif
	if (!(ReadyEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr)))
	{
		printf("CreateEvent failure: (%" PRIu32 ")\n", GetLastError());
		return -1;
	}
	if (!(SingleThread = CreateThread(nullptr, 0, named_pipe_single_thread, nullptr, 0, nullptr)))
	{
		printf("CreateThread (SingleThread) failure: (%" PRIu32 ")\n", GetLastError());
		return -1;
	}
	if (!(ClientThread = CreateThread(nullptr, 0, named_pipe_client_thread, nullptr, 0, nullptr)))
	{
		printf("CreateThread (ClientThread) failure: (%" PRIu32 ")\n", GetLastError());
		return -1;
	}
	if (!(ServerThread = CreateThread(nullptr, 0, named_pipe_server_thread, nullptr, 0, nullptr)))
	{
		printf("CreateThread (ServerThread) failure: (%" PRIu32 ")\n", GetLastError());
		return -1;
	}
	(void)WaitForSingleObject(SingleThread, INFINITE);
	(void)WaitForSingleObject(ClientThread, INFINITE);
	(void)WaitForSingleObject(ServerThread, INFINITE);
	(void)CloseHandle(SingleThread);
	(void)CloseHandle(ClientThread);
	(void)CloseHandle(ServerThread);
	return testFailed;
}
