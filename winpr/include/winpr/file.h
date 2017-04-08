/**
 * WinPR: Windows Portable Runtime
 * File Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#include <winpr/nt.h>
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

#define PAGE_NOACCESS				0x00000001
#define PAGE_READONLY				0x00000002
#define PAGE_READWRITE				0x00000004
#define PAGE_WRITECOPY				0x00000008
#define PAGE_EXECUTE				0x00000010
#define PAGE_EXECUTE_READ			0x00000020
#define PAGE_EXECUTE_READWRITE			0x00000040
#define PAGE_EXECUTE_WRITECOPY			0x00000080
#define PAGE_GUARD				0x00000100
#define PAGE_NOCACHE				0x00000200
#define PAGE_WRITECOMBINE			0x00000400

#define MEM_COMMIT				0x00001000
#define MEM_RESERVE				0x00002000
#define MEM_DECOMMIT				0x00004000
#define MEM_RELEASE				0x00008000
#define MEM_FREE				0x00010000
#define MEM_PRIVATE				0x00020000
#define MEM_MAPPED				0x00040000
#define MEM_RESET				0x00080000
#define MEM_TOP_DOWN				0x00100000
#define MEM_WRITE_WATCH				0x00200000
#define MEM_PHYSICAL				0x00400000
#define MEM_4MB_PAGES				0x80000000
#define MEM_IMAGE				SEC_IMAGE

#define SEC_NO_CHANGE				0x00400000
#define SEC_FILE				0x00800000
#define SEC_IMAGE				0x01000000
#define SEC_VLM					0x02000000
#define SEC_RESERVE				0x04000000
#define SEC_COMMIT				0x08000000
#define SEC_NOCACHE				0x10000000
#define SEC_WRITECOMBINE			0x40000000
#define SEC_LARGE_PAGES				0x80000000

#define SECTION_MAP_EXECUTE_EXPLICIT		0x00020
#define SECTION_EXTEND_SIZE			0x00010
#define SECTION_MAP_READ			0x00004
#define SECTION_MAP_WRITE			0x00002
#define SECTION_QUERY				0x00001
#define SECTION_MAP_EXECUTE			0x00008
#define SECTION_ALL_ACCESS			0xF001F

#define FILE_MAP_COPY				SECTION_QUERY
#define FILE_MAP_WRITE				SECTION_MAP_WRITE
#define FILE_MAP_READ				SECTION_MAP_READ
#define FILE_MAP_ALL_ACCESS			SECTION_ALL_ACCESS
#define FILE_MAP_EXECUTE			SECTION_MAP_EXECUTE_EXPLICIT

#define CREATE_NEW				1
#define CREATE_ALWAYS				2
#define OPEN_EXISTING				3
#define OPEN_ALWAYS				4
#define TRUNCATE_EXISTING			5

#define FIND_FIRST_EX_CASE_SENSITIVE		0x1
#define FIND_FIRST_EX_LARGE_FETCH		0x2

#define STD_INPUT_HANDLE  (DWORD)-10
#define STD_OUTPUT_HANDLE (DWORD)-11
#define STD_ERROR_HANDLE  (DWORD)-12

#define FILE_BEGIN   0
#define FILE_CURRENT 1
#define FILE_END     2

#define LOCKFILE_FAIL_IMMEDIATELY 1
#define LOCKFILE_EXCLUSIVE_LOCK   2

#define MOVEFILE_REPLACE_EXISTING 0x1
#define MOVEFILE_COPY_ALLOWED 0x2
#define MOVEFILE_DELAY_UNTIL_REBOOT 0x4
#define MOVEFILE_WRITE_THROUGH 0x8
#define MOVEFILE_CREATE_HARDLINK 0x10
#define MOVEFILE_FAIL_IF_NOT_TRACKABLE 0x20

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

typedef struct _WIN32_FILE_ATTRIBUTE_DATA
{
	DWORD    dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD    nFileSizeHigh;
	DWORD    nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA, *LPWIN32_FILE_ATTRIBUTE_DATA;

typedef enum _GET_FILEEX_INFO_LEVELS
{
	GetFileExInfoStandard,
	GetFileExMaxInfoLevel
} GET_FILEEX_INFO_LEVELS;

WINPR_API BOOL GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);

WINPR_API DWORD GetFileAttributesA(LPCSTR lpFileName);

WINPR_API BOOL GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);
	
WINPR_API DWORD GetFileAttributesW(LPCWSTR lpFileName);

WINPR_API BOOL SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes);

WINPR_API BOOL SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes);

WINPR_API BOOL SetEndOfFile(HANDLE hFile);

WINPR_API DWORD WINAPI GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);

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

WINPR_API BOOL SetFileTime(HANDLE hFile, const FILETIME *lpCreationTime,
		const FILETIME *lpLastAccessTime, const FILETIME *lpLastWriteTime);

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

WINPR_API BOOL RemoveDirectoryA(LPCSTR lpPathName);
WINPR_API BOOL RemoveDirectoryW(LPCWSTR lpPathName);

WINPR_API HANDLE GetStdHandle(DWORD nStdHandle);
WINPR_API BOOL SetStdHandle(DWORD nStdHandle, HANDLE hHandle);
WINPR_API BOOL SetStdHandleEx(DWORD dwStdHandle, HANDLE hNewHandle, HANDLE* phOldHandle);

WINPR_API BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
	LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);

WINPR_API BOOL GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
	LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);

WINPR_API BOOL MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags);

WINPR_API BOOL MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags);

WINPR_API BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);

WINPR_API BOOL MoveFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName);

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
#define RemoveDirectory		RemoveDirectoryW
#define GetFileAttributesEx	GetFileAttributesExW
#define GetFileAttributes	GetFileAttributesW
#define SetFileAttributes	SetFileAttributesW
#define GetDiskFreeSpace	GetDiskFreeSpaceW
#define MoveFileEx		MoveFileExW
#define MoveFile		MoveFileW
#else
#define CreateFile		CreateFileA
#define DeleteFile		DeleteFileA
#define FindFirstFile		FindFirstFileA
#define FindFirstFileEx		FindFirstFileExA
#define FindNextFile		FindNextFileA
#define CreateDirectory		CreateDirectoryA
#define RemoveDirectory		RemoveDirectoryA
#define GetFileAttributesEx	GetFileAttributesExA
#define GetFileAttributes	GetFileAttributesA
#define SetFileAttributes	SetFileAttributesA
#define GetDiskFreeSpace	GetDiskFreeSpaceA
#define MoveFileEx		MoveFileExA
#define MoveFile		MoveFileA
#endif

/* Extra Functions */

typedef BOOL (*pcIsFileHandled)(LPCSTR lpFileName);
typedef HANDLE (*pcCreateFileA)(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
				DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

typedef struct _HANDLE_CREATOR
{
	pcIsFileHandled IsHandled;
	pcCreateFileA CreateFileA;
} HANDLE_CREATOR, *PHANDLE_CREATOR, *LPHANDLE_CREATOR;

WINPR_API BOOL ValidFileNameComponent(LPCWSTR lpFileName);

#endif /* _WIN32 */

#ifdef _UWP

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

WINPR_API HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

WINPR_API DWORD WINAPI GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);

WINPR_API DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
	PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);

WINPR_API HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
WINPR_API HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);

WINPR_API DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart);

WINPR_API BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
	LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);

WINPR_API BOOL GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
	LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);

WINPR_API DWORD GetLogicalDriveStringsA(DWORD nBufferLength, LPSTR lpBuffer);

WINPR_API DWORD GetLogicalDriveStringsW(DWORD nBufferLength, LPWSTR lpBuffer);

WINPR_API BOOL PathIsDirectoryEmptyA(LPCSTR pszPath);

WINPR_API UINT GetACP(void);

#ifdef UNICODE
#define CreateFile		CreateFileW
#define FindFirstFile		FindFirstFileW
#else
#define CreateFile		CreateFileA
#define FindFirstFile		FindFirstFileA
#endif

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define FindFirstFile		FindFirstFileW
#else
#define FindFirstFile		FindFirstFileA
#endif

#endif

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

WINPR_API int UnixChangeFileMode(const char* filename, int flags);

WINPR_API BOOL IsNamedPipeFileNameA(LPCSTR lpName);
WINPR_API char* GetNamedPipeNameWithoutPrefixA(LPCSTR lpName);
WINPR_API char* GetNamedPipeUnixDomainSocketBaseFilePathA(void);
WINPR_API char* GetNamedPipeUnixDomainSocketFilePathA(LPCSTR lpName);

WINPR_API int GetNamePipeFileDescriptor(HANDLE hNamedPipe);
WINPR_API HANDLE GetFileHandleForFileDescriptor(int fd);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_FILE_H */
