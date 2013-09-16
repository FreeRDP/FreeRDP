/**
 * WinPR: Windows Portable Runtime
 * Pipe Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/handle.h>

#include <winpr/pipe.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef _WIN32

#include "../handle/handle.h"

#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "pipe.h"

/*
 * Unnamed pipe
 */

BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize)
{
	int pipe_fd[2];
	WINPR_PIPE* pReadPipe;
	WINPR_PIPE* pWritePipe;

	pipe_fd[0] = -1;
	pipe_fd[1] = -1;

	if (pipe(pipe_fd) < 0)
	{
		printf("CreatePipe: failed to create pipe\n");
		return FALSE;
	}

	pReadPipe = (WINPR_PIPE*) malloc(sizeof(WINPR_PIPE));
	pWritePipe = (WINPR_PIPE*) malloc(sizeof(WINPR_PIPE));

	if (!pReadPipe || !pWritePipe)
	{
		if (pReadPipe)
			free(pReadPipe);
		if (pWritePipe)
			free(pWritePipe);
		return FALSE;
	}

	pReadPipe->fd = pipe_fd[0];
	pWritePipe->fd = pipe_fd[1];

	WINPR_HANDLE_SET_TYPE(pReadPipe, HANDLE_TYPE_ANONYMOUS_PIPE);
	*((ULONG_PTR*) hReadPipe) = (ULONG_PTR) pReadPipe;

	WINPR_HANDLE_SET_TYPE(pWritePipe, HANDLE_TYPE_ANONYMOUS_PIPE);
	*((ULONG_PTR*) hWritePipe) = (ULONG_PTR) pWritePipe;

	return TRUE;
}

/**
 * Named pipe
 */

HANDLE CreateNamedPipeA(LPCSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
		DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	int status;
	HANDLE hNamedPipe;
	char* lpPipePath;
	unsigned long flags;
	struct sockaddr_un s;
	WINPR_NAMED_PIPE* pNamedPipe;

	if (!lpName)
		return INVALID_HANDLE_VALUE;

	pNamedPipe = (WINPR_NAMED_PIPE*) malloc(sizeof(WINPR_NAMED_PIPE));
	hNamedPipe = (HANDLE) pNamedPipe;

	WINPR_HANDLE_SET_TYPE(pNamedPipe, HANDLE_TYPE_NAMED_PIPE);

	pNamedPipe->name = _strdup(lpName);
	pNamedPipe->dwOpenMode = dwOpenMode;
	pNamedPipe->dwPipeMode = dwPipeMode;
	pNamedPipe->nMaxInstances = nMaxInstances;
	pNamedPipe->nOutBufferSize = nOutBufferSize;
	pNamedPipe->nInBufferSize = nInBufferSize;
	pNamedPipe->nDefaultTimeOut = nDefaultTimeOut;
	pNamedPipe->dwFlagsAndAttributes = dwOpenMode;

	pNamedPipe->lpFileName = GetNamedPipeNameWithoutPrefixA(lpName);
	pNamedPipe->lpFilePath = GetNamedPipeUnixDomainSocketFilePathA(lpName);

	lpPipePath = GetNamedPipeUnixDomainSocketBaseFilePathA();

	if (!PathFileExistsA(lpPipePath))
		CreateDirectoryA(lpPipePath, 0);

	free(lpPipePath);

	pNamedPipe->clientfd = -1;
	pNamedPipe->serverfd = socket(PF_LOCAL, SOCK_STREAM, 0);

	if (0)
	{
		flags = fcntl(pNamedPipe->serverfd, F_GETFL);
		flags = flags | O_NONBLOCK;
		fcntl(pNamedPipe->serverfd, F_SETFL, flags);
	}

	ZeroMemory(&s, sizeof(struct sockaddr_un));
	s.sun_family = AF_UNIX;
	strcpy(s.sun_path, pNamedPipe->lpFilePath);
	unlink(s.sun_path);

	status = bind(pNamedPipe->serverfd, (struct sockaddr*) &s, sizeof(struct sockaddr_un));

	if (status == 0)
	{
		status = listen(pNamedPipe->serverfd, 2);

		if (status == 0)
		{
			UnixChangeFileMode(pNamedPipe->lpFilePath, 0xFFFF);
		}
	}

	return hNamedPipe;
}

HANDLE CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
		DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	return NULL;
}

BOOL ConnectNamedPipe(HANDLE hNamedPipe, LPOVERLAPPED lpOverlapped)
{
	int status;
	socklen_t length;
	struct sockaddr_un s;
	WINPR_NAMED_PIPE* pNamedPipe;

	if (!hNamedPipe)
		return FALSE;

	pNamedPipe = (WINPR_NAMED_PIPE*) hNamedPipe;

	length = sizeof(struct sockaddr_un);
	ZeroMemory(&s, sizeof(struct sockaddr_un));

	status = accept(pNamedPipe->serverfd, (struct sockaddr*) &s, &length);

	if (status < 0)
		return FALSE;

	pNamedPipe->clientfd = status;

	if (pNamedPipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED)
	{
		if (!lpOverlapped)
			return FALSE;

		SetEvent(lpOverlapped->hEvent);
	}

	return TRUE;
}

BOOL DisconnectNamedPipe(HANDLE hNamedPipe)
{
	WINPR_NAMED_PIPE* pNamedPipe;

	pNamedPipe = (WINPR_NAMED_PIPE*) hNamedPipe;

	if (pNamedPipe->clientfd != -1)
	{
		close(pNamedPipe->clientfd);
		pNamedPipe->clientfd = -1;
	}

	return TRUE;
}

BOOL PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize,
		LPDWORD lpBytesRead, LPDWORD lpTotalBytesAvail, LPDWORD lpBytesLeftThisMessage)
{
	return TRUE;
}

BOOL TransactNamedPipe(HANDLE hNamedPipe, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer,
		DWORD nOutBufferSize, LPDWORD lpBytesRead, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

BOOL WaitNamedPipeA(LPCSTR lpNamedPipeName, DWORD nTimeOut)
{
	return TRUE;
}

BOOL WaitNamedPipeW(LPCWSTR lpNamedPipeName, DWORD nTimeOut)
{
	return TRUE;
}

BOOL SetNamedPipeHandleState(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout)
{
	WINPR_NAMED_PIPE* pNamedPipe;

	pNamedPipe = (WINPR_NAMED_PIPE*) hNamedPipe;

	if (lpMode)
		pNamedPipe->dwPipeMode = *lpMode;

	return TRUE;
}

BOOL ImpersonateNamedPipeClient(HANDLE hNamedPipe)
{
	return FALSE;
}

BOOL GetNamedPipeClientComputerNameA(HANDLE Pipe, LPCSTR ClientComputerName, ULONG ClientComputerNameLength)
{
	return FALSE;
}

BOOL GetNamedPipeClientComputerNameW(HANDLE Pipe, LPCWSTR ClientComputerName, ULONG ClientComputerNameLength)
{
	return FALSE;
}

#endif
