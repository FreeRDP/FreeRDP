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

#ifndef WINPR_IO_H
#define WINPR_IO_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

#define GENERIC_READ				0x80000000
#define GENERIC_WRITE				0x40000000
#define GENERIC_EXECUTE				0x20000000
#define GENERIC_ALL				0x10000000

#define DELETE					0x00010000
#define READ_CONTROL				0x00020000
#define WRITE_DAC				0x00040000
#define WRITE_OWNER				0x00080000
#define SYNCHRONIZE				0x00100000
#define STANDARD_RIGHTS_REQUIRED		0x000F0000
#define STANDARD_RIGHTS_READ			0x00020000
#define STANDARD_RIGHTS_WRITE			0x00020000
#define STANDARD_RIGHTS_EXECUTE			0x00020000
#define STANDARD_RIGHTS_ALL			0x001F0000
#define SPECIFIC_RIGHTS_ALL			0x0000FFFF
#define ACCESS_SYSTEM_SECURITY			0x01000000
#define MAXIMUM_ALLOWED				0x02000000

typedef struct _OVERLAPPED
{
	ULONG_PTR Internal;
	ULONG_PTR InternalHigh;
	union
	{
		struct
		{
			DWORD Offset;
			DWORD OffsetHigh;
		};
		PVOID Pointer;
	};
	HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef struct _OVERLAPPED_ENTRY
{
	ULONG_PTR lpCompletionKey;
	LPOVERLAPPED lpOverlapped;
	ULONG_PTR Internal;
	DWORD dwNumberOfBytesTransferred;
} OVERLAPPED_ENTRY, *LPOVERLAPPED_ENTRY;

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API BOOL GetOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait);

WINPR_API BOOL GetOverlappedResultEx(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, DWORD dwMilliseconds, BOOL bAlertable);

WINPR_API BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
		LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

WINPR_API HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort, ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads);

WINPR_API BOOL GetQueuedCompletionStatus(HANDLE CompletionPort, LPDWORD lpNumberOfBytesTransferred,
		PULONG_PTR lpCompletionKey, LPOVERLAPPED* lpOverlapped, DWORD dwMilliseconds);

WINPR_API BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort, LPOVERLAPPED_ENTRY lpCompletionPortEntries,
		ULONG ulCount, PULONG ulNumEntriesRemoved, DWORD dwMilliseconds, BOOL fAlertable);

WINPR_API BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred, ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL CancelIo(HANDLE hFile);

WINPR_API BOOL CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL CancelSynchronousIo(HANDLE hThread);

#ifdef __cplusplus
}
#endif

#endif

#endif /* WINPR_IO_H */

