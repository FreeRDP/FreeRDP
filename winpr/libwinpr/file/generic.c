/**
 * WinPR: Windows Portable Runtime
 * File Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Hewlett-Packard Development Company, L.P.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "../log.h"
#define TAG WINPR_TAG("file")

#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <assert.h>
#include <pthread.h>
#include <dirent.h>
#include <libgen.h>
#include <errno.h>

#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

#ifdef HAVE_AIO_H
#undef HAVE_AIO_H /* disable for now, incomplete */
#endif

#ifdef HAVE_AIO_H
#include <aio.h>
#endif

#ifdef ANDROID
#include <sys/vfs.h>
#else
#include <sys/statvfs.h>
#endif

#include "../handle/handle.h"

#include "../pipe/pipe.h"

#include "file.h"

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

/**
 * File System Behavior in the Microsoft Windows Environment:
 * http://download.microsoft.com/download/4/3/8/43889780-8d45-4b2e-9d3a-c696a890309f/File%20System%20Behavior%20Overview.pdf
 */

/**
 * Asynchronous I/O - The GNU C Library:
 * http://www.gnu.org/software/libc/manual/html_node/Asynchronous-I_002fO.html
 */

/**
 * aio.h - asynchronous input and output:
 * http://pubs.opengroup.org/onlinepubs/009695399/basedefs/aio.h.html
 */

/**
 * Asynchronous I/O User Guide:
 * http://code.google.com/p/kernel/wiki/AIOUserGuide
 */

#define EPOCH_DIFF 11644473600LL
#define STAT_TIME_TO_FILETIME(_t) (((UINT64)(_t) + EPOCH_DIFF) * 10000000LL)


static wArrayList* _HandleCreators;

static pthread_once_t _HandleCreatorsInitialized = PTHREAD_ONCE_INIT;


HANDLE_CREATOR* GetNamedPipeClientHandleCreator(void);

#if defined __linux__ && !defined ANDROID
HANDLE_CREATOR* GetCommHandleCreator(void);
#endif /* __linux__ && !defined ANDROID */

static void _HandleCreatorsInit()
{
	assert(_HandleCreators == NULL);
	_HandleCreators = ArrayList_New(TRUE);

	if (!_HandleCreators)
		return;

	/*
	 * Register all file handle creators.
	 */
	ArrayList_Add(_HandleCreators, GetNamedPipeClientHandleCreator());
#if defined __linux__ && !defined ANDROID
	ArrayList_Add(_HandleCreators, GetCommHandleCreator());
#endif /* __linux__ && !defined ANDROID */
	ArrayList_Add(_HandleCreators, GetFileHandleCreator());
}


#ifdef HAVE_AIO_H

static BOOL g_AioSignalHandlerInstalled = FALSE;

void AioSignalHandler(int signum, siginfo_t* siginfo, void* arg)
{
	WLog_INFO("%d", signum);
}

int InstallAioSignalHandler()
{
	if (!g_AioSignalHandlerInstalled)
	{
		struct sigaction action;
		sigemptyset(&action.sa_mask);
		sigaddset(&action.sa_mask, SIGIO);
		action.sa_flags = SA_SIGINFO;
		action.sa_sigaction = (void*) &AioSignalHandler;
		sigaction(SIGIO, &action, NULL);
		g_AioSignalHandlerInstalled = TRUE;
	}

	return 0;
}

#endif /* HAVE_AIO_H */

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	int i;

	if (!lpFileName)
		return INVALID_HANDLE_VALUE;

	if (pthread_once(&_HandleCreatorsInitialized, _HandleCreatorsInit) != 0)
	{
		SetLastError(ERROR_DLL_INIT_FAILED);
		return INVALID_HANDLE_VALUE;
	}

	if (_HandleCreators == NULL)
	{
		SetLastError(ERROR_DLL_INIT_FAILED);
		return INVALID_HANDLE_VALUE;
	}

	ArrayList_Lock(_HandleCreators);

	for (i = 0; i <= ArrayList_Count(_HandleCreators); i++)
	{
		HANDLE_CREATOR* creator = ArrayList_GetItem(_HandleCreators, i);

		if (creator && creator->IsHandled(lpFileName))
		{
			HANDLE newHandle = creator->CreateFileA(lpFileName, dwDesiredAccess,
			                                        dwShareMode, lpSecurityAttributes, dwCreationDisposition,
			                                        dwFlagsAndAttributes, hTemplateFile);
			ArrayList_Unlock(_HandleCreators);
			return newHandle;
		}
	}

	ArrayList_Unlock(_HandleCreators);
	return INVALID_HANDLE_VALUE;
}

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	LPSTR lpFileNameA = NULL;
	HANDLE hdl;

	if (ConvertFromUnicode(CP_UTF8, 0, lpFileName, -1, &lpFileNameA, 0, NULL, NULL) < 1)
		return NULL;

	hdl = CreateFileA(lpFileNameA, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
	                  dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	free(lpFileNameA);
	return hdl;
}

BOOL DeleteFileA(LPCSTR lpFileName)
{
	int status;
	status = unlink(lpFileName);
	return (status != -1) ? TRUE : FALSE;
}

BOOL DeleteFileW(LPCWSTR lpFileName)
{
	LPSTR lpFileNameA = NULL;
	BOOL rc = FALSE;

	if (ConvertFromUnicode(CP_UTF8, 0, lpFileName, -1, &lpFileNameA, 0, NULL, NULL) < 1)
		return FALSE;

	rc = DeleteFileA(lpFileNameA);
	free(lpFileNameA);
	return rc;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	/*
	 * from http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467%28v=vs.85%29.aspx
	 * lpNumberOfBytesRead can be NULL only when the lpOverlapped parameter is not NULL.
	 */

	if (!lpNumberOfBytesRead && !lpOverlapped)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->ReadFile)
		return handle->ops->ReadFile(handle, lpBuffer, nNumberOfBytesToRead,
		                             lpNumberOfBytesRead, lpOverlapped);

	WLog_ERR(TAG, "ReadFile operation not implemented");
	return FALSE;
}

BOOL ReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->ReadFileEx)
		return handle->ops->ReadFileEx(handle, lpBuffer, nNumberOfBytesToRead,
		                               lpOverlapped, lpCompletionRoutine);

	WLog_ERR(TAG, "ReadFileEx operation not implemented");
	return FALSE;
	return TRUE;
}

BOOL ReadFileScatter(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
                     DWORD nNumberOfBytesToRead, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->ReadFileScatter)
		return handle->ops->ReadFileScatter(handle, aSegmentArray, nNumberOfBytesToRead,
		                                    lpReserved, lpOverlapped);

	WLog_ERR(TAG, "ReadFileScatter operation not implemented");
	return FALSE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->WriteFile)
		return handle->ops->WriteFile(handle, lpBuffer, nNumberOfBytesToWrite,
		                              lpNumberOfBytesWritten, lpOverlapped);

	WLog_ERR(TAG, "WriteFile operation not implemented");
	return FALSE;
}

BOOL WriteFileEx(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                 LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->WriteFileEx)
		return handle->ops->WriteFileEx(handle, lpBuffer, nNumberOfBytesToWrite,
		                                lpOverlapped, lpCompletionRoutine);

	WLog_ERR(TAG, "WriteFileEx operation not implemented");
	return FALSE;
}

BOOL WriteFileGather(HANDLE hFile, FILE_SEGMENT_ELEMENT aSegmentArray[],
                     DWORD nNumberOfBytesToWrite, LPDWORD lpReserved, LPOVERLAPPED lpOverlapped)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->WriteFileGather)
		return handle->ops->WriteFileGather(handle, aSegmentArray, nNumberOfBytesToWrite,
		                                    lpReserved, lpOverlapped);

	WLog_ERR(TAG, "WriteFileGather operation not implemented");
	return FALSE;
}

BOOL FlushFileBuffers(HANDLE hFile)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->FlushFileBuffers)
		return handle->ops->FlushFileBuffers(handle);

	WLog_ERR(TAG, "FlushFileBuffers operation not implemented");
	return FALSE;
}

BOOL WINAPI GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
                                 LPVOID lpFileInformation)
{
	LPWIN32_FILE_ATTRIBUTE_DATA fd = lpFileInformation;
	WIN32_FIND_DATAA findFileData;
	HANDLE hFind;

	if ((hFind = FindFirstFileA(lpFileName, &findFileData)) == INVALID_HANDLE_VALUE)
		return FALSE;

	FindClose(hFind);
	fd->dwFileAttributes = findFileData.dwFileAttributes;
	fd->ftCreationTime = findFileData.ftCreationTime;
	fd->ftLastAccessTime = findFileData.ftLastAccessTime;
	fd->ftLastWriteTime = findFileData.ftLastWriteTime;
	fd->nFileSizeHigh = findFileData.nFileSizeHigh;
	fd->nFileSizeLow = findFileData.nFileSizeLow;
	return TRUE;
}

BOOL WINAPI GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId,
                                 LPVOID lpFileInformation)
{
	BOOL ret;
	LPSTR lpCFileName;

	if (ConvertFromUnicode(CP_UTF8, 0, lpFileName, -1, &lpCFileName, 0, NULL, NULL) <= 0)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	ret = GetFileAttributesExA(lpCFileName, fInfoLevelId, lpFileInformation);
	free(lpCFileName);
	return ret;
}

DWORD WINAPI GetFileAttributesA(LPCSTR lpFileName)
{
	WIN32_FIND_DATAA findFileData;
	HANDLE hFind;

	if ((hFind = FindFirstFileA(lpFileName, &findFileData)) == INVALID_HANDLE_VALUE)
		return INVALID_FILE_ATTRIBUTES;

	FindClose(hFind);
	return findFileData.dwFileAttributes;
}

DWORD WINAPI GetFileAttributesW(LPCWSTR lpFileName)
{
	DWORD ret;
	LPSTR lpCFileName;

	if (ConvertFromUnicode(CP_UTF8, 0, lpFileName, -1, &lpCFileName, 0, NULL, NULL) <= 0)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	ret = GetFileAttributesA(lpCFileName);
	free(lpCFileName);
	return ret;
}

BOOL SetFileAttributesA(LPCSTR lpFileName, DWORD dwFileAttributes)
{
	struct stat st;

	if (stat(lpFileName, &st) != 0)
	{
		return FALSE;
	}

	if (dwFileAttributes & FILE_ATTRIBUTE_READONLY)
	{
		st.st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	}
	else
	{
		st.st_mode |= S_IWUSR;
	}

	if (chmod(lpFileName, st.st_mode) != 0)
	{
		return FALSE;
	}

	return TRUE;
}

BOOL SetFileAttributesW(LPCWSTR lpFileName, DWORD dwFileAttributes)
{
	BOOL ret;
	LPSTR lpCFileName;

	if (ConvertFromUnicode(CP_UTF8, 0, lpFileName, -1, &lpCFileName, 0, NULL, NULL) <= 0)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	ret = SetFileAttributesA(lpCFileName, dwFileAttributes);
	free(lpCFileName);
	return ret;
}

BOOL SetEndOfFile(HANDLE hFile)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->SetEndOfFile)
		return handle->ops->SetEndOfFile(handle);

	WLog_ERR(TAG, "SetEndOfFile operation not implemented");
	return FALSE;
}

DWORD WINAPI GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->GetFileSize)
		return handle->ops->GetFileSize(handle, lpFileSizeHigh);

	WLog_ERR(TAG, "GetFileSize operation not implemented");
	return 0;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
                     PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->SetFilePointer)
		return handle->ops->SetFilePointer(handle, lDistanceToMove,
		                                   lpDistanceToMoveHigh, dwMoveMethod);

	WLog_ERR(TAG, "SetFilePointer operation not implemented");
	return 0;
}

BOOL SetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove,
                      PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->SetFilePointerEx)
		return handle->ops->SetFilePointerEx(handle, liDistanceToMove,
		                                     lpNewFilePointer, dwMoveMethod);

	WLog_ERR(TAG, "SetFilePointerEx operation not implemented");
	return 0;
}

BOOL LockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
              DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->LockFile)
		return handle->ops->LockFile(handle, dwFileOffsetLow, dwFileOffsetHigh,
		                             nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh);

	WLog_ERR(TAG, "LockFile operation not implemented");
	return FALSE;
}

BOOL LockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved,
                DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->LockFileEx)
		return handle->ops->LockFileEx(handle, dwFlags, dwReserved,
		                               nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);

	WLog_ERR(TAG, "LockFileEx operation not implemented");
	return FALSE;
}

BOOL UnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
                DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->UnlockFile)
		return handle->ops->UnlockFile(handle, dwFileOffsetLow, dwFileOffsetHigh,
		                               nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh);

	WLog_ERR(TAG, "UnLockFile operation not implemented");
	return FALSE;
}

BOOL UnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow,
                  DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->UnlockFileEx)
		return handle->ops->UnlockFileEx(handle, dwReserved,
		                                 nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh, lpOverlapped);

	WLog_ERR(TAG, "UnLockFileEx operation not implemented");
	return FALSE;
}

BOOL WINAPI SetFileTime(HANDLE hFile, const FILETIME* lpCreationTime,
                        const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime)
{
	ULONG Type;
	WINPR_HANDLE* handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE*)hFile;

	if (handle->ops->SetFileTime)
		return handle->ops->SetFileTime(handle, lpCreationTime,
		                                lpLastAccessTime, lpLastWriteTime);

	WLog_ERR(TAG, "%s operation not implemented", __FUNCTION__);
	return FALSE;
}

struct _WIN32_FILE_SEARCH
{
	DIR* pDir;
	LPSTR lpPath;
	LPSTR lpPattern;
	struct dirent* pDirent;
};
typedef struct _WIN32_FILE_SEARCH WIN32_FILE_SEARCH;

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
	LPSTR p;
	int index;
	int length;
	struct stat fileStat;
	WIN32_FILE_SEARCH* pFileSearch;
	ZeroMemory(lpFindFileData, sizeof(WIN32_FIND_DATAA));
	pFileSearch = (WIN32_FILE_SEARCH*) calloc(1, sizeof(WIN32_FILE_SEARCH));

	if (!pFileSearch)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return INVALID_HANDLE_VALUE;
	}

	/* Separate lpFileName into path and pattern components */
	p = strrchr(lpFileName, '/');

	if (!p)
		p = strrchr(lpFileName, '\\');

	index = (p - lpFileName);
	length = (p - lpFileName) + 1;
	pFileSearch->lpPath = (LPSTR) malloc(length + 1);

	if (!pFileSearch->lpPath)
	{
		free(pFileSearch);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return INVALID_HANDLE_VALUE;
	}

	CopyMemory(pFileSearch->lpPath, lpFileName, length);
	pFileSearch->lpPath[length] = '\0';
	length = strlen(lpFileName) - index;
	pFileSearch->lpPattern = (LPSTR) malloc(length + 1);

	if (!pFileSearch->lpPattern)
	{
		free(pFileSearch->lpPath);
		free(pFileSearch);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return INVALID_HANDLE_VALUE;
	}

	CopyMemory(pFileSearch->lpPattern, &lpFileName[index + 1], length);
	pFileSearch->lpPattern[length] = '\0';

	/* Check if the path is a directory */

	if (stat(pFileSearch->lpPath, &fileStat) < 0)
	{
		FindClose(pFileSearch);
		return INVALID_HANDLE_VALUE; /* stat error */
	}

	if (S_ISDIR(fileStat.st_mode) == 0)
	{
		FindClose(pFileSearch);
		return INVALID_HANDLE_VALUE; /* not a directory */
	}

	/* Open directory for reading */
	pFileSearch->pDir = opendir(pFileSearch->lpPath);

	if (!pFileSearch->pDir)
	{
		FindClose(pFileSearch);
		return INVALID_HANDLE_VALUE; /* failed to open directory */
	}

	if (FindNextFileA((HANDLE) pFileSearch, lpFindFileData))
		return (HANDLE) pFileSearch;

	FindClose(pFileSearch);
	return INVALID_HANDLE_VALUE;
}

static BOOL ConvertFindDataAToW(LPWIN32_FIND_DATAA lpFindFileDataA,
                                LPWIN32_FIND_DATAW lpFindFileDataW)
{
	int length;
	WCHAR* unicodeFileName;

	if (!lpFindFileDataA || !lpFindFileDataW)
		return FALSE;

	lpFindFileDataW->dwFileAttributes = lpFindFileDataA->dwFileAttributes;
	lpFindFileDataW->ftCreationTime = lpFindFileDataA->ftCreationTime;
	lpFindFileDataW->ftLastAccessTime = lpFindFileDataA->ftLastAccessTime;
	lpFindFileDataW->ftLastWriteTime = lpFindFileDataA->ftLastWriteTime;
	lpFindFileDataW->nFileSizeHigh = lpFindFileDataA->nFileSizeHigh;
	lpFindFileDataW->nFileSizeLow = lpFindFileDataA->nFileSizeLow;
	lpFindFileDataW->dwReserved0 = lpFindFileDataA->dwReserved0;
	lpFindFileDataW->dwReserved1 = lpFindFileDataA->dwReserved1;
	unicodeFileName = NULL;
	length = ConvertToUnicode(CP_UTF8, 0, lpFindFileDataA->cFileName, -1, &unicodeFileName, 0) * 2;

	if (length == 0)
		return FALSE;

	if (length > MAX_PATH)
		length = MAX_PATH;

	CopyMemory(lpFindFileDataW->cFileName, unicodeFileName, length);
	free(unicodeFileName);
	length = ConvertToUnicode(CP_UTF8, 0, lpFindFileDataA->cAlternateFileName,
	                          -1, &unicodeFileName, 0) * 2;

	if (length == 0)
		return TRUE;

	if (length > 14)
		length = 14;

	CopyMemory(lpFindFileDataW->cAlternateFileName, unicodeFileName, length);
	free(unicodeFileName);
	return TRUE;
}

HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
	LPSTR utfFileName = NULL;
	HANDLE h;
	LPWIN32_FIND_DATAA fd = (LPWIN32_FIND_DATAA)calloc(1, sizeof(WIN32_FIND_DATAA));

	if (!fd)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return INVALID_HANDLE_VALUE;
	}

	if (ConvertFromUnicode(CP_UTF8, 0, lpFileName, -1, &utfFileName, 0, NULL, NULL) <= 0)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		free(fd);
		return INVALID_HANDLE_VALUE;
	}

	h = FindFirstFileA(utfFileName, fd);
	free(utfFileName);

	if (h != INVALID_HANDLE_VALUE)
	{
		if (!ConvertFindDataAToW(fd, lpFindFileData))
		{
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			h = INVALID_HANDLE_VALUE;
			goto out;
		}
	}

out:
	free(fd);
	return h;
}

HANDLE FindFirstFileExA(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData,
                        FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
	return INVALID_HANDLE_VALUE;
}

HANDLE FindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData,
                        FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
	return INVALID_HANDLE_VALUE;
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
	WIN32_FILE_SEARCH* pFileSearch;
	struct stat fileStat;
	char* fullpath;
	int pathlen;
	int namelen;
	UINT64 ft;
	ZeroMemory(lpFindFileData, sizeof(WIN32_FIND_DATAA));

	if (!hFindFile)
		return FALSE;

	if (hFindFile == INVALID_HANDLE_VALUE)
		return FALSE;

	pFileSearch = (WIN32_FILE_SEARCH*) hFindFile;

	while ((pFileSearch->pDirent = readdir(pFileSearch->pDir)) != NULL)
	{
		if (FilePatternMatchA(pFileSearch->pDirent->d_name, pFileSearch->lpPattern))
		{
			strcpy(lpFindFileData->cFileName, pFileSearch->pDirent->d_name);
			namelen = strlen(lpFindFileData->cFileName);
			pathlen = strlen(pFileSearch->lpPath);
			fullpath = (char*)malloc(pathlen + namelen + 2);

			if (fullpath == NULL)
			{
				SetLastError(ERROR_NOT_ENOUGH_MEMORY);
				return FALSE;
			}

			memcpy(fullpath, pFileSearch->lpPath, pathlen);
			fullpath[pathlen] = '/';
			memcpy(fullpath + pathlen + 1, pFileSearch->pDirent->d_name, namelen);
			fullpath[pathlen + namelen + 1] = 0;

			if (stat(fullpath, &fileStat) != 0)
			{
				free(fullpath);
				SetLastError(map_posix_err(errno));
				return FALSE;
			}

			free(fullpath);
			lpFindFileData->dwFileAttributes = 0;

			if (S_ISDIR(fileStat.st_mode))
				lpFindFileData->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

			if (lpFindFileData->dwFileAttributes == 0)
				lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_ARCHIVE;

			if (pFileSearch->pDirent->d_name[0] == '.' && namelen != 1 &&
			    (pFileSearch->pDirent->d_name[1] != '.' && namelen != 2))
				lpFindFileData->dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;

			if (!(fileStat.st_mode & S_IWUSR))
				lpFindFileData->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;

#ifdef _DARWIN_FEATURE_64_BIT_INODE
			ft = STAT_TIME_TO_FILETIME(fileStat.st_birthtime);
#else
			ft = STAT_TIME_TO_FILETIME(fileStat.st_ctime);
#endif
			lpFindFileData->ftCreationTime.dwHighDateTime = ((UINT64)ft) >> 32ULL;
			lpFindFileData->ftCreationTime.dwLowDateTime = ft & 0xFFFFFFFF;
			ft = STAT_TIME_TO_FILETIME(fileStat.st_mtime);
			lpFindFileData->ftLastWriteTime.dwHighDateTime = ((UINT64)ft) >> 32ULL;
			lpFindFileData->ftCreationTime.dwLowDateTime = ft & 0xFFFFFFFF;
			ft = STAT_TIME_TO_FILETIME(fileStat.st_atime);
			lpFindFileData->ftLastAccessTime.dwHighDateTime = ((UINT64)ft) >> 32ULL;
			lpFindFileData->ftLastAccessTime.dwLowDateTime = ft & 0xFFFFFFFF;
			lpFindFileData->nFileSizeHigh = ((UINT64)fileStat.st_size) >> 32ULL;
			lpFindFileData->nFileSizeLow = fileStat.st_size & 0xFFFFFFFF;
			return TRUE;
		}
	}

	SetLastError(ERROR_NO_MORE_FILES);
	return FALSE;
}

BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
	LPWIN32_FIND_DATAA fd = (LPWIN32_FIND_DATAA)calloc(1, sizeof(WIN32_FIND_DATAA));

	if (!fd)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	if (FindNextFileA(hFindFile, fd))
	{
		if (!ConvertFindDataAToW(fd, lpFindFileData))
		{
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			free(fd);
			return FALSE;
		}

		free(fd);
		return TRUE;
	}

	free(fd);
	return FALSE;
}

BOOL FindClose(HANDLE hFindFile)
{
	WIN32_FILE_SEARCH* pFileSearch = (WIN32_FILE_SEARCH*) hFindFile;

	if (!pFileSearch || (pFileSearch == INVALID_HANDLE_VALUE))
		return FALSE;

	free(pFileSearch->lpPath);
	free(pFileSearch->lpPattern);

	if (pFileSearch->pDir)
		closedir(pFileSearch->pDir);

	free(pFileSearch);
	return TRUE;
}

BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	if (!mkdir(lpPathName, S_IRUSR | S_IWUSR | S_IXUSR))
		return TRUE;

	return FALSE;
}

BOOL CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	char* utfPathName = NULL;
	BOOL ret;

	if (ConvertFromUnicode(CP_UTF8, 0, lpPathName, -1, &utfPathName, 0, NULL, NULL) <= 0)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	ret = CreateDirectoryA(utfPathName, lpSecurityAttributes);
	free(utfPathName);
	return ret;
}

BOOL RemoveDirectoryA(LPCSTR lpPathName)
{
	int ret = rmdir(lpPathName);

	if (ret != 0)
		SetLastError(map_posix_err(errno));
	else
		SetLastError(STATUS_SUCCESS);

	return ret == 0;
}

BOOL RemoveDirectoryW(LPCWSTR lpPathName)
{
	char* utfPathName = NULL;
	BOOL ret;

	if (ConvertFromUnicode(CP_UTF8, 0, lpPathName, -1, &utfPathName, 0, NULL, NULL) <= 0)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	ret = RemoveDirectoryA(utfPathName);
	free(utfPathName);
	return ret;
}

BOOL MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags)
{
	struct stat st;
	int ret;
	ret = stat(lpNewFileName, &st);

	if ((dwFlags & MOVEFILE_REPLACE_EXISTING) == 0)
	{
		if (ret == 0)
		{
			SetLastError(ERROR_ALREADY_EXISTS);
			return FALSE;
		}
	}
	else
	{
		if (ret == 0 && (st.st_mode & S_IWUSR) == 0)
		{
			SetLastError(ERROR_ACCESS_DENIED);
			return FALSE;
		}
	}

	ret = rename(lpExistingFileName, lpNewFileName);

	if (ret != 0)
		SetLastError(map_posix_err(errno));

	return ret == 0;
}

BOOL MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags)
{
	LPSTR lpCExistingFileName;
	LPSTR lpCNewFileName;
	BOOL ret;

	if (ConvertFromUnicode(CP_UTF8, 0, lpExistingFileName, -1, &lpCExistingFileName, 0, NULL,
	                       NULL) <= 0)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	if (ConvertFromUnicode(CP_UTF8, 0, lpNewFileName, -1, &lpCNewFileName, 0, NULL, NULL) <= 0)
	{
		free(lpCExistingFileName);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}

	ret = MoveFileExA(lpCExistingFileName, lpCNewFileName, dwFlags);
	free(lpCNewFileName);
	free(lpCExistingFileName);
	return ret;
}

BOOL MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
	return MoveFileExA(lpExistingFileName, lpNewFileName, 0);
}

BOOL MoveFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName)
{
	return MoveFileExW(lpExistingFileName, lpNewFileName, 0);
}

#endif

/* Extended API */

int UnixChangeFileMode(const char* filename, int flags)
{
#ifndef _WIN32
	mode_t fl = 0;
	fl |= (flags & 0x4000) ? S_ISUID : 0;
	fl |= (flags & 0x2000) ? S_ISGID : 0;
	fl |= (flags & 0x1000) ? S_ISVTX : 0;
	fl |= (flags & 0x0400) ? S_IRUSR : 0;
	fl |= (flags & 0x0200) ? S_IWUSR : 0;
	fl |= (flags & 0x0100) ? S_IXUSR : 0;
	fl |= (flags & 0x0040) ? S_IRGRP : 0;
	fl |= (flags & 0x0020) ? S_IWGRP : 0;
	fl |= (flags & 0x0010) ? S_IXGRP : 0;
	fl |= (flags & 0x0004) ? S_IROTH : 0;
	fl |= (flags & 0x0002) ? S_IWOTH : 0;
	fl |= (flags & 0x0001) ? S_IXOTH : 0;
	return chmod(filename, fl);
#else
	int rc;
	WCHAR* wfl = NULL;
	int fl = 0;

	if (ConvertToUnicode(CP_UTF8, 0, filename, -1, &wfl, 0) <= 0)
		return -1;

	/* Check for unsupported flags. */
	if (flags & ~(_S_IREAD | _S_IWRITE))
		WLog_WARN(TAG, "Unsupported file mode %d for _wchmod", flags);

	rc = _wchmod(wfl, flags);
	free(wfl);
	return rc;
#endif
}
