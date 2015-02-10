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
#include <assert.h>

#include "pipe.h"

#include "../log.h"
#define TAG WINPR_TAG("pipe")

/*
 * Since the WinPR implementation of named pipes makes use of UNIX domain
 * sockets, it is not possible to bind the same name more than once (i.e.,
 * SO_REUSEADDR does not work with UNIX domain sockets).  As a result, the
 * first call to CreateNamedPipe with name n creates a "shared" UNIX domain
 * socket descriptor that gets duplicated via dup() for the first and all
 * subsequent calls to CreateNamedPipe with name n.
 *
 * The following array keeps track of the references to the shared socked
 * descriptors. If an entry's reference count is zero the base socket
 * descriptor gets closed and the entry is removed from the list.
 */

static wArrayList* g_NamedPipeServerSockets = NULL;

typedef struct _NamedPipeServerSocketEntry
{
	char* name;
	int serverfd;
	int references;
} NamedPipeServerSocketEntry;

static void InitWinPRPipeModule()
{
	if (g_NamedPipeServerSockets)
		return;

	g_NamedPipeServerSockets = ArrayList_New(FALSE);
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
		WLog_ERR(TAG, "failed to create pipe");
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

static void winpr_unref_named_pipe(WINPR_NAMED_PIPE* pNamedPipe)
{
	int index;
	NamedPipeServerSocketEntry* baseSocket;

	if (!pNamedPipe)
		return;

	assert(pNamedPipe->name);
	assert(g_NamedPipeServerSockets);
	//WLog_VRB(TAG, "%p (%s)", pNamedPipe, pNamedPipe->name);
	ArrayList_Lock(g_NamedPipeServerSockets);

	for (index = 0; index < ArrayList_Count(g_NamedPipeServerSockets); index++)
	{
		baseSocket = (NamedPipeServerSocketEntry*) ArrayList_GetItem(
						 g_NamedPipeServerSockets, index);
		assert(baseSocket->name);

		if (!strcmp(baseSocket->name, pNamedPipe->name))
		{
			assert(baseSocket->references > 0);
			assert(baseSocket->serverfd != -1);

			if (--baseSocket->references == 0)
			{
				//WLog_DBG(TAG, "removing shared server socked resource");
				//WLog_DBG(TAG, "closing shared serverfd %d", baseSocket->serverfd);
				ArrayList_Remove(g_NamedPipeServerSockets, baseSocket);
				close(baseSocket->serverfd);
				free(baseSocket->name);
				free(baseSocket);
			}

			break;
		}
	}

	ArrayList_Unlock(g_NamedPipeServerSockets);
}

HANDLE CreateNamedPipeA(LPCSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
						DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	int index;
	HANDLE hNamedPipe = INVALID_HANDLE_VALUE;
	char* lpPipePath;
	struct sockaddr_un s;
	WINPR_NAMED_PIPE* pNamedPipe = NULL;
	int serverfd = -1;
	NamedPipeServerSocketEntry* baseSocket = NULL;

	if (!lpName)
		return INVALID_HANDLE_VALUE;

	InitWinPRPipeModule();
	pNamedPipe = (WINPR_NAMED_PIPE*) calloc(1, sizeof(WINPR_NAMED_PIPE));
	if (!pNamedPipe)
		return INVALID_HANDLE_VALUE;

	WINPR_HANDLE_SET_TYPE(pNamedPipe, HANDLE_TYPE_NAMED_PIPE);

	if (!(pNamedPipe->name = _strdup(lpName)))
		goto out;

	if (!(pNamedPipe->lpFileName = GetNamedPipeNameWithoutPrefixA(lpName)))
		goto out;

	if (!(pNamedPipe->lpFilePath = GetNamedPipeUnixDomainSocketFilePathA(lpName)))
		goto out;

	pNamedPipe->dwOpenMode = dwOpenMode;
	pNamedPipe->dwPipeMode = dwPipeMode;
	pNamedPipe->nMaxInstances = nMaxInstances;
	pNamedPipe->nOutBufferSize = nOutBufferSize;
	pNamedPipe->nInBufferSize = nInBufferSize;
	pNamedPipe->nDefaultTimeOut = nDefaultTimeOut;
	pNamedPipe->dwFlagsAndAttributes = dwOpenMode;
	pNamedPipe->clientfd = -1;
	pNamedPipe->ServerMode = TRUE;
	ArrayList_Lock(g_NamedPipeServerSockets);

	for (index = 0; index < ArrayList_Count(g_NamedPipeServerSockets); index++)
	{
		baseSocket = (NamedPipeServerSocketEntry*) ArrayList_GetItem(
						 g_NamedPipeServerSockets, index);

		if (!strcmp(baseSocket->name, lpName))
		{
			serverfd = baseSocket->serverfd;
			//WLog_DBG(TAG, "using shared socked resource for pipe %p (%s)", pNamedPipe, lpName);
			break;
		}
	}

	/* If this is the first instance of the named pipe... */
	if (serverfd == -1)
	{
		/* Create the UNIX domain socket and start listening. */
		if (!(lpPipePath = GetNamedPipeUnixDomainSocketBaseFilePathA()))
			goto out;

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

		if ((serverfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		{
			WLog_ERR(TAG, "CreateNamedPipeA: socket error, %s", strerror(errno));
			goto out;
		}

		ZeroMemory(&s, sizeof(struct sockaddr_un));
		s.sun_family = AF_UNIX;
		strcpy(s.sun_path, pNamedPipe->lpFilePath);

		if (bind(serverfd, (struct sockaddr*) &s, sizeof(struct sockaddr_un)) == -1)
		{
			WLog_ERR(TAG, "CreateNamedPipeA: bind error, %s", strerror(errno));
			goto out;
		}

		if (listen(serverfd, 2) == -1)
		{
			WLog_ERR(TAG, "CreateNamedPipeA: listen error, %s", strerror(errno));
			goto out;
		}

		UnixChangeFileMode(pNamedPipe->lpFilePath, 0xFFFF);

		if (!(baseSocket = (NamedPipeServerSocketEntry*) malloc(sizeof(NamedPipeServerSocketEntry))))
			goto out;

		if (!(baseSocket->name = _strdup(lpName)))
		{
			free(baseSocket);
			goto out;
		}

		baseSocket->serverfd = serverfd;
		baseSocket->references = 0;
		ArrayList_Add(g_NamedPipeServerSockets, baseSocket);
		//WLog_DBG(TAG, "created shared socked resource for pipe %p (%s). base serverfd = %d", pNamedPipe, lpName, serverfd);
	}

	pNamedPipe->serverfd = dup(baseSocket->serverfd);
	//WLog_DBG(TAG, "using serverfd %d (duplicated from %d)", pNamedPipe->serverfd, baseSocket->serverfd);
	pNamedPipe->pfnUnrefNamedPipe = winpr_unref_named_pipe;
	baseSocket->references++;

	if (dwOpenMode & FILE_FLAG_OVERLAPPED)
	{
#if 0
		int flags = fcntl(pNamedPipe->serverfd, F_GETFL);

		if (flags != -1)
			fcntl(pNamedPipe->serverfd, F_SETFL, flags | O_NONBLOCK);

#endif
	}

	hNamedPipe = (HANDLE) pNamedPipe;
out:

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		if (pNamedPipe)
		{
			free((void*)pNamedPipe->name);
			free((void*)pNamedPipe->lpFileName);
			free((void*)pNamedPipe->lpFilePath);
			free(pNamedPipe);
		}

		if (serverfd != -1)
			close(serverfd);
	}

	ArrayList_Unlock(g_NamedPipeServerSockets);
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

	if (!(pNamedPipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
	{
		length = sizeof(struct sockaddr_un);
		ZeroMemory(&s, sizeof(struct sockaddr_un));
		status = accept(pNamedPipe->serverfd, (struct sockaddr*) &s, &length);

		if (status < 0)
		{
			WLog_ERR(TAG, "ConnectNamedPipe: accept error");
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
	WLog_ERR(TAG, "Not implemented");
	return TRUE;
}

BOOL TransactNamedPipe(HANDLE hNamedPipe, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer,
					   DWORD nOutBufferSize, LPDWORD lpBytesRead, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "Not implemented");
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
	WLog_ERR(TAG, "Not implemented");
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
		if (flags < 0)
			return FALSE;

		if (pNamedPipe->dwPipeMode & PIPE_NOWAIT)
			flags = (flags | O_NONBLOCK);
		else
			flags = (flags & ~(O_NONBLOCK));

		if (fcntl(fd, F_SETFL, flags) < 0)
			return FALSE;
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
	WLog_ERR(TAG, "Not implemented");
	return FALSE;
}

BOOL GetNamedPipeClientComputerNameA(HANDLE Pipe, LPCSTR ClientComputerName, ULONG ClientComputerNameLength)
{
	WLog_ERR(TAG, "Not implemented");
	return FALSE;
}

BOOL GetNamedPipeClientComputerNameW(HANDLE Pipe, LPCWSTR ClientComputerName, ULONG ClientComputerNameLength)
{
	WLog_ERR(TAG, "Not implemented");
	return FALSE;
}

#endif
