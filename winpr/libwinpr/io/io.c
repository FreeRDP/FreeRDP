/**
 * WinPR: Windows Portable Runtime
 * Asynchronous I/O Functions
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

#include <winpr/io.h>

#ifndef _WIN32

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>

#include "../handle/handle.h"
#include "../pipe/pipe.h"
#include "../log.h"

#define TAG WINPR_TAG("io")

BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
{
#if 1
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
#else
	ULONG Type;
	WINPR_HANDLE* Object;

	if (!winpr_Handle_GetInfo(hFile, &Type, &Object))
		return FALSE;

	else if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		int status = -1;
		DWORD request;
		PVOID lpBuffer;
		DWORD nNumberOfBytes;
		WINPR_NAMED_PIPE* pipe;

		pipe = (WINPR_NAMED_PIPE*) Object;

		if (!(pipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
			return FALSE;

		lpBuffer = lpOverlapped->Pointer;
		request = (DWORD) lpOverlapped->Internal;
		nNumberOfBytes = (DWORD) lpOverlapped->InternalHigh;

		if (request == 0)
		{
			if (pipe->clientfd == -1)
				return FALSE;

			status = read(pipe->clientfd, lpBuffer, nNumberOfBytes);
		}
		else if (request == 1)
		{
			if (pipe->clientfd == -1)
				return FALSE;

			status = write(pipe->clientfd, lpBuffer, nNumberOfBytes);
		}
		else if (request == 2)
		{
			socklen_t length;
			struct sockaddr_un s;

			if (pipe->serverfd == -1)
				return FALSE;

			length = sizeof(struct sockaddr_un);
			ZeroMemory(&s, sizeof(struct sockaddr_un));

			status = accept(pipe->serverfd, (struct sockaddr*) &s, &length);

			if (status < 0)
				return FALSE;

			pipe->clientfd = status;
			pipe->ServerMode = FALSE;

			status = 0;
		}

		if (status < 0)
		{
			*lpNumberOfBytesTransferred = 0;
			return FALSE;
		}

		*lpNumberOfBytesTransferred = status;
	}

	return TRUE;
#endif
}

BOOL GetOverlappedResultEx(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, DWORD dwMilliseconds, BOOL bAlertable)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
		LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
		PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped, DWORD dwMilliseconds)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
		ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds, BOOL fAlertable)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelIo(HANDLE hFile)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelSynchronousIo(HANDLE hThread)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

#endif

#ifdef _UWP

BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
{
	return GetOverlappedResultEx(hFile, lpOverlapped, lpNumberOfBytesTransferred, bWait ? INFINITE : 0, TRUE);
}

BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
	LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
	PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped, DWORD dwMilliseconds)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
	ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds, BOOL fAlertable)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelIo(HANDLE hFile)
{
	return CancelIoEx(hFile, NULL);
}

BOOL CancelSynchronousIo(HANDLE hThread)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

#endif
