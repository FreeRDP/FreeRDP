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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/file.h>

/**
 * api-ms-win-core-file-l1-2-0.dll:
 * 
 * CreateFileA
 * CreateFileW
 * CreateFile2
 * DeleteFileA
 * DeleteFileW
 * CreateDirectoryA
 * CreateDirectoryW
 * RemoveDirectoryA
 * RemoveDirectoryW
 * CompareFileTime
 * DefineDosDeviceW
 * DeleteVolumeMountPointW
 * FileTimeToLocalFileTime
 * LocalFileTimeToFileTime
 * FindClose
 * FindCloseChangeNotification
 * FindFirstChangeNotificationA
 * FindFirstChangeNotificationW
 * FindFirstFileA
 * FindFirstFileExA
 * FindFirstFileExW
 * FindFirstFileW
 * FindFirstVolumeW
 * FindNextChangeNotification
 * FindNextFileA
 * FindNextFileW
 * FindNextVolumeW
 * FindVolumeClose
 * GetDiskFreeSpaceA
 * GetDiskFreeSpaceExA
 * GetDiskFreeSpaceExW
 * GetDiskFreeSpaceW
 * GetDriveTypeA
 * GetDriveTypeW
 * GetFileAttributesA
 * GetFileAttributesExA
 * GetFileAttributesExW
 * GetFileAttributesW
 * GetFileInformationByHandle
 * GetFileSize
 * GetFileSizeEx
 * GetFileTime
 * GetFileType
 * GetFinalPathNameByHandleA
 * GetFinalPathNameByHandleW
 * GetFullPathNameA
 * GetFullPathNameW
 * GetLogicalDrives
 * GetLogicalDriveStringsW
 * GetLongPathNameA
 * GetLongPathNameW
 * GetShortPathNameW
 * GetTempFileNameW
 * GetTempPathW
 * GetVolumeInformationByHandleW
 * GetVolumeInformationW
 * GetVolumeNameForVolumeMountPointW
 * GetVolumePathNamesForVolumeNameW
 * GetVolumePathNameW
 * QueryDosDeviceW
 * SetFileAttributesA
 * SetFileAttributesW
 * SetFileTime
 * SetFileValidData
 * SetFileInformationByHandle
 * ReadFile
 * ReadFileEx
 * ReadFileScatter
 * WriteFile
 * WriteFileEx
 * WriteFileGather
 * FlushFileBuffers
 * SetEndOfFile
 * SetFilePointer
 * SetFilePointerEx
 * LockFile
 * LockFileEx
 * UnlockFile
 * UnlockFileEx
 */

#ifndef _WIN32

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	return NULL;
}

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	return NULL;
}

BOOL DeleteFileA(LPCSTR lpFileName)
{
	return TRUE;
}

BOOL DeleteFileW(LPCWSTR lpFileName)
{
	return TRUE;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

BOOL ReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	return TRUE;
}

BOOL ReadFileScatter(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToRead, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

BOOL WriteFileEx(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	return TRUE;
}

BOOL WriteFileGather(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
		DWORD nNumberOfBytesToWrite, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

BOOL FlushFileBuffers(HANDLE hFile)
{
	return TRUE;
}

BOOL SetEndOfFile(HANDLE hFile)
{
	return TRUE;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
		PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	return TRUE;
}

BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove,
		PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod)
{
	return TRUE;
}

BOOL LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
		DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh)
{
	return TRUE;
}

BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved,
		DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

BOOL UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
		DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh)
{
	return TRUE;
}

BOOL UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow,
		DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped)
{
	return TRUE;
}

#endif

