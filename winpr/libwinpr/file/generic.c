/**
 * WinPR: Windows Portable Runtime
 * File Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Hewlett-Packard Development Company, L.P.
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

#ifndef _WIN32

#include <assert.h>
#include <pthread.h>
#include <dirent.h>

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

static wArrayList *_HandleCreators;

static pthread_once_t _HandleCreatorsInitialized = PTHREAD_ONCE_INIT;


HANDLE_CREATOR *GetNamedPipeClientHandleCreator(void);

#if defined __linux__ && !defined ANDROID
HANDLE_CREATOR *GetCommHandleCreator(void);
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

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
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

	for (i=0; i <= ArrayList_Count(_HandleCreators); i++)
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

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
				   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	LPSTR lpFileNameA = NULL;
	HANDLE hdl;

	if (ConvertFromUnicode(CP_UTF8, 0, lpFileName, -1, &lpFileNameA, 0, NULL, NULL))
		return NULL;

	hdl= CreateFileA(lpFileNameA, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
			dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	free (lpFileNameA);

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

	if (ConvertFromUnicode(CP_UTF8, 0, lpFileName, -1, &lpFileNameA, 0, NULL, NULL))
		return FALSE;
	rc = DeleteFileA(lpFileNameA);
	free (lpFileNameA);

	return rc;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
			  LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	ULONG Type;
	WINPR_HANDLE *handle;

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

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
	if (handle->ops->WriteFileGather)
		return handle->ops->WriteFileGather(handle, aSegmentArray, nNumberOfBytesToWrite,
			lpReserved, lpOverlapped);

	WLog_ERR(TAG, "WriteFileGather operation not implemented");
	return FALSE;
}

BOOL FlushFileBuffers(HANDLE hFile)
{
	ULONG Type;
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
	if (handle->ops->FlushFileBuffers)
		return handle->ops->FlushFileBuffers(handle);

	WLog_ERR(TAG, "FlushFileBuffers operation not implemented");
	return FALSE;
}

BOOL SetEndOfFile(HANDLE hFile)
{
	ULONG Type;
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
	if (handle->ops->SetEndOfFile)
		return handle->ops->SetEndOfFile(handle);

	WLog_ERR(TAG, "SetEndOfFile operation not implemented");
	return FALSE;
}

DWORD WINAPI GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
	ULONG Type;
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
	if (handle->ops->GetFileSize)
		return handle->ops->GetFileSize(handle, lpFileSizeHigh);

	WLog_ERR(TAG, "GetFileSize operation not implemented");
	return 0;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
					 PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	ULONG Type;
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
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
	WINPR_HANDLE *handle;

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!winpr_Handle_GetInfo(hFile, &Type, &handle))
		return FALSE;

	handle = (WINPR_HANDLE *)hFile;
	if (handle->ops->UnlockFileEx)
		return handle->ops->UnlockFileEx(handle, dwReserved,
				nNumberOfBytesToUnlockLow, nNumberOfBytesToUnlockHigh, lpOverlapped);

	WLog_ERR(TAG, "UnLockFileEx operation not implemented");
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
	char* p;
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
	length = (p - lpFileName);
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

	if (lstat(pFileSearch->lpPath, &fileStat) < 0)
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

	while ((pFileSearch->pDirent = readdir(pFileSearch->pDir)) != NULL)
	{
		if ((strcmp(pFileSearch->pDirent->d_name, ".") == 0) || (strcmp(pFileSearch->pDirent->d_name, "..") == 0))
		{
			/* skip "." and ".." */
			continue;
		}

		if (FilePatternMatchA(pFileSearch->pDirent->d_name, pFileSearch->lpPattern))
		{
			strcpy(lpFindFileData->cFileName, pFileSearch->pDirent->d_name);
			return (HANDLE) pFileSearch;
		}
	}

	FindClose(pFileSearch);
	return INVALID_HANDLE_VALUE;
}

HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
	return NULL;
}

HANDLE FindFirstFileExA(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData,
						FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
	return NULL;
}

HANDLE FindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, LPVOID lpFindFileData,
						FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
	return NULL;
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
	WIN32_FILE_SEARCH* pFileSearch;

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
			return TRUE;
		}
	}

	return FALSE;
}

BOOL FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
	return FALSE;
}

BOOL FindClose(HANDLE hFindFile)
{
	WIN32_FILE_SEARCH* pFileSearch;
	pFileSearch = (WIN32_FILE_SEARCH*) hFindFile;

	if (pFileSearch)
	{
		free(pFileSearch->lpPath);
		free(pFileSearch->lpPattern);

		if (pFileSearch->pDir)
			closedir(pFileSearch->pDir);

		free(pFileSearch);
		return TRUE;
	}

	return FALSE;
}

BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	if (!mkdir(lpPathName, S_IRUSR | S_IWUSR | S_IXUSR))
		return TRUE;

	return FALSE;
}

BOOL CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
	return FALSE;
}

BOOL RemoveDirectoryA(LPCSTR lpPathName)
{
	return (rmdir(lpPathName) == 0);
}

BOOL RemoveDirectoryW(LPCWSTR lpPathName)
{
	return FALSE;
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
	return 0;
#endif
}
