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

	return file->fd;
}

static BOOL FileCloseHandle(HANDLE handle) {
	WINPR_FILE* file = (WINPR_FILE *)handle;

	if (!FileIsHandled(handle))
		return FALSE;

	if (file->fd != -1)
	{

		/* Don't close stdin/stdout/stderr */
		if (file->fd > 2)
		{
			close(file->fd);
			file->fd = -1;
		}
	}

	free(file->lpFileName);
	free(file);
	return TRUE;
}

static BOOL FileSetEndOfFile(HANDLE hFile)
{
	WINPR_FILE* pFile = (WINPR_FILE*) hFile;
	DWORD lowSize, highSize;
	off_t size;

	if (!hFile)
		return FALSE;

	lowSize = GetFileSize(hFile, &highSize);
	if (lowSize == INVALID_FILE_SIZE)
		return FALSE;

	size = lowSize | ((off_t)highSize << 32);
	if (ftruncate(pFile->fd, size) < 0)
	{
		WLog_ERR(TAG, "ftruncate %d failed with %s [%08X]",
			pFile->fd, strerror(errno), errno);
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
	FILE* fp;

	if (!hFile)
		return INVALID_SET_FILE_POINTER;

	fp = fdopen(pFile->fd, "wb");

	if (!fp)
	{
		WLog_ERR(TAG, "fdopen(%s) failed with %s [%08X]", pFile->lpFileName,
			 strerror(errno), errno);
		return INVALID_SET_FILE_POINTER;
	}

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

	if (fseek(fp, offset, whence))
	{
		WLog_ERR(TAG, "fseek(%s) failed with %s [%08X]", pFile->lpFileName,
			 strerror(errno), errno);
		return INVALID_SET_FILE_POINTER;
	}

	return ftell(fp);
}

static BOOL FileRead(PVOID Object, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
					LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	int io_status;
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
	do
	{
		io_status = read(file->fd, lpBuffer, nNumberOfBytesToRead);
	}
	while ((io_status < 0) && (errno == EINTR));

	if (io_status < 0)
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
		*lpNumberOfBytesRead = io_status;

	return status;
}

static BOOL FileWrite(PVOID Object, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
						LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	int io_status;
	WINPR_FILE* file;

	if (!Object)
		return FALSE;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "Overlapping write not supported.");
		return FALSE;
	}

	file = (WINPR_FILE *)Object;

	do
	{
		io_status = write(file->fd, lpBuffer, nNumberOfBytesToWrite);
	}
	while ((io_status < 0) && (errno == EINTR));

	if ((io_status < 0) && (errno == EWOULDBLOCK))
		io_status = 0;

	*lpNumberOfBytesWritten = io_status;
	return TRUE;
}

static DWORD FileGetFileSize(HANDLE Object, LPDWORD lpFileSizeHigh)
{
	WINPR_FILE* file;
	FILE* fp;
	long cur, size;

	if (!Object)
		return 0;

	file = (WINPR_FILE *)Object;
	fp = fdopen(file->fd, "wb");

	if (!fp)
	{
		WLog_ERR(TAG, "fopen(%s) failed with %s [%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	cur = ftell(fp);

	if (cur < 0)
	{
		WLog_ERR(TAG, "ftell(%s) failed with %s [%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	if (fseek(fp, 0, SEEK_END) != 0)
	{
		WLog_ERR(TAG, "fseek(%s) failed with %s [%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	size = ftell(fp);

	if (size < 0)
	{
		WLog_ERR(TAG, "ftell(%s) failed with %s [%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	if (fseek(fp, cur, SEEK_SET) != 0)
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

	if (flock(pFile->fd, lock) < 0)
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

	if (flock(pFile->fd, LOCK_UN) < 0)
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

	if (flock(pFile->fd, LOCK_UN) < 0)
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
	int lock;

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
		FILE* fp = fopen(pFile->lpFileName, "ab");
		if (!fp)
		{
			free(pFile->lpFileName);
			free(pFile);
			return INVALID_HANDLE_VALUE;
		}
		fclose(fp);
	}

	{
		FILE* fp = fopen(pFile->lpFileName, mode);
		pFile->fd = -1;
		if (fp)
			pFile->fd = fileno(fp);
	}
	if (pFile->fd < 0)
	{
		WLog_ERR(TAG, "Failed to open file %s with mode %s",
			 pFile->lpFileName, mode);

		free(pFile->lpFileName);
		free(pFile);
		return INVALID_HANDLE_VALUE;
	}

	if (dwShareMode & FILE_SHARE_READ)
		lock = LOCK_SH;
	if (dwShareMode & FILE_SHARE_WRITE)
		lock = LOCK_EX;

	if (dwShareMode & (FILE_SHARE_READ | FILE_SHARE_WRITE))
	{
		if (flock(pFile->fd, lock) < 0)
		{
			WLog_ERR(TAG, "flock failed with %s [%08X]",
				 strerror(errno), errno);
			close(pFile->fd);
			free(pFile->lpFileName);
			free(pFile);
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


static WINPR_FILE *FileHandle_New(int fd)
{
	WINPR_FILE *pFile;
	HANDLE hFile;
	char name[MAX_PATH];

	_snprintf(name, sizeof(name), "device_%d", fd);
	pFile = (WINPR_FILE*) calloc(1, sizeof(WINPR_FILE));
	if (!pFile)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}
	pFile->fd = fd;
	pFile->ops = &shmOps;
	pFile->lpFileName = _strdup(name);

	hFile = (HANDLE) pFile;
	WINPR_HANDLE_SET_TYPE_AND_MODE(pFile, HANDLE_TYPE_FILE, WINPR_FD_READ);
	return pFile;
}

HANDLE GetStdHandle(DWORD nStdHandle)
{
	int fd;
	WINPR_FILE *pFile;
	switch (nStdHandle)
	{
		case STD_INPUT_HANDLE:
			fd = STDIN_FILENO;
			break;
		case STD_OUTPUT_HANDLE:
			fd = STDOUT_FILENO;
			break;
		case STD_ERROR_HANDLE:
			fd = STDERR_FILENO;
			break;
		default:
			return INVALID_HANDLE_VALUE;
	}
	pFile = FileHandle_New(fd);
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

HANDLE GetFileHandleForFileDescriptor(int fd)
{
#ifdef WIN32
	return (HANDLE)_get_osfhandle(fd);
#else /* WIN32 */
	WINPR_FILE *pFile;

	/* Make sure it's a valid fd */
	if (fcntl(fd, F_GETFD) == -1 && errno == EBADF)
		return INVALID_HANDLE_VALUE;

	pFile = FileHandle_New(fd);
	if (!pFile)
		return INVALID_HANDLE_VALUE;
	pFile->fd = fd;
	return (HANDLE)pFile;
#endif /* WIN32 */
}

