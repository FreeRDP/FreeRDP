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
#include <errno.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "pipe.h"

/*
 * Since the WinPR implementation of named pipes makes use of UNIX domain
 * sockets, it is not possible to bind the same name more than once (i.e.,
 * SO_REUSEADDR does not work with UNIX domain sockets).  As a result, the
 * first call to CreateNamedPipe must create the UNIX domain socket and
 * subsequent calls to CreateNamedPipe will reference the first named pipe
 * handle and duplicate the socket descriptor.
 *
 * The following array keeps track of the named pipe handles for the first
 * instance.  If multiple instances are created, subsequent instances store
 * a pointer to the first instance and a reference count is maintained.  When
 * the last instance is closed, the named pipe handle is removed from the list.
 */

static wArrayList* g_BaseNamedPipeList = NULL;

static BOOL g_Initialized = FALSE;

static void InitWinPRPipeModule()
{
	if (g_Initialized)
		return;

	g_BaseNamedPipeList = ArrayList_New(TRUE);

	g_Initialized = TRUE;
}

void WinPR_RemoveBaseNamedPipeFromList(WINPR_NAMED_PIPE* pNamedPipe)
{
	ArrayList_Remove(g_BaseNamedPipeList, pNamedPipe);
}

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
	int index;
	int status;
	HANDLE hNamedPipe;
	char* lpPipePath;
	struct sockaddr_un s;
	WINPR_NAMED_PIPE* pNamedPipe;
	WINPR_NAMED_PIPE* pBaseNamedPipe;

	if (!lpName)
		return INVALID_HANDLE_VALUE;

	InitWinPRPipeModule();

	/* Find the base named pipe instance (i.e., the first instance). */
	pBaseNamedPipe = NULL;

	ArrayList_Lock(g_BaseNamedPipeList);

	for (index = 0; index < ArrayList_Count(g_BaseNamedPipeList); index++)
	{
		WINPR_NAMED_PIPE* p = (WINPR_NAMED_PIPE*) ArrayList_GetItem(g_BaseNamedPipeList, index);

		if (strcmp(p->name, lpName) == 0)
		{
			pBaseNamedPipe = p;
			break;
		}
	}

	ArrayList_Unlock(g_BaseNamedPipeList);

	pNamedPipe = (WINPR_NAMED_PIPE*) malloc(sizeof(WINPR_NAMED_PIPE));
	hNamedPipe = (HANDLE) pNamedPipe;

	WINPR_HANDLE_SET_TYPE(pNamedPipe, HANDLE_TYPE_NAMED_PIPE);

	pNamedPipe->pfnRemoveBaseNamedPipeFromList = WinPR_RemoveBaseNamedPipeFromList;

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

	pNamedPipe->clientfd = -1;
	pNamedPipe->ServerMode = TRUE;

	pNamedPipe->pBaseNamedPipe = pBaseNamedPipe;
	pNamedPipe->dwRefCount = 1;

	/* If this is the first instance of the named pipe... */
	if (pBaseNamedPipe == NULL)
	{
		/* Create the UNIX domain socket and start listening. */
		lpPipePath = GetNamedPipeUnixDomainSocketBaseFilePathA();

		if (!PathFileExistsA(lpPipePath))
		{
			CreateDirectoryA(lpPipePath, 0);
			UnixChangeFileMode(lpPipePath, 0xFFFF);
		}

		free(lpPipePath);

		if (PathFileExistsA(pNamedPipe->lpFilePath))
		{
			DeleteFileA(pNamedPipe->lpFilePath);
		}

		pNamedPipe->serverfd = socket(AF_UNIX, SOCK_STREAM, 0);

		if (pNamedPipe->serverfd == -1)
		{
			fprintf(stderr, "CreateNamedPipeA: socket error, %s\n", strerror(errno));
			goto err_out;
		}

		ZeroMemory(&s, sizeof(struct sockaddr_un));
		s.sun_family = AF_UNIX;
		strcpy(s.sun_path, pNamedPipe->lpFilePath);

		status = bind(pNamedPipe->serverfd, (struct sockaddr*) &s, sizeof(struct sockaddr_un));

		if (status != 0)
		{
			fprintf(stderr, "CreateNamedPipeA: bind error, %s\n", strerror(errno));
			goto err_out;
		}

		status = listen(pNamedPipe->serverfd, 2);

		if (status != 0)
		{
			fprintf(stderr, "CreateNamedPipeA: listen error, %s\n", strerror(errno));
			goto err_out;
		}

		UnixChangeFileMode(pNamedPipe->lpFilePath, 0xFFFF);

		/* Add the named pipe to the list of base named pipe instances. */
		ArrayList_Add(g_BaseNamedPipeList, pNamedPipe);
	}
	else
	{
		/* Duplicate the file handle for the UNIX domain socket in the first instance. */
		pNamedPipe->serverfd = dup(pBaseNamedPipe->serverfd);

		/* Update the reference count in the base named pipe instance. */
		pBaseNamedPipe->dwRefCount++;
	}

	if (dwOpenMode & FILE_FLAG_OVERLAPPED)
	{
#if 0
		int flags = fcntl(pNamedPipe->serverfd, F_GETFL);

		if (flags != -1)
			fcntl(pNamedPipe->serverfd, F_SETFL, flags | O_NONBLOCK);
#endif
	}

	return hNamedPipe;

err_out:
	if (pNamedPipe) {
		if (pNamedPipe->serverfd != -1)
			close(pNamedPipe->serverfd);
		free(pNamedPipe);
	}
	return INVALID_HANDLE_VALUE;
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

	if (!(pNamedPipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
	{
		length = sizeof(struct sockaddr_un);
		ZeroMemory(&s, sizeof(struct sockaddr_un));

		status = accept(pNamedPipe->serverfd, (struct sockaddr*) &s, &length);

		if (status < 0)
		{
			fprintf(stderr, "ConnectNamedPipe: accept error\n");
			return FALSE;
		}

		pNamedPipe->clientfd = status;
		pNamedPipe->ServerMode = FALSE;
	}
	else
	{
		if (!lpOverlapped)
			return FALSE;

		if (pNamedPipe->serverfd == -1)
			return FALSE;

		pNamedPipe->lpOverlapped = lpOverlapped;

		/* synchronous behavior */

		lpOverlapped->Internal = 2;
		lpOverlapped->InternalHigh = (ULONG_PTR) 0;
		lpOverlapped->Pointer = (PVOID) NULL;

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
	BOOL status;
	DWORD nWaitTime;
	char* lpFilePath;
	DWORD dwSleepInterval;

	if (!lpNamedPipeName)
		return FALSE;

	lpFilePath = GetNamedPipeUnixDomainSocketFilePathA(lpNamedPipeName);

	if (nTimeOut == NMPWAIT_USE_DEFAULT_WAIT)
		nTimeOut = 50;

	nWaitTime = 0;
	status = TRUE;
	dwSleepInterval = 10;

	while (!PathFileExistsA(lpFilePath))
	{
		Sleep(dwSleepInterval);
		nWaitTime += dwSleepInterval;

		if (nWaitTime >= nTimeOut)
		{
			status = FALSE;
			break;
		}
	}
	free(lpFilePath);
	return status;
}

BOOL WaitNamedPipeW(LPCWSTR lpNamedPipeName, DWORD nTimeOut)
{
	return TRUE;
}

BOOL SetNamedPipeHandleState(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout)
{
	int fd;
	int flags;
	WINPR_NAMED_PIPE* pNamedPipe;

	pNamedPipe = (WINPR_NAMED_PIPE*) hNamedPipe;

	if (lpMode)
	{
		pNamedPipe->dwPipeMode = *lpMode;

		fd = (pNamedPipe->ServerMode) ? pNamedPipe->serverfd : pNamedPipe->clientfd;

		if (fd == -1)
			return FALSE;

		flags = fcntl(fd, F_GETFL);

		if (pNamedPipe->dwPipeMode & PIPE_NOWAIT)
			flags = (flags | O_NONBLOCK);
		else
			flags = (flags & ~(O_NONBLOCK));

		fcntl(fd, F_SETFL, flags);
	}

	if (lpMaxCollectionCount)
	{

	}

	if (lpCollectDataTimeout)
	{

	}

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
