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

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/handle.h>
#include <winpr/platform.h>

#include <winpr/file.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

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

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	char* name;
	int status;
	HANDLE hNamedPipe;
	unsigned long flags;
	struct sockaddr_un s;
	WINPR_NAMED_PIPE* pNamedPipe;

	if (!lpFileName)
		return INVALID_HANDLE_VALUE;

	name = GetNamedPipeNameWithoutPrefixA(lpFileName);

	if (!name)
		return INVALID_HANDLE_VALUE;

	free(name);

	pNamedPipe = (WINPR_NAMED_PIPE*) malloc(sizeof(WINPR_NAMED_PIPE));
	hNamedPipe = (HANDLE) pNamedPipe;

	WINPR_HANDLE_SET_TYPE(pNamedPipe, HANDLE_TYPE_NAMED_PIPE);

	pNamedPipe->name = _strdup(lpFileName);
	pNamedPipe->dwOpenMode = 0;
	pNamedPipe->dwPipeMode = 0;
	pNamedPipe->nMaxInstances = 0;
	pNamedPipe->nOutBufferSize = 0;
	pNamedPipe->nInBufferSize = 0;
	pNamedPipe->nDefaultTimeOut = 0;
	pNamedPipe->dwFlagsAndAttributes = dwFlagsAndAttributes;

	pNamedPipe->lpFileName = GetNamedPipeNameWithoutPrefixA(lpFileName);
	pNamedPipe->lpFilePath = GetNamedPipeUnixDomainSocketFilePathA(lpFileName);

	pNamedPipe->clientfd = socket(PF_LOCAL, SOCK_STREAM, 0);
	pNamedPipe->serverfd = -1;

	if (0)
	{
		flags = fcntl(pNamedPipe->clientfd, F_GETFL);
		flags = flags | O_NONBLOCK;
		fcntl(pNamedPipe->clientfd, F_SETFL, flags);
	}

	ZeroMemory(&s, sizeof(struct sockaddr_un));
	s.sun_family = AF_UNIX;
	strcpy(s.sun_path, pNamedPipe->lpFilePath);

	status = connect(pNamedPipe->clientfd, (struct sockaddr*) &s, sizeof(struct sockaddr_un));

	if (status != 0)
	{
		close(pNamedPipe->clientfd);
		free(pNamedPipe);
		return INVALID_HANDLE_VALUE;
	}

	return hNamedPipe;
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
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hFile, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_ANONYMOUS_PIPE)
	{
		int status;
		WINPR_PIPE* pipe;

		pipe = (WINPR_PIPE*) Object;

		status = read(pipe->fd, lpBuffer, nNumberOfBytesToRead);

		*lpNumberOfBytesRead = status;

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		int status;
		WINPR_NAMED_PIPE* pipe;

		pipe = (WINPR_NAMED_PIPE*) Object;

		if (!(pipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
		{
			status = nNumberOfBytesToRead;

			if (pipe->clientfd == -1)
				return FALSE;

			status = read(pipe->clientfd, lpBuffer, nNumberOfBytesToRead);

			if (status < 0)
			{
				*lpNumberOfBytesRead = 0;
				return FALSE;
			}

			*lpNumberOfBytesRead = status;
		}
		else
		{
			/* Overlapped I/O */

			if (!lpOverlapped)
				return FALSE;

			if (pipe->clientfd == -1)
				return FALSE;

			pipe->lpOverlapped = lpOverlapped;

#ifdef HAVE_AIO_H
			{
				struct aiocb cb;

				ZeroMemory(&cb, sizeof(struct aiocb));
				cb.aio_nbytes = nNumberOfBytesToRead;
				cb.aio_fildes = pipe->clientfd;
				cb.aio_offset = lpOverlapped->Offset;
				cb.aio_buf = lpBuffer;

				status = aio_read(&cb);

				printf("aio_read status: %d\n", status);

				if (status < 0)
				{
					return FALSE;
				}
			}
#else

			/* synchronous behavior */

			lpOverlapped->Internal = 0;
			lpOverlapped->InternalHigh = (ULONG_PTR) nNumberOfBytesToRead;
			lpOverlapped->Pointer = (PVOID) lpBuffer;

			SetEvent(lpOverlapped->hEvent);

#endif
		}

		return TRUE;
	}

	return FALSE;
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
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hFile, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_ANONYMOUS_PIPE)
	{
		int status;
		WINPR_PIPE* pipe;

		pipe = (WINPR_PIPE*) Object;

		status = write(pipe->fd, lpBuffer, nNumberOfBytesToWrite);

		*lpNumberOfBytesWritten = status;

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		int status;
		WINPR_NAMED_PIPE* pipe;

		pipe = (WINPR_NAMED_PIPE*) Object;

		if (!(pipe->dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
		{
			status = nNumberOfBytesToWrite;

			if (pipe->clientfd == -1)
				return FALSE;

			status = write(pipe->clientfd, lpBuffer, nNumberOfBytesToWrite);

			if (status < 0)
			{
				*lpNumberOfBytesWritten = 0;
				return FALSE;
			}

			*lpNumberOfBytesWritten = status;
		}
		else
		{
			/* Overlapped I/O */

			if (!lpOverlapped)
				return FALSE;

			if (pipe->clientfd == -1)
				return FALSE;

			pipe->lpOverlapped = lpOverlapped;

#ifdef HAVE_AIO_H
			{
				struct aiocb cb;

				ZeroMemory(&cb, sizeof(struct aiocb));
				cb.aio_nbytes = nNumberOfBytesToWrite;
				cb.aio_fildes = pipe->clientfd;
				cb.aio_offset = lpOverlapped->Offset;
				cb.aio_buf = lpBuffer;

				status = aio_write(&cb);

				printf("aio_write status: %d\n", status);

				if (status < 0)
				{
					return FALSE;
				}
			}
#else

			/* synchronous behavior */

			lpOverlapped->Internal = 1;
			lpOverlapped->InternalHigh = (ULONG_PTR) nNumberOfBytesToWrite;
			lpOverlapped->Pointer = (PVOID) lpBuffer;

			SetEvent(lpOverlapped->hEvent);

#endif
		}

		return TRUE;
	}

	return FALSE;
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

	pFileSearch = (WIN32_FILE_SEARCH*) malloc(sizeof(WIN32_FILE_SEARCH));
	ZeroMemory(pFileSearch, sizeof(WIN32_FILE_SEARCH));

	/* Separate lpFileName into path and pattern components */

	p = strrchr(lpFileName, '/');

	if (!p)
		p = strrchr(lpFileName, '\\');

	index = (p - lpFileName);
	length = (p - lpFileName);
	pFileSearch->lpPath = (LPSTR) malloc(length + 1);
	CopyMemory(pFileSearch->lpPath, lpFileName, length);
	pFileSearch->lpPath[length] = '\0';

	length = strlen(lpFileName) - index;
	pFileSearch->lpPattern = (LPSTR) malloc(length + 1);
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
		if (pFileSearch->lpPath)
			free(pFileSearch->lpPath);
		if (pFileSearch->lpPattern)
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
	return TRUE;
}

#endif

/* Extended API */

#define NAMED_PIPE_PREFIX_PATH		"\\\\.\\pipe\\"

char* GetNamedPipeNameWithoutPrefixA(LPCSTR lpName)
{
	char* lpFileName;

	if (!lpName)
		return NULL;

	if (strncmp(lpName, NAMED_PIPE_PREFIX_PATH, sizeof(NAMED_PIPE_PREFIX_PATH) - 1) != 0)
		return NULL;

	lpFileName = _strdup(&lpName[strlen(NAMED_PIPE_PREFIX_PATH)]);

	return lpFileName;
}

char* GetNamedPipeUnixDomainSocketBaseFilePathA()
{
	char* lpTempPath;
	char* lpPipePath;

	lpTempPath = GetKnownPath(KNOWN_PATH_TEMP);
	lpPipePath = GetCombinedPath(lpTempPath, ".pipe");

	free(lpTempPath);

	return lpPipePath;
}

char* GetNamedPipeUnixDomainSocketFilePathA(LPCSTR lpName)
{
	char* lpPipePath;
	char* lpFileName;
	char* lpFilePath;

	lpPipePath = GetNamedPipeUnixDomainSocketBaseFilePathA();

	lpFileName = GetNamedPipeNameWithoutPrefixA(lpName);
	lpFilePath = GetCombinedPath(lpPipePath, (char*) lpFileName);

	free(lpPipePath);
	free(lpFileName);

	return lpFilePath;
}

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
