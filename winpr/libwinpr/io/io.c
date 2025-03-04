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

#include <winpr/config.h>

#include <winpr/io.h>

#ifndef _WIN32

#ifdef WINPR_HAVE_UNISTD_H
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

BOOL GetOverlappedResult(WINPR_ATTR_UNUSED HANDLE hFile,
                         WINPR_ATTR_UNUSED LPOVERLAPPED lpOverlapped,
                         WINPR_ATTR_UNUSED LPDWORD lpNumberOfBytesTransferred,
                         WINPR_ATTR_UNUSED BOOL bWait)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetOverlappedResultEx(WINPR_ATTR_UNUSED HANDLE hFile,
                           WINPR_ATTR_UNUSED LPOVERLAPPED lpOverlapped,
                           WINPR_ATTR_UNUSED LPDWORD lpNumberOfBytesTransferred,
                           WINPR_ATTR_UNUSED DWORD dwMilliseconds,
                           WINPR_ATTR_UNUSED BOOL bAlertable)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL DeviceIoControl(WINPR_ATTR_UNUSED HANDLE hDevice, WINPR_ATTR_UNUSED DWORD dwIoControlCode,
                     WINPR_ATTR_UNUSED LPVOID lpInBuffer, WINPR_ATTR_UNUSED DWORD nInBufferSize,
                     WINPR_ATTR_UNUSED LPVOID lpOutBuffer, WINPR_ATTR_UNUSED DWORD nOutBufferSize,
                     WINPR_ATTR_UNUSED LPDWORD lpBytesReturned,
                     WINPR_ATTR_UNUSED LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

HANDLE CreateIoCompletionPort(WINPR_ATTR_UNUSED HANDLE FileHandle,
                              WINPR_ATTR_UNUSED HANDLE ExistingCompletionPort,
                              WINPR_ATTR_UNUSED ULONG_PTR CompletionKey,
                              WINPR_ATTR_UNUSED DWORD NumberOfConcurrentThreads)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

BOOL GetQueuedCompletionStatus(WINPR_ATTR_UNUSED HANDLE CompletionPort,
                               WINPR_ATTR_UNUSED LPDWORD lpNumberOfBytesTransferred,
                               WINPR_ATTR_UNUSED PULONG_PTR lpCompletionKey,
                               WINPR_ATTR_UNUSED LPOVERLAPPED* lpOverlapped,
                               WINPR_ATTR_UNUSED DWORD dwMilliseconds)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetQueuedCompletionStatusEx(WINPR_ATTR_UNUSED HANDLE CompletionPort,
                                 WINPR_ATTR_UNUSED LPOVERLAPPED_ENTRY lpCompletionPortEntries,
                                 WINPR_ATTR_UNUSED ULONG ulCount,
                                 WINPR_ATTR_UNUSED PULONG ulNumEntriesRemoved,
                                 WINPR_ATTR_UNUSED DWORD dwMilliseconds,
                                 WINPR_ATTR_UNUSED BOOL fAlertable)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL PostQueuedCompletionStatus(WINPR_ATTR_UNUSED HANDLE CompletionPort,
                                WINPR_ATTR_UNUSED DWORD dwNumberOfBytesTransferred,
                                WINPR_ATTR_UNUSED ULONG_PTR dwCompletionKey,
                                WINPR_ATTR_UNUSED LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelIo(WINPR_ATTR_UNUSED HANDLE hFile)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelIoEx(WINPR_ATTR_UNUSED HANDLE hFile, WINPR_ATTR_UNUSED LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelSynchronousIo(WINPR_ATTR_UNUSED HANDLE hThread)
{
	WLog_ERR(TAG, "Not implemented");
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
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort,
                              ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
                               PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped,
                               DWORD dwMilliseconds)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
                                 ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds,
                                 BOOL fAlertable)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred,
                                ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CancelIo(HANDLE hFile)
{
	return CancelIoEx(hFile, NULL);
}

BOOL CancelSynchronousIo(HANDLE hThread)
{
	WLog_ERR(TAG, "Not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

#endif
