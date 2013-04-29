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
#include <winpr/error.h>

#ifndef _WIN32

#ifndef MAX_PATH
#define MAX_PATH				260
#endif

#define INVALID_HANDLE_VALUE			((HANDLE) (LONG_PTR) - 1)
#define INVALID_FILE_SIZE			((DWORD) 0xFFFFFFFF)
#define INVALID_SET_FILE_POINTER		((DWORD) - 1)
#define INVALID_FILE_ATTRIBUTES			((DWORD) - 1)

#define FILE_READ_DATA				0x0001
#define FILE_LIST_DIRECTORY			0x0001
#define FILE_WRITE_DATA				0x0002
#define FILE_ADD_FILE				0x0002
#define FILE_APPEND_DATA			0x0004
#define FILE_ADD_SUBDIRECTORY			0x0004
#define FILE_CREATE_PIPE_INSTANCE		0x0004
#define FILE_READ_EA				0x0008
#define FILE_WRITE_EA				0x0010
#define FILE_EXECUTE				0x0020
#define FILE_TRAVERSE				0x0020
#define FILE_DELETE_CHILD			0x0040
#define FILE_READ_ATTRIBUTES			0x0080
#define FILE_WRITE_ATTRIBUTES			0x0100

#define FILE_ALL_ACCESS		(STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x1FF)
#define FILE_GENERIC_READ	(STANDARD_RIGHTS_READ | FILE_READ_DATA | FILE_READ_ATTRIBUTES | FILE_READ_EA | SYNCHRONIZE)
#define FILE_GENERIC_WRITE	(STANDARD_RIGHTS_WRITE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_APPEND_DATA | SYNCHRONIZE)
#define FILE_GENERIC_EXECUTE	(STANDARD_RIGHTS_EXECUTE | FILE_READ_ATTRIBUTES | FILE_EXECUTE | SYNCHRONIZE)

#define FILE_SHARE_READ				0x00000001
#define FILE_SHARE_WRITE			0x00000002
#define FILE_SHARE_DELETE			0x00000004

#define FILE_ATTRIBUTE_READONLY			0x00000001
#define FILE_ATTRIBUTE_HIDDEN			0x00000002
#define FILE_ATTRIBUTE_SYSTEM			0x00000004
#define FILE_ATTRIBUTE_DIRECTORY		0x00000010
#define FILE_ATTRIBUTE_ARCHIVE			0x00000020
#define FILE_ATTRIBUTE_DEVICE			0x00000040
#define FILE_ATTRIBUTE_NORMAL			0x00000080
#define FILE_ATTRIBUTE_TEMPORARY		0x00000100
#define FILE_ATTRIBUTE_SPARSE_FILE		0x00000200
#define FILE_ATTRIBUTE_REPARSE_POINT		0x00000400
#define FILE_ATTRIBUTE_COMPRESSED		0x00000800
#define FILE_ATTRIBUTE_OFFLINE			0x00001000
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED	0x00002000
#define FILE_ATTRIBUTE_ENCRYPTED		0x00004000
#define FILE_ATTRIBUTE_VIRTUAL			0x00010000

#define FILE_NOTIFY_CHANGE_FILE_NAME		0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME		0x00000002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES		0x00000004
#define FILE_NOTIFY_CHANGE_SIZE			0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE		0x00000010
#define FILE_NOTIFY_CHANGE_LAST_ACCESS		0x00000020
#define FILE_NOTIFY_CHANGE_CREATION		0x00000040
#define FILE_NOTIFY_CHANGE_SECURITY		0x00000100

#define FILE_ACTION_ADDED			0x00000001
#define FILE_ACTION_REMOVED			0x00000002
#define FILE_ACTION_MODIFIED			0x00000003
#define FILE_ACTION_RENAMED_OLD_NAME		0x00000004
#define FILE_ACTION_RENAMED_NEW_NAME		0x00000005

#define FILE_CASE_SENSITIVE_SEARCH		0x00000001
#define FILE_CASE_PRESERVED_NAMES		0x00000002
#define FILE_UNICODE_ON_DISK			0x00000004
#define FILE_PERSISTENT_ACLS			0x00000008
#define FILE_FILE_COMPRESSION			0x00000010
#define FILE_VOLUME_QUOTAS			0x00000020
#define FILE_SUPPORTS_SPARSE_FILES		0x00000040
#define FILE_SUPPORTS_REPARSE_POINTS		0x00000080
#define FILE_SUPPORTS_REMOTE_STORAGE		0x00000100
#define FILE_VOLUME_IS_COMPRESSED		0x00008000
#define FILE_SUPPORTS_OBJECT_IDS		0x00010000
#define FILE_SUPPORTS_ENCRYPTION		0x00020000
#define FILE_NAMED_STREAMS			0x00040000
#define FILE_READ_ONLY_VOLUME			0x00080000
#define FILE_SEQUENTIAL_WRITE_ONCE		0x00100000
#define FILE_SUPPORTS_TRANSACTIONS		0x00200000
#define FILE_SUPPORTS_HARD_LINKS		0x00400000
#define FILE_SUPPORTS_EXTENDED_ATTRIBUTES	0x00800000
#define FILE_SUPPORTS_OPEN_BY_FILE_ID		0x01000000
#define FILE_SUPPORTS_USN_JOURNAL		0x02000000

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

#define CREATE_NEW				1
#define CREATE_ALWAYS				2
#define OPEN_EXISTING				3
#define OPEN_ALWAYS				4
#define TRUNCATE_EXISTING			5

#define FIND_FIRST_EX_CASE_SENSITIVE		0x1
#define FIND_FIRST_EX_LARGE_FETCH		0x2

typedef union _FILE_SEGMENT_ELEMENT
{
	PVOID64 Buffer;
	ULONGLONG Alignment;
} FILE_SEGMENT_ELEMENT, *PFILE_SEGMENT_ELEMENT;

typedef struct _WIN32_FIND_DATAA
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;
	CHAR cFileName[MAX_PATH];
	CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, *PWIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

typedef struct _WIN32_FIND_DATAW
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;
	WCHAR cFileName[MAX_PATH];
	WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

typedef enum _FINDEX_INFO_LEVELS
{
	FindExInfoStandard,
	FindExInfoMaxInfoLevel
} FINDEX_INFO_LEVELS;

typedef enum _FINDEX_SEARCH_OPS
{
	FindExSearchNameMatch,
	FindExSearchLimitToDirectories,
	FindExSearchLimitToDevices,
	FindExSearchMaxSearchOp
} FINDEX_SEARCH_OPS;

typedef VOID (*LPOVERLAPPED_COMPLETION_ROUTINE)(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

#ifdef UNICODE
#define WIN32_FIND_DATA		WIN32_FIND_DATAW
#define PWIN32_FIND_DATA	PWIN32_FIND_DATAW
#define LPWIN32_FIND_DATA	LPWIN32_FIND_DATAW
#else
#define WIN32_FIND_DATA		WIN32_FIND_DATAA
#define PWIN32_FIND_DATA	PWIN32_FIND_DATAA
#define LPWIN32_FIND_DATA	LPWIN32_FIND_DATAA
#endif

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

WINPR_API HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

WINPR_API BOOL DeleteFileA(LPCSTR lpFileName);

WINPR_API BOOL DeleteFileW(LPCWSTR lpFileName);

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

WINPR_API HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
WINPR_API HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);

WINPR_API HANDLE FindFirstFileExA(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData,
		FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags);
WINPR_API HANDLE FindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData,
		FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags);

WINPR_API BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
WINPR_API BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData);

WINPR_API BOOL FindClose(HANDLE hFindFile);

WINPR_API BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
WINPR_API BOOL CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define CreateFile		CreateFileW
#define DeleteFile		DeleteFileW
#define FindFirstFile		FindFirstFileW
#define FindFirstFileEx		FindFirstFileExW
#define FindNextFile		FindNextFileW
#define CreateDirectory		CreateDirectoryW
#else
#define CreateFile		CreateFileA
#define DeleteFile		DeleteFileA
#define FindFirstFile		FindFirstFileA
#define FindFirstFileEx		FindFirstFileExA
#define FindNextFile		FindNextFileA
#define CreateDirectory		CreateDirectoryA
#endif

#endif

/* Extra Functions */

#define WILDCARD_STAR		0x00000001
#define WILDCARD_QM		0x00000002
#define WILDCARD_DOS		0x00000100
#define WILDCARD_DOS_STAR	0x00000110
#define WILDCARD_DOS_QM		0x00000120
#define WILDCARD_DOS_DOT	0x00000140

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API BOOL FilePatternMatchA(LPCSTR lpFileName, LPCSTR lpPattern);
WINPR_API LPSTR FilePatternFindNextWildcardA(LPCSTR lpPattern, DWORD* pFlags);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_FILE_H */

