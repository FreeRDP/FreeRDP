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
#include "io.h"

#define TAG WINPR_TAG("io")

static UINT32 hashtable_threadId_hash(DWORD* key)
{
	return (UINT32)*key;
}

static BOOL hashtable_threadId_compare(DWORD* key1, DWORD* key2)
{
	return *key1 == *key2;
}

void hashtable_perThread_free(PerThreadOverlap* value)
{
	apc_remove(&value->apc);
	while (Queue_Count(value->pendingOperations))
		value->freeOpFn(Queue_Dequeue(value->pendingOperations));
	Queue_Free(value->pendingOperations);
	free(value);
}

BOOL winpr_overlap_init(WINPR_OVERLAP_IMPL* over)
{
	int i;
	wHashTable* hashtables[2];

	hashtables[0] = over->readOverlaps = HashTable_New(TRUE);
	if (!over->readOverlaps)
		return FALSE;

	hashtables[1] = over->writeOverlaps = HashTable_New(TRUE);
	if (!over->writeOverlaps)
	{
		HashTable_Free(over->readOverlaps);
		return FALSE;
	}

	for (i = 0; i < 2; i++)
	{
		hashtables[i]->hash = (HASH_TABLE_HASH_FN)hashtable_threadId_hash;
		hashtables[i]->keyCompare = (HASH_TABLE_KEY_COMPARE_FN)hashtable_threadId_compare;
		hashtables[i]->valueFree = (HASH_TABLE_VALUE_FREE_FN)hashtable_perThread_free;
	}
	return TRUE;
}

void winpr_overlap_uninit(WINPR_OVERLAP_IMPL* over)
{
	HashTable_Free(over->readOverlaps);
	HashTable_Free(over->writeOverlaps);
}

PerThreadOverlap* ensureThreadOverlap(DWORD threadId, wHashTable* overlaps, BOOL* created)
{
	PerThreadOverlap* ret = HashTable_GetItemValue(overlaps, &threadId);
	if (!ret)
	{
		ret = calloc(1, sizeof(PerThreadOverlap));
		if (!ret)
			return NULL;

		ret->threadId = threadId;
		ret->pendingOperations = Queue_New(TRUE, 0, 0);
		if (!ret->pendingOperations)
		{
			free(ret);
			return NULL;
		}

		if (HashTable_Add(overlaps, &ret->threadId, ret) < 0)
		{
			Queue_Free(ret->pendingOperations);
			free(ret);
			return NULL;
		}
		*created = TRUE;
	}
	else
	{
		*created = FALSE;
	}
	return ret;
}

BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped,
                         LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
{
	if (!bWait)
	{
		if (lpOverlapped->Internal == (ULONG_PTR)STATUS_PENDING)
		{
			SetLastError(ERROR_IO_INCOMPLETE);
			return FALSE;
		}

		*lpNumberOfBytesTransferred = (DWORD)lpOverlapped->InternalHigh;
		return TRUE;
	}

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

		pipe = (WINPR_NAMED_PIPE*)Object;

		if (!(pipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
			return FALSE;

		lpBuffer = lpOverlapped->Pointer;
		request = (DWORD)lpOverlapped->Internal;
		nNumberOfBytes = (DWORD)lpOverlapped->InternalHigh;

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

			status = accept(pipe->serverfd, (struct sockaddr*)&s, &length);

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

BOOL GetOverlappedResultEx(HANDLE hFile, LPOVERLAPPED lpOverlapped,
                           LPDWORD lpNumberOfBytesTransferred, DWORD dwMilliseconds,
                           BOOL bAlertable)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
                     LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned,
                     LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort,
                              ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
                               PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped,
                               DWORD dwMilliseconds)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
                                 ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds,
                                 BOOL fAlertable)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred,
                                ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelIo(HANDLE hFile)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->CancelIo)
		return handle->ops->CancelIo(handle);

	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	WLog_ERR(TAG, "%s operation not implemented", __FUNCTION__);
	return FALSE;
}

BOOL CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->CancelIoEx)
		return handle->ops->CancelIoEx(handle, lpOverlapped);

	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	WLog_ERR(TAG, "%s operation not implemented", __FUNCTION__);
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

#include <winpr/crt.h>
#include <winpr/wlog.h>

#include "../log.h"

#define TAG WINPR_TAG("io")

BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped,
                         LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
{
	return GetOverlappedResultEx(hFile, lpOverlapped, lpNumberOfBytesTransferred,
	                             bWait ? INFINITE : 0, TRUE);
}

BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
                     LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned,
                     LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort,
                              ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
                               PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped,
                               DWORD dwMilliseconds)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
                                 ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds,
                                 BOOL fAlertable)
{
	WLog_ERR(TAG, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred,
                                ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped)
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
