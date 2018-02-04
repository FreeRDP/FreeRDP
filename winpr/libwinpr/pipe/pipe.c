/**
 * WinPR: Windows Portable Runtime
 * Pipe Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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
#include <sys/socket.h>
#include <assert.h>
#include <unistd.h>

#ifdef HAVE_SYS_AIO_H
#undef HAVE_SYS_AIO_H /* disable for now, incomplete */
#endif

#ifdef HAVE_SYS_AIO_H
#include <aio.h>
#endif

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


static BOOL PipeIsHandled(HANDLE handle)
{
	WINPR_PIPE* pPipe = (WINPR_PIPE*) handle;

	if (!pPipe || (pPipe->Type != HANDLE_TYPE_ANONYMOUS_PIPE))
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

static int PipeGetFd(HANDLE handle)
{
	WINPR_PIPE* pipe = (WINPR_PIPE*)handle;

	if (!PipeIsHandled(handle))
		return -1;

	return pipe->fd;
}

static BOOL PipeCloseHandle(HANDLE handle)
{
	WINPR_PIPE* pipe = (WINPR_PIPE*)handle;

	if (!PipeIsHandled(handle))
		return FALSE;

	if (pipe->fd != -1)
	{
		close(pipe->fd);
		pipe->fd = -1;
	}

	free(handle);
	return TRUE;
}

static BOOL PipeRead(PVOID Object, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                     LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	int io_status;
	WINPR_PIPE* pipe;
	BOOL status = TRUE;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	pipe = (WINPR_PIPE*)Object;

	do
	{
		io_status = read(pipe->fd, lpBuffer, nNumberOfBytesToRead);
	}
	while ((io_status < 0) && (errno == EINTR));

	if (io_status < 0)
	{
		status = FALSE;

		switch (errno)
		{
			case EWOULDBLOCK:
				SetLastError(ERROR_NO_DATA);
				break;
		}
	}

	if (lpNumberOfBytesRead)
		*lpNumberOfBytesRead = io_status;

	return status;
}

static BOOL PipeWrite(PVOID Object, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                      LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	int io_status;
	WINPR_PIPE* pipe;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	pipe = (WINPR_PIPE*)Object;

	do
	{
		io_status = write(pipe->fd, lpBuffer, nNumberOfBytesToWrite);
	}
	while ((io_status < 0) && (errno == EINTR));

	if ((io_status < 0) && (errno == EWOULDBLOCK))
		io_status = 0;

	*lpNumberOfBytesWritten = io_status;
	return TRUE;
}


static HANDLE_OPS ops =
{
	PipeIsHandled,
	PipeCloseHandle,
	PipeGetFd,
	NULL, /* CleanupHandle */
	PipeRead,
	NULL, /* FileReadEx */
	NULL, /* FileReadScatter */
	PipeWrite,
	NULL, /* FileWriteEx */
	NULL, /* FileWriteGather */
	NULL, /* FileGetFileSize */
	NULL, /*  FlushFileBuffers */
	NULL, /* FileSetEndOfFile */
	NULL, /* FileSetFilePointer */
	NULL, /* SetFilePointerEx */
	NULL, /* FileLockFile */
	NULL, /* FileLockFileEx */
	NULL, /* FileUnlockFile */
	NULL, /* FileUnlockFileEx */
	NULL  /* SetFileTime */
};



static BOOL NamedPipeIsHandled(HANDLE handle)
{
	WINPR_NAMED_PIPE* pPipe = (WINPR_NAMED_PIPE*) handle;

	if (!pPipe || (pPipe->Type != HANDLE_TYPE_NAMED_PIPE) || (pPipe == INVALID_HANDLE_VALUE))
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

static int NamedPipeGetFd(HANDLE handle)
{
	WINPR_NAMED_PIPE* pipe = (WINPR_NAMED_PIPE*)handle;

	if (!NamedPipeIsHandled(handle))
		return -1;

	if (pipe->ServerMode)
		return pipe->serverfd;

	return pipe->clientfd;
}

static BOOL NamedPipeCloseHandle(HANDLE handle)
{
	WINPR_NAMED_PIPE* pNamedPipe = (WINPR_NAMED_PIPE*)handle;

	if (!NamedPipeIsHandled(handle))
		return FALSE;

	if (pNamedPipe->pfnUnrefNamedPipe)
		pNamedPipe->pfnUnrefNamedPipe(pNamedPipe);

	free(pNamedPipe->name);
	free(pNamedPipe->lpFileName);
	free(pNamedPipe->lpFilePath);

	if (pNamedPipe->serverfd != -1)
		close(pNamedPipe->serverfd);

	if (pNamedPipe->clientfd != -1)
		close(pNamedPipe->clientfd);

	free(pNamedPipe);
	return TRUE;
}

BOOL NamedPipeRead(PVOID Object, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                   LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	int io_status;
	WINPR_NAMED_PIPE* pipe;
	BOOL status = TRUE;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	pipe = (WINPR_NAMED_PIPE*)Object;

	if (!(pipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
	{
		if (pipe->clientfd == -1)
			return FALSE;

		do
		{
			io_status = read(pipe->clientfd, lpBuffer, nNumberOfBytesToRead);
		}
		while ((io_status < 0) && (errno == EINTR));

		if (io_status == 0)
		{
			SetLastError(ERROR_BROKEN_PIPE);
			status = FALSE;
		}
		else if (io_status < 0)
		{
			status = FALSE;

			switch (errno)
			{
				case EWOULDBLOCK:
					SetLastError(ERROR_NO_DATA);
					break;

				default:
					SetLastError(ERROR_BROKEN_PIPE);
					break;
			}
		}

		if (lpNumberOfBytesRead)
			*lpNumberOfBytesRead = io_status;
	}
	else
	{
		/* Overlapped I/O */
		if (!lpOverlapped)
			return FALSE;

		if (pipe->clientfd == -1)
			return FALSE;

		pipe->lpOverlapped = lpOverlapped;
#ifdef HAVE_SYS_AIO_H
		{
			int aio_status;
			struct aiocb cb;
			ZeroMemory(&cb, sizeof(struct aiocb));
			cb.aio_fildes = pipe->clientfd;
			cb.aio_buf = lpBuffer;
			cb.aio_nbytes = nNumberOfBytesToRead;
			cb.aio_offset = lpOverlapped->Offset;
			cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
			cb.aio_sigevent.sigev_signo = SIGIO;
			cb.aio_sigevent.sigev_value.sival_ptr = (void*) lpOverlapped;
			InstallAioSignalHandler();
			aio_status = aio_read(&cb);
			WLog_DBG(TAG, "aio_read status: %d", aio_status);

			if (aio_status < 0)
				status = FALSE;

			return status;
		}
#else
		/* synchronous behavior */
		lpOverlapped->Internal = 0;
		lpOverlapped->InternalHigh = (ULONG_PTR) nNumberOfBytesToRead;
		lpOverlapped->Pointer = (PVOID) lpBuffer;
		SetEvent(lpOverlapped->hEvent);
#endif
	}

	return status;
}

BOOL NamedPipeWrite(PVOID Object, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                    LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	int io_status;
	WINPR_NAMED_PIPE* pipe;
	BOOL status = TRUE;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	pipe = (WINPR_NAMED_PIPE*) Object;

	if (!(pipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
	{
		if (pipe->clientfd == -1)
			return FALSE;

		do
		{
			io_status = write(pipe->clientfd, lpBuffer, nNumberOfBytesToWrite);
		}
		while ((io_status < 0) && (errno == EINTR));

		if (io_status < 0)
		{
			*lpNumberOfBytesWritten = 0;

			switch (errno)
			{
				case EWOULDBLOCK:
					io_status = 0;
					status = TRUE;
					break;

				default:
					status = FALSE;
			}
		}

		*lpNumberOfBytesWritten = io_status;
		return status;
	}
	else
	{
		/* Overlapped I/O */
		if (!lpOverlapped)
			return FALSE;

		if (pipe->clientfd == -1)
			return FALSE;

		pipe->lpOverlapped = lpOverlapped;
#ifdef HAVE_SYS_AIO_H
		{
			struct aiocb cb;
			ZeroMemory(&cb, sizeof(struct aiocb));
			cb.aio_fildes = pipe->clientfd;
			cb.aio_buf = (void*) lpBuffer;
			cb.aio_nbytes = nNumberOfBytesToWrite;
			cb.aio_offset = lpOverlapped->Offset;
			cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
			cb.aio_sigevent.sigev_signo = SIGIO;
			cb.aio_sigevent.sigev_value.sival_ptr = (void*) lpOverlapped;
			InstallAioSignalHandler();
			io_status = aio_write(&cb);
			WLog_DBG("aio_write status: %d", io_status);

			if (io_status < 0)
				status = FALSE;

			return status;
		}
#else
		/* synchronous behavior */
		lpOverlapped->Internal = 1;
		lpOverlapped->InternalHigh = (ULONG_PTR) nNumberOfBytesToWrite;
		lpOverlapped->Pointer = (PVOID) lpBuffer;
		SetEvent(lpOverlapped->hEvent);
#endif
	}

	return TRUE;
}

static HANDLE_OPS namedOps =
{
	NamedPipeIsHandled,
	NamedPipeCloseHandle,
	NamedPipeGetFd,
	NULL, /* CleanupHandle */
	NamedPipeRead,
	NULL,
	NULL,
	NamedPipeWrite
};

static BOOL InitWinPRPipeModule()
{
	if (g_NamedPipeServerSockets)
		return TRUE;

	g_NamedPipeServerSockets = ArrayList_New(FALSE);
	return g_NamedPipeServerSockets != NULL;
}

/*
 * Unnamed pipe
 */

BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes,
                DWORD nSize)
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

	pReadPipe = (WINPR_PIPE*) calloc(1, sizeof(WINPR_PIPE));
	pWritePipe = (WINPR_PIPE*) calloc(1, sizeof(WINPR_PIPE));

	if (!pReadPipe || !pWritePipe)
	{
		free(pReadPipe);
		free(pWritePipe);
		return FALSE;
	}

	pReadPipe->fd = pipe_fd[0];
	pWritePipe->fd = pipe_fd[1];
	WINPR_HANDLE_SET_TYPE_AND_MODE(pReadPipe, HANDLE_TYPE_ANONYMOUS_PIPE, WINPR_FD_READ);
	pReadPipe->ops = &ops;
	*((ULONG_PTR*) hReadPipe) = (ULONG_PTR) pReadPipe;
	WINPR_HANDLE_SET_TYPE_AND_MODE(pWritePipe, HANDLE_TYPE_ANONYMOUS_PIPE, WINPR_FD_READ);
	pWritePipe->ops = &ops;
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
	//WLog_VRB(TAG, "%p (%s)", (void*) pNamedPipe, pNamedPipe->name);
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
                        DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                        LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	int index;
	char* lpPipePath;
	struct sockaddr_un s;
	WINPR_NAMED_PIPE* pNamedPipe = NULL;
	int serverfd = -1;
	NamedPipeServerSocketEntry* baseSocket = NULL;

	if (dwOpenMode & FILE_FLAG_OVERLAPPED)
	{
		WLog_ERR(TAG, "WinPR %s does not support the FILE_FLAG_OVERLAPPED flag", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return INVALID_HANDLE_VALUE;
	}

	if (!lpName)
		return INVALID_HANDLE_VALUE;

	if (!InitWinPRPipeModule())
		return INVALID_HANDLE_VALUE;

	pNamedPipe = (WINPR_NAMED_PIPE*) calloc(1, sizeof(WINPR_NAMED_PIPE));

	if (!pNamedPipe)
		return INVALID_HANDLE_VALUE;

	ArrayList_Lock(g_NamedPipeServerSockets);
	WINPR_HANDLE_SET_TYPE_AND_MODE(pNamedPipe, HANDLE_TYPE_NAMED_PIPE, WINPR_FD_READ);
	pNamedPipe->serverfd = -1;
	pNamedPipe->clientfd = -1;

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
	pNamedPipe->ops = &namedOps;

	for (index = 0; index < ArrayList_Count(g_NamedPipeServerSockets); index++)
	{
		baseSocket = (NamedPipeServerSocketEntry*) ArrayList_GetItem(
		                 g_NamedPipeServerSockets, index);

		if (!strcmp(baseSocket->name, lpName))
		{
			serverfd = baseSocket->serverfd;
			//WLog_DBG(TAG, "using shared socked resource for pipe %p (%s)", (void*) pNamedPipe, lpName);
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
			if (!CreateDirectoryA(lpPipePath, 0))
			{
				free(lpPipePath);
				goto out;
			}

			UnixChangeFileMode(lpPipePath, 0xFFFF);
		}

		free(lpPipePath);

		if (PathFileExistsA(pNamedPipe->lpFilePath))
			DeleteFileA(pNamedPipe->lpFilePath);

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

		if (ArrayList_Add(g_NamedPipeServerSockets, baseSocket) < 0)
		{
			free(baseSocket->name);
			goto out;
		}

		//WLog_DBG(TAG, "created shared socked resource for pipe %p (%s). base serverfd = %d", (void*) pNamedPipe, lpName, serverfd);
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

	ArrayList_Unlock(g_NamedPipeServerSockets);
	return pNamedPipe;
out:
	NamedPipeCloseHandle(pNamedPipe);

	if (serverfd != -1)
		close(serverfd);

	ArrayList_Unlock(g_NamedPipeServerSockets);
	return INVALID_HANDLE_VALUE;
}

HANDLE CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                        DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                        LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	WLog_ERR(TAG, "%s is not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

BOOL ConnectNamedPipe(HANDLE hNamedPipe, LPOVERLAPPED lpOverlapped)
{
	int status;
	socklen_t length;
	struct sockaddr_un s;
	WINPR_NAMED_PIPE* pNamedPipe;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

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
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL TransactNamedPipe(HANDLE hNamedPipe, LPVOID lpInBuffer, DWORD nInBufferSize,
                       LPVOID lpOutBuffer,
                       DWORD nOutBufferSize, LPDWORD lpBytesRead, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
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

	if (!lpFilePath)
		return FALSE;

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
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL SetNamedPipeHandleState(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount,
                             LPDWORD lpCollectDataTimeout)
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
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetNamedPipeClientComputerNameA(HANDLE Pipe, LPCSTR ClientComputerName,
                                     ULONG ClientComputerNameLength)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetNamedPipeClientComputerNameW(HANDLE Pipe, LPCWSTR ClientComputerName,
                                     ULONG ClientComputerNameLength)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

#endif
