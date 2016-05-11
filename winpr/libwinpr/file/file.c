/**
 * WinPR: Windows Portable Runtime
 * File Functions
 *
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 Bernhard Miklautz <bernhard.miklautz@thincast.com>
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
#endif /* HAVE_CONFIG_H */

#include <winpr/crt.h>
#include <winpr/file.h>

#ifdef _WIN32

#include <io.h>

#else /* _WIN32 */

#include "../log.h"
#define TAG WINPR_TAG("file")

#include <winpr/wlog.h>
#include <winpr/string.h>

#include "file.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>

static BOOL FileIsHandled(HANDLE handle)
{
	WINPR_FILE* pFile = (WINPR_FILE*) handle;

	if (!pFile || (pFile->Type != HANDLE_TYPE_FILE))
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

static int FileGetFd(HANDLE handle)
{
	WINPR_FILE *file= (WINPR_FILE*)handle;

	if (!FileIsHandled(handle))
		return -1;

	return fileno(file->fp);
}

static BOOL FileCloseHandle(HANDLE handle) {
	WINPR_FILE* file = (WINPR_FILE *)handle;

	if (!FileIsHandled(handle))
		return FALSE;

	if (file->fp)
	{
		/* Don't close stdin/stdout/stderr */
		if (fileno(file->fp) > 2)
		{
			fclose(file->fp);
			file->fp = NULL;
		}
	}

	free(file->lpFileName);
	free(file);
	return TRUE;
}

static BOOL FileSetEndOfFile(HANDLE hFile)
{
	WINPR_FILE* pFile = (WINPR_FILE*) hFile;
	off_t size;

	if (!hFile)
		return FALSE;

	size = ftell(pFile->fp);
	if (ftruncate(fileno(pFile->fp), size) < 0)
	{
		WLog_ERR(TAG, "ftruncate %s failed with %s [%08X]",
			pFile->lpFileName, strerror(errno), errno);
		return FALSE;
	}

	return TRUE;
}


static DWORD FileSetFilePointer(HANDLE hFile, LONG lDistanceToMove,
			PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	WINPR_FILE* pFile = (WINPR_FILE*) hFile;
	long offset = lDistanceToMove;
	int whence;

	if (!hFile)
		return INVALID_SET_FILE_POINTER;

	switch(dwMoveMethod)
	{
	case FILE_BEGIN:
		whence = SEEK_SET;
		break;
	case FILE_END:
		whence = SEEK_END;
		break;
	case FILE_CURRENT:
		whence = SEEK_CUR;
		break;
	default:
		return INVALID_SET_FILE_POINTER;
	}

	if (fseek(pFile->fp, offset, whence))
	{
		WLog_ERR(TAG, "fseek(%s) failed with %s [%08X]", pFile->lpFileName,
			 strerror(errno), errno);
		return INVALID_SET_FILE_POINTER;
	}

	return ftell(pFile->fp);
}

static BOOL FileRead(PVOID Object, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
					LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	size_t io_status;
	WINPR_FILE* file;
	BOOL status = TRUE;

	if (!Object)
		return FALSE;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "Overlapping write not supported.");
		return FALSE;
	}

	file = (WINPR_FILE *)Object;
	io_status = fread(lpBuffer, nNumberOfBytesToRead, 1, file->fp);

	if (io_status != 1)
	{
		status = FALSE;

		switch (errno)
		{
			case EWOULDBLOCK:
				SetLastError(ERROR_NO_DATA);
				break;
		}
	}

	if (lpNumberOfBytesRead)
		*lpNumberOfBytesRead = nNumberOfBytesToRead;

	return status;
}

static BOOL FileWrite(PVOID Object, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
						LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	size_t io_status;
	WINPR_FILE* file;

	if (!Object)
		return FALSE;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "Overlapping write not supported.");
		return FALSE;
	}

	file = (WINPR_FILE *)Object;

	io_status = fwrite(lpBuffer, nNumberOfBytesToWrite, 1, file->fp);
	if (io_status != 1)
		return FALSE;

	*lpNumberOfBytesWritten = nNumberOfBytesToWrite;
	return TRUE;
}

static DWORD FileGetFileSize(HANDLE Object, LPDWORD lpFileSizeHigh)
{
	WINPR_FILE* file;
	long cur, size;

	if (!Object)
		return 0;

	file = (WINPR_FILE *)Object;

	cur = ftell(file->fp);

	if (cur < 0)
	{
		WLog_ERR(TAG, "ftell(%s) failed with %s [%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	if (fseek(file->fp, 0, SEEK_END) != 0)
	{
		WLog_ERR(TAG, "fseek(%s) failed with %s [%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	size = ftell(file->fp);

	if (size < 0)
	{
		WLog_ERR(TAG, "ftell(%s) failed with %s [%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	if (fseek(file->fp, cur, SEEK_SET) != 0)
	{
		WLog_ERR(TAG, "ftell(%s) failed with %s [%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	if (lpFileSizeHigh)
		*lpFileSizeHigh = 0;

	return size;
}

static BOOL FileLockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved,
		DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh,
		LPOVERLAPPED lpOverlapped)
 {
	int lock;
	WINPR_FILE* pFile = (WINPR_FILE*)hFile;

	if (!hFile)
		return FALSE;

	if (pFile->bLocked)
	{
		WLog_ERR(TAG, "File %s already locked!", pFile->lpFileName);
		return FALSE;
	}

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "lpOverlapped not implemented!");
		return FALSE;
	}

	if (dwFlags & LOCKFILE_EXCLUSIVE_LOCK)
		lock = LOCK_EX;
	else
		lock = LOCK_SH;

	if (dwFlags & LOCKFILE_FAIL_IMMEDIATELY)
		lock |= LOCK_NB;

	if (flock(fileno(pFile->fp), lock) < 0)
	{
		WLog_ERR(TAG, "flock failed with %s [%08X]",
			 strerror(errno), errno);
		return FALSE;
	}

	pFile->bLocked = TRUE;

	return TRUE;
}

static BOOL FileUnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
				DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh)
{
	WINPR_FILE* pFile = (WINPR_FILE*)hFile;

	if (!hFile)
		return FALSE;

	if (!pFile->bLocked)
	{
		WLog_ERR(TAG, "File %s is not locked!", pFile->lpFileName);
		return FALSE;
	}

	if (flock(fileno(pFile->fp), LOCK_UN) < 0)
	{
		WLog_ERR(TAG, "flock(LOCK_UN) %s failed with %s [%08X]",
			 pFile->lpFileName, strerror(errno), errno);
		return FALSE;
	}

	return TRUE;
}

static BOOL FileUnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow,
				  DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped)
{
	WINPR_FILE* pFile = (WINPR_FILE*)hFile;

	if (!hFile)
		return FALSE;

	if (!pFile->bLocked)
	{
		WLog_ERR(TAG, "File %s is not locked!", pFile->lpFileName);
		return FALSE;
	}

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "lpOverlapped not implemented!");
		return FALSE;
	}

	if (flock(fileno(pFile->fp), LOCK_UN) < 0)
	{
		WLog_ERR(TAG, "flock(LOCK_UN) %s failed with %s [%08X]",
			 pFile->lpFileName, strerror(errno), errno);
		return FALSE;
	}

	return TRUE;
}

static BOOL FileSetFileTime(HANDLE hFile, const FILETIME *lpCreationTime,
		const FILETIME *lpLastAccessTime, const FILETIME *lpLastWriteTime)
{
	int rc;
#if defined(__APPLE__) || defined(ANDROID) || defined(__FreeBSD__)
	struct stat buf;
#endif
/* OpenBSD, NetBSD and DragonflyBSD support POSIX futimens */
#if defined(ANDROID) || defined(__FreeBSD__)
	struct timeval timevals[2];
#else
	struct timespec times[2]; /* last access, last modification */
#endif
	WINPR_FILE* pFile = (WINPR_FILE*)hFile;
	const UINT64 EPOCH_DIFF = 11644473600ULL;

	if (!hFile)
		return FALSE;

#if defined(__APPLE__) || defined(ANDROID) || defined(__FreeBSD__)
	rc = fstat(fileno(pFile->fp), &buf);
	if (rc < 0)
		return FALSE;
#endif
	if (!lpLastAccessTime)
	{
#if defined(__APPLE__)
#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
		times[0] = buf.st_atimespec;
#else
		times[0].tv_sec = buf.st_atime;
		times[0].tv_nsec = buf.st_atimensec;
#endif
#elif defined(__FreeBSD__)
		timevals[0].tv_sec = buf.st_atime;
#ifdef _POSIX_SOURCE
		TIMESPEC_TO_TIMEVAL(&timevals[0], &buf.st_atim);
#else
		TIMESPEC_TO_TIMEVAL(&timevals[0], &buf.st_atimespec);
#endif
#elif defined(ANDROID)
		timevals[0].tv_sec = buf.st_atime;
		timevals[0].tv_usec = buf.st_atimensec / 1000UL;
#else
		times[0].tv_sec = UTIME_OMIT;
		times[0].tv_nsec = UTIME_OMIT;
#endif
	}
	else
	{
		UINT64 tmp = ((UINT64)lpLastAccessTime->dwHighDateTime) << 32
				| lpLastAccessTime->dwLowDateTime;
		tmp -= EPOCH_DIFF;
		tmp /= 10ULL;

#if defined(ANDROID) || defined(__FreeBSD__)
		tmp /= 10000ULL;

		timevals[0].tv_sec = tmp / 10000ULL;
		timevals[0].tv_usec = tmp % 10000ULL;
#else
		times[0].tv_sec = tmp / 10000000ULL;
		times[0].tv_nsec = tmp % 10000000ULL;
#endif
	}
	if (!lpLastWriteTime)
	{
#ifdef __APPLE__
#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
		times[1] = buf.st_mtimespec;
#else
		times[1].tv_sec = buf.st_mtime;
		times[1].tv_nsec = buf.st_mtimensec;
#endif
#elif defined(__FreeBSD__)
		timevals[1].tv_sec = buf.st_mtime;
#ifdef _POSIX_SOURCE
		TIMESPEC_TO_TIMEVAL(&timevals[1], &buf.st_mtim);
#else
		TIMESPEC_TO_TIMEVAL(&timevals[1], &buf.st_mtimespec);
#endif
#elif defined(ANDROID)
		timevals[1].tv_sec = buf.st_mtime;
		timevals[1].tv_usec = buf.st_mtimensec / 1000UL;
#else
		times[1].tv_sec = UTIME_OMIT;
		times[1].tv_nsec = UTIME_OMIT;
#endif
	}
	else
	{
		UINT64 tmp = ((UINT64)lpLastWriteTime->dwHighDateTime) << 32
				| lpLastWriteTime->dwLowDateTime;
		tmp -= EPOCH_DIFF;
		tmp /= 10ULL;

#if defined(ANDROID) || defined(__FreeBSD__)
		tmp /= 10000ULL;

		timevals[1].tv_sec = tmp / 10000ULL;
		timevals[1].tv_usec = tmp % 10000ULL;
#else
		times[1].tv_sec = tmp / 10000000ULL;
		times[1].tv_nsec = tmp % 10000000ULL;
#endif
	}

	// TODO: Creation time can not be handled!
#ifdef __APPLE__
	rc = futimes(fileno(pFile->fp), times);
#elif defined(ANDROID) || defined(__FreeBSD__)
	rc = utimes(pFile->lpFileName, timevals);
#else
	rc = futimens(fileno(pFile->fp), times);
#endif
	if (rc != 0)
		return FALSE;

	return TRUE;

}

static HANDLE_OPS fileOps = {
	FileIsHandled,
	FileCloseHandle,
	FileGetFd,
	NULL, /* CleanupHandle */
	FileRead,
	NULL, /* FileReadEx */
	NULL, /* FileReadScatter */
	FileWrite,
	NULL, /* FileWriteEx */
	NULL, /* FileWriteGather */
	FileGetFileSize,
	NULL, /*  FlushFileBuffers */
	FileSetEndOfFile,
	FileSetFilePointer,
	NULL, /* SetFilePointerEx */
	NULL, /* FileLockFile */
	FileLockFileEx,
	FileUnlockFile,
	FileUnlockFileEx,
	FileSetFileTime
};

static HANDLE_OPS shmOps = {
	FileIsHandled,
	FileCloseHandle,
	FileGetFd,
	NULL, /* CleanupHandle */
	FileRead,
	NULL, /* FileReadEx */
	NULL, /* FileReadScatter */
	FileWrite,
	NULL, /* FileWriteEx */
	NULL, /* FileWriteGather */
	NULL, /* FileGetFileSize */
	NULL, /*  FlushFileBuffers */
	NULL, /* FileSetEndOfFile */
	NULL, /* FileSetFilePointer */
	NULL, /* SetFilePointerEx */
	NULL, /* FileLockFile */
	NULL, /* FileLockFileEx */
	NULL, /* FileUnlockFile */
	NULL, /* FileUnlockFileEx */
	NULL  /* FileSetFileTime */
};


static const char* FileGetMode(DWORD dwDesiredAccess, DWORD dwCreationDisposition, BOOL* create)
{
	BOOL writeable = dwDesiredAccess & GENERIC_WRITE;

	switch(dwCreationDisposition)
	{
	case CREATE_ALWAYS:
		*create = TRUE;
		return (writeable) ? "wb+" : "rwb";
	case CREATE_NEW:
		*create = TRUE;
		return "wb+";
	case OPEN_ALWAYS:
		*create = TRUE;
		return "rb+";
	case OPEN_EXISTING:
		*create = FALSE;
		return "rb+";
	case TRUNCATE_EXISTING:
		*create = FALSE;
		return "wb+";
	default:
		*create = FALSE;
		return "";
	}
}

static HANDLE FileCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
				  DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	WINPR_FILE* pFile;
	BOOL create;
	const char* mode = FileGetMode(dwDesiredAccess, dwCreationDisposition, &create);
	int lock = 0;
	FILE* fp = NULL;

	pFile = (WINPR_FILE*) calloc(1, sizeof(WINPR_FILE));
	if (!pFile)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return INVALID_HANDLE_VALUE;
	}

	WINPR_HANDLE_SET_TYPE_AND_MODE(pFile, HANDLE_TYPE_FILE, WINPR_FD_READ);
	pFile->ops = &fileOps;

	pFile->lpFileName = _strdup(lpFileName);
	if (!pFile->lpFileName)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		free(pFile);
		return INVALID_HANDLE_VALUE;
	}

	pFile->dwOpenMode = dwDesiredAccess;
	pFile->dwShareMode = dwShareMode;
	pFile->dwFlagsAndAttributes = dwFlagsAndAttributes;
	pFile->lpSecurityAttributes = lpSecurityAttributes;
	pFile->dwCreationDisposition = dwCreationDisposition;
	pFile->hTemplateFile = hTemplateFile;

	if (create)
	{
		fp = fopen(pFile->lpFileName, "ab");
		if (!fp)
		{
			free(pFile->lpFileName);
			free(pFile);
			return INVALID_HANDLE_VALUE;
		}

		fp = freopen(pFile->lpFileName, mode, fp);
	}

	if (NULL == fp)
		fp = fopen(pFile->lpFileName, mode);

	pFile->fp = fp;
	if (!pFile->fp)
	{
		/* This case can occur when trying to open a
		 * not existing file without create flag. */
		free(pFile->lpFileName);
		free(pFile);
		return INVALID_HANDLE_VALUE;
	}

	setvbuf(fp, NULL, _IONBF, 0);

	if (dwShareMode & FILE_SHARE_READ)
		lock = LOCK_SH;
	if (dwShareMode & FILE_SHARE_WRITE)
		lock = LOCK_EX;

	if (dwShareMode & (FILE_SHARE_READ | FILE_SHARE_WRITE))
	{
		if (flock(fileno(pFile->fp), lock) < 0)
		{
			WLog_ERR(TAG, "flock failed with %s [%08X]",
				 strerror(errno), errno);
			FileCloseHandle(pFile);
			return INVALID_HANDLE_VALUE;
		}

		pFile->bLocked = TRUE;
	}

	return pFile;
}

BOOL IsFileDevice(LPCTSTR lpDeviceName)
{
	return TRUE;
}

HANDLE_CREATOR _FileHandleCreator =
{
	IsFileDevice,
	FileCreateFileA
};

HANDLE_CREATOR *GetFileHandleCreator(void)
{
	return &_FileHandleCreator;
}


static WINPR_FILE *FileHandle_New(FILE* fp)
{
	WINPR_FILE *pFile;
	char name[MAX_PATH];

	_snprintf(name, sizeof(name), "device_%d", fileno(fp));
	pFile = (WINPR_FILE*) calloc(1, sizeof(WINPR_FILE));
	if (!pFile)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}
	pFile->fp = fp;
	pFile->ops = &shmOps;
	pFile->lpFileName = _strdup(name);

	WINPR_HANDLE_SET_TYPE_AND_MODE(pFile, HANDLE_TYPE_FILE, WINPR_FD_READ);
	return pFile;
}

HANDLE GetStdHandle(DWORD nStdHandle)
{
	FILE* fp;
	WINPR_FILE *pFile;

	switch (nStdHandle)
	{
		case STD_INPUT_HANDLE:
			fp = stdin;
			break;
		case STD_OUTPUT_HANDLE:
			fp = stdout;
			break;
		case STD_ERROR_HANDLE:
			fp = stderr;
			break;
		default:
			return INVALID_HANDLE_VALUE;
	}
	pFile = FileHandle_New(fp);
	if (!pFile)
		return INVALID_HANDLE_VALUE;

	return (HANDLE)pFile;
}

BOOL SetStdHandle(DWORD nStdHandle, HANDLE hHandle)
{
	return FALSE;
}

BOOL SetStdHandleEx(DWORD dwStdHandle, HANDLE hNewHandle, HANDLE* phOldHandle)
{
	return FALSE;
}

#endif /* _WIN32 */

#ifdef _UWP

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	HANDLE hFile;
	CREATEFILE2_EXTENDED_PARAMETERS params;

	ZeroMemory(&params, sizeof(CREATEFILE2_EXTENDED_PARAMETERS));

	params.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);

	if (dwFlagsAndAttributes & FILE_FLAG_BACKUP_SEMANTICS) params.dwFileFlags |= FILE_FLAG_BACKUP_SEMANTICS;
	if (dwFlagsAndAttributes & FILE_FLAG_DELETE_ON_CLOSE) params.dwFileFlags |= FILE_FLAG_DELETE_ON_CLOSE;
	if (dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING) params.dwFileFlags |= FILE_FLAG_NO_BUFFERING;
	if (dwFlagsAndAttributes & FILE_FLAG_OPEN_NO_RECALL) params.dwFileFlags |= FILE_FLAG_OPEN_NO_RECALL;
	if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT) params.dwFileFlags |= FILE_FLAG_OPEN_REPARSE_POINT;
	if (dwFlagsAndAttributes & FILE_FLAG_OPEN_REQUIRING_OPLOCK) params.dwFileFlags |= FILE_FLAG_OPEN_REQUIRING_OPLOCK;
	if (dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED) params.dwFileFlags |= FILE_FLAG_OVERLAPPED;
	if (dwFlagsAndAttributes & FILE_FLAG_POSIX_SEMANTICS) params.dwFileFlags |= FILE_FLAG_POSIX_SEMANTICS;
	if (dwFlagsAndAttributes & FILE_FLAG_RANDOM_ACCESS) params.dwFileFlags |= FILE_FLAG_RANDOM_ACCESS;
	if (dwFlagsAndAttributes & FILE_FLAG_SESSION_AWARE) params.dwFileFlags |= FILE_FLAG_SESSION_AWARE;
	if (dwFlagsAndAttributes & FILE_FLAG_SEQUENTIAL_SCAN) params.dwFileFlags |= FILE_FLAG_SEQUENTIAL_SCAN;
	if (dwFlagsAndAttributes & FILE_FLAG_WRITE_THROUGH) params.dwFileFlags |= FILE_FLAG_WRITE_THROUGH;

	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_ARCHIVE) params.dwFileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_COMPRESSED) params.dwFileAttributes |= FILE_ATTRIBUTE_COMPRESSED;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_DEVICE) params.dwFileAttributes |= FILE_ATTRIBUTE_DEVICE;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_DIRECTORY) params.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_ENCRYPTED) params.dwFileAttributes |= FILE_ATTRIBUTE_ENCRYPTED;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_HIDDEN) params.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_INTEGRITY_STREAM) params.dwFileAttributes |= FILE_ATTRIBUTE_INTEGRITY_STREAM;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_NORMAL) params.dwFileAttributes |= FILE_ATTRIBUTE_NORMAL;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) params.dwFileAttributes |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_NO_SCRUB_DATA) params.dwFileAttributes |= FILE_ATTRIBUTE_NO_SCRUB_DATA;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_OFFLINE) params.dwFileAttributes |= FILE_ATTRIBUTE_OFFLINE;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_READONLY) params.dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_REPARSE_POINT) params.dwFileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_SPARSE_FILE) params.dwFileAttributes |= FILE_ATTRIBUTE_SPARSE_FILE;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_SYSTEM) params.dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_TEMPORARY) params.dwFileAttributes |= FILE_ATTRIBUTE_TEMPORARY;
	if (dwFlagsAndAttributes & FILE_ATTRIBUTE_VIRTUAL) params.dwFileAttributes |= FILE_ATTRIBUTE_VIRTUAL;

	if (dwFlagsAndAttributes & SECURITY_ANONYMOUS) params.dwSecurityQosFlags |= SECURITY_ANONYMOUS;
	if (dwFlagsAndAttributes & SECURITY_CONTEXT_TRACKING) params.dwSecurityQosFlags |= SECURITY_CONTEXT_TRACKING;
	if (dwFlagsAndAttributes & SECURITY_DELEGATION) params.dwSecurityQosFlags |= SECURITY_DELEGATION;
	if (dwFlagsAndAttributes & SECURITY_EFFECTIVE_ONLY) params.dwSecurityQosFlags |= SECURITY_EFFECTIVE_ONLY;
	if (dwFlagsAndAttributes & SECURITY_IDENTIFICATION) params.dwSecurityQosFlags |= SECURITY_IDENTIFICATION;
	if (dwFlagsAndAttributes & SECURITY_IMPERSONATION) params.dwSecurityQosFlags |= SECURITY_IMPERSONATION;

	params.lpSecurityAttributes = lpSecurityAttributes;
	params.hTemplateFile = hTemplateFile;

	hFile = CreateFile2(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, &params);

	return hFile;
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	HANDLE hFile;
	WCHAR* lpFileNameW = NULL;
	
	ConvertToUnicode(CP_UTF8, 0, lpFileName, -1, &lpFileNameW, 0);

	if (!lpFileNameW)
		return NULL;

	hFile = CreateFileW(lpFileNameW, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
			dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

	free(lpFileNameW);

	return hFile;
}

DWORD WINAPI GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
	BOOL status;
	LARGE_INTEGER fileSize = { 0, 0 };

	if (!lpFileSizeHigh)
		return INVALID_FILE_SIZE;

	status = GetFileSizeEx(hFile, &fileSize);

	if (!status)
		return INVALID_FILE_SIZE;

	*lpFileSizeHigh = fileSize.HighPart;

	return fileSize.LowPart;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
	PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	BOOL status;
	LARGE_INTEGER liDistanceToMove = { 0, 0 };
	LARGE_INTEGER liNewFilePointer = { 0, 0 };

	liDistanceToMove.LowPart = lDistanceToMove;

	status = SetFilePointerEx(hFile, liDistanceToMove, &liNewFilePointer, dwMoveMethod);

	if (!status)
		return INVALID_SET_FILE_POINTER;

	if (lpDistanceToMoveHigh)
		*lpDistanceToMoveHigh = liNewFilePointer.HighPart;

	return liNewFilePointer.LowPart;
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
	return FindFirstFileExA(lpFileName, FindExInfoStandard, lpFindFileData, FindExSearchNameMatch, NULL, 0);
}

HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
	return FindFirstFileExW(lpFileName, FindExInfoStandard, lpFindFileData, FindExSearchNameMatch, NULL, 0);
}

DWORD GetFullPathNameA(LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart)
{
	DWORD dwStatus;
	WCHAR* lpFileNameW = NULL;
	WCHAR* lpBufferW = NULL;
	WCHAR* lpFilePartW = NULL;
	DWORD nBufferLengthW = nBufferLength * 2;

	if (!lpFileName || (nBufferLength < 1))
		return 0;

	ConvertToUnicode(CP_UTF8, 0, lpFileName, -1, &lpFileNameW, 0);

	if (!lpFileNameW)
		return 0;

	lpBufferW = (WCHAR*) malloc(nBufferLengthW);

	if (!lpBufferW)
		return 0;

	dwStatus = GetFullPathNameW(lpFileNameW, nBufferLengthW, lpBufferW, &lpFilePartW);

	ConvertFromUnicode(CP_UTF8, 0, lpBufferW, nBufferLengthW, &lpBuffer, nBufferLength, NULL, NULL);

	if (lpFilePart)
		lpFilePart = lpBuffer + (lpFilePartW - lpBufferW);

	free(lpFileNameW);
	free(lpBufferW);

	return dwStatus * 2;
}

BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
	LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
	BOOL status;
	ULARGE_INTEGER FreeBytesAvailableToCaller = { 0, 0 };
	ULARGE_INTEGER TotalNumberOfBytes = { 0, 0 };
	ULARGE_INTEGER TotalNumberOfFreeBytes = { 0, 0 };

	status = GetDiskFreeSpaceExA(lpRootPathName, &FreeBytesAvailableToCaller,
			&TotalNumberOfBytes, &TotalNumberOfFreeBytes);

	if (!status)
		return FALSE;

	*lpBytesPerSector = 1;
	*lpSectorsPerCluster = TotalNumberOfBytes.LowPart;
	*lpNumberOfFreeClusters = FreeBytesAvailableToCaller.LowPart;
	*lpTotalNumberOfClusters = TotalNumberOfFreeBytes.LowPart;

	return TRUE;
}

BOOL GetDiskFreeSpaceW(LPCWSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
	LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
	BOOL status;
	ULARGE_INTEGER FreeBytesAvailableToCaller = { 0, 0 };
	ULARGE_INTEGER TotalNumberOfBytes = { 0, 0 };
	ULARGE_INTEGER TotalNumberOfFreeBytes = { 0, 0 };

	status = GetDiskFreeSpaceExW(lpRootPathName, &FreeBytesAvailableToCaller,
		&TotalNumberOfBytes, &TotalNumberOfFreeBytes);

	if (!status)
		return FALSE;

	*lpBytesPerSector = 1;
	*lpSectorsPerCluster = TotalNumberOfBytes.LowPart;
	*lpNumberOfFreeClusters = FreeBytesAvailableToCaller.LowPart;
	*lpTotalNumberOfClusters = TotalNumberOfFreeBytes.LowPart;

	return TRUE;
}

DWORD GetLogicalDriveStringsA(DWORD nBufferLength, LPSTR lpBuffer)
{
	SetLastError(ERROR_INVALID_FUNCTION);
	return 0;
}

DWORD GetLogicalDriveStringsW(DWORD nBufferLength, LPWSTR lpBuffer)
{
	SetLastError(ERROR_INVALID_FUNCTION);
	return 0;
}

BOOL PathIsDirectoryEmptyA(LPCSTR pszPath)
{
	return FALSE;
}

UINT GetACP(void)
{
	return CP_UTF8;
}

#endif

/* Extended API */

#ifdef _WIN32
#include <io.h>
#endif

HANDLE GetFileHandleForFileDescriptor(int fd)
{
#ifdef _WIN32
	return (HANDLE)_get_osfhandle(fd);
#else /* _WIN32 */
	WINPR_FILE *pFile;
	FILE* fp;
	int flags;

	/* Make sure it's a valid fd */
	if (fcntl(fd, F_GETFD) == -1 && errno == EBADF)
		return INVALID_HANDLE_VALUE;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		return INVALID_HANDLE_VALUE;

	if (flags & O_WRONLY)
		fp = fdopen(fd, "wb");
	else
		fp = fdopen(fd, "rb");

	if (!fp)
		return INVALID_HANDLE_VALUE;

	setvbuf(fp, NULL, _IONBF, 0);

	pFile = FileHandle_New(fp);
	if (!pFile)
		return INVALID_HANDLE_VALUE;

	return (HANDLE)pFile;
#endif /* _WIN32 */
}


