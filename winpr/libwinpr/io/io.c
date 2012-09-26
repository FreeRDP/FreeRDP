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

/**
 * api-ms-win-core-io-l1-1-1.dll:
 * 
 * GetOverlappedResult
 * GetOverlappedResultEx
 * DeviceIoControl
 * CreateIoCompletionPort
 * GetQueuedCompletionStatus
 * GetQueuedCompletionStatusEx
 * PostQueuedCompletionStatus
 * CancelIo
 * CancelIoEx
 * CancelSynchronousIo
 */

#ifndef _WIN32

BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait)
{
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
