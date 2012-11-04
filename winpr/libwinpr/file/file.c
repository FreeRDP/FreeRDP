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
#include <winpr/handle.h>

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

#ifndef _WIN32

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef ANDROID
#include <sys/vfs.h>
#else
#include <sys/statvfs.h>
#endif

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
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hFile, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_ANONYMOUS_PIPE)
	{
		int status;
		int read_fd;

		read_fd = (int) ((ULONG_PTR) Object);

		status = read(read_fd, lpBuffer, nNumberOfBytesToRead);

		*lpNumberOfBytesRead = status;

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
		int write_fd;

		write_fd = (int) ((ULONG_PTR) Object);

		status = write(write_fd, lpBuffer, nNumberOfBytesToWrite);

		*lpNumberOfBytesWritten = status;

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

LPSTR FilePatternFindNextWildcardA(LPCSTR lpPattern, DWORD* pFlags)
{
	LPSTR lpWildcard;

	*pFlags = 0;
	lpWildcard = strpbrk(lpPattern, "*?~");

	if (lpWildcard)
	{
		if (*lpWildcard == '*')
		{
			*pFlags = WILDCARD_STAR;
			return lpWildcard;
		}
		else if (*lpWildcard == '?')
		{
			*pFlags = WILDCARD_QM;
			return lpWildcard;
		}
		else if (*lpWildcard == '~')
		{
			if (lpWildcard[1] == '*')
			{
				*pFlags = WILDCARD_DOS_STAR;
				return lpWildcard;
			}
			else if (lpWildcard[1] == '?')
			{
				*pFlags = WILDCARD_DOS_QM;
				return lpWildcard;
			}
			else if (lpWildcard[1] == '.')
			{
				*pFlags = WILDCARD_DOS_DOT;
				return lpWildcard;
			}
		}
	}

	return NULL;
}

BOOL FilePatternMatchSubExpressionA(LPCSTR lpFileName, size_t cchFileName,
		LPCSTR lpX, size_t cchX, LPCSTR lpY, size_t cchY, LPCSTR lpWildcard, LPSTR* ppMatchEnd)
{
	LPSTR lpMatch;

	if (*lpWildcard == '*')
	{
		/*
		 *                            S
		 *                         <-----<
		 *                      X  |     |  e       Y
		 * X * Y ==        (0)----->-(1)->-----(2)-----(3)
		 */

		/*
		 * State 0: match 'X'
		 */

		if (_strnicmp(lpFileName, lpX, cchX) != 0)
			return FALSE;

		/*
		 * State 1: match 'S' or 'e'
		 *
		 * We use 'e' to transition to state 2
		 */

		/**
		 * State 2: match Y
		 */

		if (cchY != 0)
		{
			/* TODO: case insensitive character search */
			lpMatch = strchr(&lpFileName[cchX], *lpY);

			if (!lpMatch)
				return FALSE;

			if (_strnicmp(lpMatch, lpY, cchY) != 0)
				return FALSE;
		}
		else
		{
			lpMatch = (LPSTR) &lpFileName[cchFileName];
		}

		/**
		 * State 3: final state
		 */

		*ppMatchEnd = (LPSTR) &lpMatch[cchY];

		return TRUE;
	}
	else if (*lpWildcard == '?')
	{
		/**
		 *                     X     S     Y
		 * X ? Y ==        (0)---(1)---(2)---(3)
		 */

		/*
		 * State 0: match 'X'
		 */

		if (cchFileName < cchX)
			return FALSE;

		if (_strnicmp(lpFileName, lpX, cchX) != 0)
			return FALSE;

		/*
		 * State 1: match 'S'
		 */

		/**
		 * State 2: match Y
		 */

		if (cchY != 0)
		{
			/* TODO: case insensitive character search */
			lpMatch = strchr(&lpFileName[cchX + 1], *lpY);

			if (!lpMatch)
				return FALSE;

			if (_strnicmp(lpMatch, lpY, cchY) != 0)
				return FALSE;
			}
		else
		{
			if ((cchX + 1) > cchFileName)
				return FALSE;

			lpMatch = (LPSTR) &lpFileName[cchX + 1];
		}

		/**
		 * State 3: final state
		 */

		*ppMatchEnd = (LPSTR) &lpMatch[cchY];

		return TRUE;
	}
	else if (*lpWildcard == '~')
	{
		printf("warning: unimplemented '~' pattern match\n");

		return TRUE;
	}

	return FALSE;
}

BOOL FilePatternMatchA(LPCSTR lpFileName, LPCSTR lpPattern)
{
	BOOL match;
	LPSTR lpTail;
	size_t cchTail;
	size_t cchPattern;
	size_t cchFileName;
	DWORD dwFlags;
	DWORD dwNextFlags;
	LPSTR lpWildcard;
	LPSTR lpNextWildcard;

	/**
	 * Wild Card Matching
	 *
	 * '*'	matches 0 or more characters
	 * '?'	matches exactly one character
	 *
	 * '~*'	DOS_STAR - matches 0 or more characters until encountering and matching final '.'
	 *
	 * '~?'	DOS_QM - matches any single character, or upon encountering a period or end of name
	 *               string, advances the expresssion to the end of the set of contiguous DOS_QMs.
	 *
	 * '~.'	DOS_DOT - matches either a '.' or zero characters beyond name string.
	 */

	if (!lpPattern)
		return FALSE;

	if (!lpFileName)
		return FALSE;

	cchPattern = strlen(lpPattern);
	cchFileName = strlen(lpFileName);

	/**
	 * First and foremost the file system starts off name matching with the expression “*”.
	 * If the expression contains a single wild card character ‘*’ all matches are satisfied
	 * immediately. This is the most common wild card character used in Windows and expression
	 * evaluation is optimized by looking for this character first.
	 */

	if ((lpPattern[0] == '*') && (cchPattern == 1))
		return TRUE;

	/**
	 * Subsequently evaluation of the “*X” expression is performed. This is a case where
	 * the expression starts off with a wild card character and contains some non-wild card
	 * characters towards the tail end of the name. This is evaluated by making sure the
	 * expression starts off with the character ‘*’ and does not contain any wildcards in
	 * the latter part of the expression. The tail part of the expression beyond the first
	 * character ‘*’ is matched against the file name at the end uppercasing each character
	 * if necessary during the comparison.
	 */

	if (lpPattern[0] == '*')
	{
		lpTail = (LPSTR) &lpPattern[1];
		cchTail = strlen(lpTail);

		if (!FilePatternFindNextWildcardA(lpTail, &dwFlags))
		{
			/* tail contains no wildcards */

			if (cchFileName < cchTail)
				return FALSE;

			if (_stricmp(&lpFileName[cchFileName - cchTail], lpTail) == 0)
				return TRUE;

			return FALSE;
		}
	}

	/**
	 * The remaining expressions are evaluated in a non deterministic
	 * finite order as listed below, where:
	 *
	 * 'S' is any single character
	 * 'S-.' is any single character except the final '.'
	 * 'e' is a null character transition
	 * 'EOF' is the end of the name string
	 *
	 *                            S
	 *                         <-----<
	 *                      X  |     |  e       Y
	 * X * Y ==        (0)----->-(1)->-----(2)-----(3)
	 *
	 *
	 *                           S-.
	 *                         <-----<
	 *                      X  |     |  e       Y
	 * X ~* Y ==       (0)----->-(1)->-----(2)-----(3)
	 *
	 *
	 *                     X     S     S     Y
	 * X ?? Y ==       (0)---(1)---(2)---(3)---(4)
	 *
	 *
	 *                     X     S-.     S-.     Y
	 * X ~?~? ==      (0)---(1)-----(2)-----(3)---(4)
	 *                        |       |_______|
	 *                        |            ^  |
	 *                        |_______________|
	 *                            ^EOF of .^
	 *
	 */

	lpWildcard = FilePatternFindNextWildcardA(lpPattern, &dwFlags);

	if (lpWildcard)
	{
		LPSTR lpX;
		LPSTR lpY;
		size_t cchX;
		size_t cchY;
		LPSTR lpMatchEnd;
		LPSTR lpSubPattern;
		size_t cchSubPattern;
		LPSTR lpSubFileName;
		size_t cchSubFileName;
		size_t cchWildcard;
		size_t cchNextWildcard;

		cchSubPattern = cchPattern;
		lpSubPattern = (LPSTR) lpPattern;

		cchSubFileName = cchFileName;
		lpSubFileName = (LPSTR) lpFileName;

		cchWildcard = ((dwFlags & WILDCARD_DOS) ? 2 : 1);
		lpNextWildcard = FilePatternFindNextWildcardA(&lpWildcard[cchWildcard], &dwNextFlags);

		if (!lpNextWildcard)
		{
			lpX = (LPSTR) lpSubPattern;
			cchX = (lpWildcard - lpSubPattern);

			lpY = (LPSTR) &lpSubPattern[cchX + cchWildcard];
			cchY = (cchSubPattern - (lpY - lpSubPattern));

			match = FilePatternMatchSubExpressionA(lpSubFileName, cchSubFileName,
					lpX, cchX, lpY, cchY, lpWildcard, &lpMatchEnd);

			return match;
		}
		else
		{
			while (lpNextWildcard)
			{
				cchSubFileName = cchFileName - (lpSubFileName - lpFileName);
				cchNextWildcard = ((dwNextFlags & WILDCARD_DOS) ? 2 : 1);

				lpX = (LPSTR) lpSubPattern;
				cchX = (lpWildcard - lpSubPattern);

				lpY = (LPSTR) &lpSubPattern[cchX + cchWildcard];
				cchY = (lpNextWildcard - lpWildcard) - cchWildcard;

				match = FilePatternMatchSubExpressionA(lpSubFileName, cchSubFileName,
						lpX, cchX, lpY, cchY, lpWildcard, &lpMatchEnd);

				if (!match)
					return FALSE;

				lpSubFileName = lpMatchEnd;

				cchWildcard = cchNextWildcard;
				lpWildcard = lpNextWildcard;
				dwFlags = dwNextFlags;

				lpNextWildcard = FilePatternFindNextWildcardA(&lpWildcard[cchWildcard], &dwNextFlags);
			}

			return TRUE;
		}
	}
	else
	{
		/* no wildcard characters */

		if (_stricmp(lpFileName, lpPattern) == 0)
			return TRUE;
	}

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

	ZeroMemory(lpFindFileData, sizeof(LPWIN32_FIND_DATAA));

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
		free(pFileSearch);
		return INVALID_HANDLE_VALUE; /* stat error */
	}

	if (S_ISDIR(fileStat.st_mode) == 0)
	{
		free(pFileSearch);
		return INVALID_HANDLE_VALUE; /* not a directory */
	}

	/* Open directory for reading */

	pFileSearch->pDir = opendir(pFileSearch->lpPath);

	if (!pFileSearch->pDir)
	{
		free(pFileSearch);
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

	free(pFileSearch->lpPath);
	free(pFileSearch->lpPattern);
	closedir(pFileSearch->pDir);

	free(pFileSearch);

	return TRUE;
}

#endif

