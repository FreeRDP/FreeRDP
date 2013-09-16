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

#include "../handle/handle.h"

#include "../pipe/pipe.h"

BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hFile, &Type, &Object))
		return FALSE;

	else if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		int status;
		DWORD request;
		PVOID lpBuffer;
		DWORD nNumberOfBytes;
		WINPR_NAMED_PIPE* pipe;

		pipe = (WINPR_NAMED_PIPE*) Object;

		if (!(pipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
			return FALSE;

		if (pipe->clientfd == -1)
			return FALSE;

		lpBuffer = lpOverlapped->Pointer;
		request = (DWORD) lpOverlapped->Internal;
		nNumberOfBytes = (DWORD) lpOverlapped->InternalHigh;

		if (request == 0)
		{
			status = read(pipe->clientfd, lpBuffer, nNumberOfBytes);
		}
		else
		{
			status = write(pipe->clientfd, lpBuffer, nNumberOfBytes);
		}

		if (status < 0)
		{
			*lpNumberOfBytesTransferred = 0;
			return FALSE;
		}

		*lpNumberOfBytesTransferred = status;
	}

	return TRUE;
}

BOOL GetOverlappedResultEx(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, DWORD dwMilliseconds, BOOL bAlertable)
{
	return TRUE;
}

BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
		LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads)
{
	return NULL;
}

BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
		PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped, DWORD dwMilliseconds)
{
	return TRUE;
}

BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
		ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds, BOOL fAlertable)
{
	return TRUE;
}

BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

BOOL CancelIo(HANDLE hFile)
{
	return TRUE;
}

BOOL CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

BOOL CancelSynchronousIo(HANDLE hThread)
{
	return TRUE;
}

#endif
