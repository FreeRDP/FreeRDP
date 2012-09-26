/**
 * WinPR: Windows Portable Runtime
 * File Functions
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

#ifndef WINPR_FILE_H
#define WINPR_FILE_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/io.h>

#ifndef _WIN32

#define FILE_FLAG_WRITE_THROUGH			0x80000000
#define FILE_FLAG_OVERLAPPED			0x40000000
#define FILE_FLAG_NO_BUFFERING			0x20000000
#define FILE_FLAG_RANDOM_ACCESS			0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN		0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE		0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS		0x02000000
#define FILE_FLAG_POSIX_SEMANTICS		0x01000000
#define FILE_FLAG_OPEN_REPARSE_POINT		0x00200000
#define FILE_FLAG_OPEN_NO_RECALL		0x00100000
#define FILE_FLAG_FIRST_PIPE_INSTANCE		0x00080000

typedef union _FILE_SEGMENT_ELEMENT
{
	PVOID64 Buffer;
	ULONGLONG Alignment;
} FILE_SEGMENT_ELEMENT, *PFILE_SEGMENT_ELEMENT;

typedef VOID (*LPOVERLAPPED_COMPLETION_ROUTINE)(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL ReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

WINPR_API BOOL ReadFileScatter(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToRead, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL WriteFileEx(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);

WINPR_API BOOL WriteFileGather(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToWrite, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL FlushFileBuffers(HANDLE hFile);

WINPR_API BOOL SetEndOfFile(HANDLE hFile);

WINPR_API DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
		PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);

WINPR_API BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove,
		PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod);

WINPR_API BOOL LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
		DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh);

WINPR_API BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved,
		DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
		DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh);

WINPR_API BOOL UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow,
		DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped);

#endif

#endif /* WINPR_FILE_H */

