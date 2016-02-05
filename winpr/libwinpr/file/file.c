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

#include <winpr/file.h>

#ifndef _WIN32

#include "../log.h"
#define TAG WINPR_TAG("file")

#include <winpr/wlog.h>
#include <winpr/string.h>

#include "file.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>

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
	FileUnlockFileEx
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
	NULL /* FileUnlockFileEx */

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

/* Extended API */

#ifdef _WIN32
#include <io.h>
#endif

HANDLE GetFileHandleForFileDescriptor(int fd)
{
#ifdef _WIN32
	return (HANDLE)_get_osfhandle(fd);
#else /* WIN32 */
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
#endif /* WIN32 */
}


