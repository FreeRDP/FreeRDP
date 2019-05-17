/**
 * WinPR: Windows Portable Runtime
 * File Functions
 *
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 Bernhard Miklautz <bernhard.miklautz@thincast.com>
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
#endif /* HAVE_CONFIG_H */

#if defined(__FreeBSD_kernel__) && defined(__GLIBC__)
#define _GNU_SOURCE
#define KFREEBSD
#endif

#include <winpr/wtypes.h>
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

#ifdef ANDROID
#include <sys/vfs.h>
#else
#include <sys/statvfs.h>
#endif


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
	INT64 size;

	if (!hFile)
		return FALSE;

	size = _ftelli64(pFile->fp);

	if (ftruncate(fileno(pFile->fp), size) < 0)
	{
		WLog_ERR(TAG, "ftruncate %s failed with %s [0x%08X]",
			pFile->lpFileName, strerror(errno), errno);
		SetLastError(map_posix_err(errno));
		return FALSE;
	}

	return TRUE;
}


static DWORD FileSetFilePointer(HANDLE hFile, LONG lDistanceToMove,
			PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
	WINPR_FILE* pFile = (WINPR_FILE*) hFile;
	INT64 offset;
	int whence;

	if (!hFile)
		return INVALID_SET_FILE_POINTER;

	/* If there is a high part, the sign is contained in that
	 * and the low integer must be interpreted as unsigned. */
	if (lpDistanceToMoveHigh)
	{
		offset = (INT64)(((UINT64)*lpDistanceToMoveHigh << 32U) | (UINT64)lDistanceToMove);
	}
	else
		 offset = lDistanceToMove;

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

	if (_fseeki64(pFile->fp, offset, whence))
	{
		WLog_ERR(TAG, "_fseeki64(%s) failed with %s [0x%08X]", pFile->lpFileName,
			 strerror(errno), errno);
		return INVALID_SET_FILE_POINTER;
	}

	return _ftelli64(pFile->fp);
}

static BOOL FileSetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod)
{
	WINPR_FILE* pFile = (WINPR_FILE*) hFile;
	int whence;

	if (!hFile)
		return FALSE;

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
		return FALSE;
	}

	if (_fseeki64(pFile->fp, liDistanceToMove.QuadPart, whence))
	{
		WLog_ERR(TAG, "_fseeki64(%s) failed with %s [0x%08X]", pFile->lpFileName,
			 strerror(errno), errno);
		return FALSE;
	}

	if (lpNewFilePointer)
		lpNewFilePointer->QuadPart = _ftelli64(pFile->fp);

	return TRUE;
}

static BOOL FileRead(PVOID Object, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
					LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	size_t io_status;
	WINPR_FILE* file;
	BOOL status = TRUE;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (!Object)
		return FALSE;

	file = (WINPR_FILE *)Object;
	clearerr(file->fp);
	io_status = fread(lpBuffer, 1, nNumberOfBytesToRead, file->fp);

	if (io_status == 0 && ferror(file->fp))
	{
		status = FALSE;

		switch (errno)
		{
			case EWOULDBLOCK:
				SetLastError(ERROR_NO_DATA);
				break;
			default:
				SetLastError(map_posix_err(errno));
		}
	}

	if (lpNumberOfBytesRead)
		*lpNumberOfBytesRead = io_status;

	return status;
}

static BOOL FileWrite(PVOID Object, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
						LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	size_t io_status;
	WINPR_FILE* file;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (!Object)
		return FALSE;

	file = (WINPR_FILE *)Object;

	clearerr(file->fp);
	io_status = fwrite(lpBuffer, 1, nNumberOfBytesToWrite, file->fp);
	if (io_status == 0 && ferror(file->fp))
	{
		SetLastError(map_posix_err(errno));
		return FALSE;
	}

	*lpNumberOfBytesWritten = io_status;
	return TRUE;
}

static DWORD FileGetFileSize(HANDLE Object, LPDWORD lpFileSizeHigh)
{
	WINPR_FILE* file;
	INT64 cur, size;

	if (!Object)
		return 0;

	file = (WINPR_FILE *)Object;

	cur = _ftelli64(file->fp);

	if (cur < 0)
	{
		WLog_ERR(TAG, "_ftelli64(%s) failed with %s [0x%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	if (_fseeki64(file->fp, 0, SEEK_END) != 0)
	{
		WLog_ERR(TAG, "_fseeki64(%s) failed with %s [0x%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	size = _ftelli64(file->fp);

	if (size < 0)
	{
		WLog_ERR(TAG, "_ftelli64(%s) failed with %s [0x%08X]", file->lpFileName,
			 strerror(errno), errno);
		return INVALID_FILE_SIZE;
	}

	if (_fseeki64(file->fp, cur, SEEK_SET) != 0)
	{
		WLog_ERR(TAG, "_ftelli64(%s) failed with %s [0x%08X]", file->lpFileName,
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
#ifdef __sun
	struct flock lock;
	int lckcmd;
#else
	int lock;
#endif
	WINPR_FILE* pFile = (WINPR_FILE*)hFile;

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (!hFile)
		return FALSE;

	if (pFile->bLocked)
	{
		WLog_ERR(TAG, "File %s already locked!", pFile->lpFileName);
		return FALSE;
	}

#ifdef __sun
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;

	if (dwFlags & LOCKFILE_EXCLUSIVE_LOCK)
		lock.l_type = F_WRLCK;
	else
		lock.l_type = F_WRLCK;

	if (dwFlags & LOCKFILE_FAIL_IMMEDIATELY)
		lckcmd = F_SETLK;
	else
		lckcmd = F_SETLKW;

	if(fcntl(fileno(pFile->fp), lckcmd, &lock) == -1) {
		WLog_ERR(TAG, "F_SETLK failed with %s [0x%08X]",
			 strerror(errno), errno);
		return FALSE;
	}
#else
	if (dwFlags & LOCKFILE_EXCLUSIVE_LOCK)
		lock = LOCK_EX;
	else
		lock = LOCK_SH;

	if (dwFlags & LOCKFILE_FAIL_IMMEDIATELY)
		lock |= LOCK_NB;

	if (flock(fileno(pFile->fp), lock) < 0)
	{
		WLog_ERR(TAG, "flock failed with %s [0x%08X]",
			 strerror(errno), errno);
		return FALSE;
	}
#endif

	pFile->bLocked = TRUE;

	return TRUE;
}

static BOOL FileUnlockFile(HANDLE hFile, DWORD dwFileOffsetLow, DWORD dwFileOffsetHigh,
				DWORD nNumberOfBytesToUnlockLow, DWORD nNumberOfBytesToUnlockHigh)
{
	WINPR_FILE* pFile = (WINPR_FILE*)hFile;
#ifdef __sun
	struct flock lock;
#endif

	if (!hFile)
		return FALSE;

	if (!pFile->bLocked)
	{
		WLog_ERR(TAG, "File %s is not locked!", pFile->lpFileName);
		return FALSE;
	}

#ifdef __sun
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;
	lock.l_type = F_UNLCK;
	if (fcntl(fileno(pFile->fp), F_GETLK, &lock) == -1)
	{
		WLog_ERR(TAG, "F_UNLCK on %s failed with %s [0x%08X]",
			 pFile->lpFileName, strerror(errno), errno);
		return FALSE;
	}

#else
	if (flock(fileno(pFile->fp), LOCK_UN) < 0)
	{
		WLog_ERR(TAG, "flock(LOCK_UN) %s failed with %s [0x%08X]",
			 pFile->lpFileName, strerror(errno), errno);
		return FALSE;
	}
#endif

	return TRUE;
}

static BOOL FileUnlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToUnlockLow,
				  DWORD nNumberOfBytesToUnlockHigh, LPOVERLAPPED lpOverlapped)
{
	WINPR_FILE* pFile = (WINPR_FILE*)hFile;
#ifdef __sun
	struct flock lock;
#endif

	if (lpOverlapped)
	{
		WLog_ERR(TAG, "WinPR %s does not support the lpOverlapped parameter", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (!hFile)
		return FALSE;

	if (!pFile->bLocked)
	{
		WLog_ERR(TAG, "File %s is not locked!", pFile->lpFileName);
		return FALSE;
	}

#ifdef __sun
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;
	lock.l_type = F_UNLCK;
	if (fcntl(fileno(pFile->fp), F_GETLK, &lock) == -1)
	{
		WLog_ERR(TAG, "F_UNLCK on %s failed with %s [0x%08X]",
			 pFile->lpFileName, strerror(errno), errno);
		return FALSE;
	}
#else
	if (flock(fileno(pFile->fp), LOCK_UN) < 0)
	{
		WLog_ERR(TAG, "flock(LOCK_UN) %s failed with %s [0x%08X]",
			 pFile->lpFileName, strerror(errno), errno);
		return FALSE;
	}
#endif

	return TRUE;
}

static UINT64 FileTimeToUS(const FILETIME* ft)
{
	const UINT64 EPOCH_DIFF = 11644473600ULL * 1000000ULL;
	UINT64 tmp = ((UINT64)ft->dwHighDateTime) << 32
	             | ft->dwLowDateTime;
	tmp /= 10; /* 100ns steps to 1us step */
	tmp -= EPOCH_DIFF;
	return tmp;
}

static BOOL FileSetFileTime(HANDLE hFile, const FILETIME* lpCreationTime,
                            const FILETIME* lpLastAccessTime, const FILETIME* lpLastWriteTime)
{
	int rc;
#if defined(__APPLE__) || defined(ANDROID) || defined(__FreeBSD__) || defined(KFREEBSD)
	struct stat buf;
	/* OpenBSD, NetBSD and DragonflyBSD support POSIX futimens */
	struct timeval timevals[2];
#else
	struct timespec times[2]; /* last access, last modification */
#endif
	WINPR_FILE* pFile = (WINPR_FILE*)hFile;

	if (!hFile)
		return FALSE;

#if defined(__APPLE__) || defined(ANDROID) || defined(__FreeBSD__) || defined(KFREEBSD)
	rc = fstat(fileno(pFile->fp), &buf);

	if (rc < 0)
		return FALSE;

#endif

	if (!lpLastAccessTime)
	{
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(KFREEBSD)
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
		UINT64 tmp = FileTimeToUS(lpLastAccessTime);
#if defined(ANDROID) || defined(__FreeBSD__) || defined(__APPLE__) || defined(KFREEBSD)
		timevals[0].tv_sec = tmp / 1000000ULL;
		timevals[0].tv_usec = tmp % 1000000ULL;
#else
		times[0].tv_sec = tmp / 1000000ULL;
		times[0].tv_nsec = (tmp % 1000000ULL) * 1000ULL;
#endif
	}

	if (!lpLastWriteTime)
	{
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(KFREEBSD)
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
		UINT64 tmp = FileTimeToUS(lpLastWriteTime);
#if defined(ANDROID) || defined(__FreeBSD__) || defined(__APPLE__) || defined(KFREEBSD)
		timevals[1].tv_sec = tmp / 1000000ULL;
		timevals[1].tv_usec = tmp % 1000000ULL;
#else
		times[1].tv_sec = tmp / 1000000ULL;
		times[1].tv_nsec = (tmp % 1000000ULL) * 1000ULL;
#endif
	}

	// TODO: Creation time can not be handled!
#if defined(ANDROID) || defined(__FreeBSD__) || defined(__APPLE__) || defined(KFREEBSD)
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
	FileSetFilePointerEx,
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
	BOOL writeable = (dwDesiredAccess & (GENERIC_WRITE |  FILE_WRITE_DATA | FILE_APPEND_DATA)) != 0;

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
		return (writeable) ? "rb+" : "rb";
	case TRUNCATE_EXISTING:
		*create = FALSE;
		return "wb+";
	default:
		*create = FALSE;
		return "";
	}
}

UINT32 map_posix_err(int fs_errno)
{
	UINT32 rc;

	/* try to return NTSTATUS version of error code */

	switch (fs_errno)
	{
		case 0:
			rc = STATUS_SUCCESS;
			break;

		case ENOTCONN:
		case ENODEV:
		case ENOTDIR:
		case ENXIO:
			rc = ERROR_FILE_NOT_FOUND;
			break;

		case EROFS:
		case EPERM:
		case EACCES:
			rc = ERROR_ACCESS_DENIED;
			break;

		case ENOENT:
			rc = ERROR_FILE_NOT_FOUND;
			break;

		case EBUSY:
			rc = ERROR_BUSY_DRIVE;
			break;

		case EEXIST:
			rc  = ERROR_FILE_EXISTS;
			break;

		case EISDIR:
			rc = STATUS_FILE_IS_A_DIRECTORY;
			break;

		case ENOTEMPTY:
			rc = STATUS_DIRECTORY_NOT_EMPTY;
			break;

		default:
			WLog_ERR(TAG, "Missing ERRNO mapping %s [%d]",
			         strerror(fs_errno), fs_errno);
			rc = STATUS_UNSUCCESSFUL;
			break;
	}
	
	return rc;
}

static HANDLE FileCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
				  DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	WINPR_FILE* pFile;
	BOOL create;
	const char* mode = FileGetMode(dwDesiredAccess, dwCreationDisposition, &create);
#ifdef __sun
	struct flock lock;
#else
	int lock = 0;
#endif
	FILE* fp = NULL;
	struct stat st;

	if (dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED)
	{
		WLog_ERR(TAG, "WinPR %s does not support the FILE_FLAG_OVERLAPPED flag", __FUNCTION__);
		SetLastError(ERROR_NOT_SUPPORTED);
		return INVALID_HANDLE_VALUE;
	}

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
		if (dwCreationDisposition == CREATE_NEW)
		{
			if (stat(pFile->lpFileName, &st) == 0)
			{
				SetLastError(ERROR_FILE_EXISTS);
				free(pFile->lpFileName);
				free(pFile);
				return INVALID_HANDLE_VALUE;
			}
		}

		fp = fopen(pFile->lpFileName, "ab");
		if (!fp)
		{
			SetLastError(map_posix_err(errno));
			free(pFile->lpFileName);
			free(pFile);
			return INVALID_HANDLE_VALUE;
		}

		fp = freopen(pFile->lpFileName, mode, fp);
	}
	else
	{
		if (stat(pFile->lpFileName, &st) != 0)
		{
			SetLastError(map_posix_err(errno));
			free(pFile->lpFileName);
			free(pFile);
			return INVALID_HANDLE_VALUE;
		}

		/* FIFO (named pipe) would block the following fopen
		 * call if not connected. This renders the channel unusable,
		 * therefore abort early. */
		if (S_ISFIFO(st.st_mode))
		{
			SetLastError(ERROR_FILE_NOT_FOUND);
			free(pFile->lpFileName);
			free(pFile);
			return INVALID_HANDLE_VALUE;
		}
	}

	if (NULL == fp)
		fp = fopen(pFile->lpFileName, mode);

	pFile->fp = fp;
	if (!pFile->fp)
	{
		/* This case can occur when trying to open a
		 * not existing file without create flag. */
		SetLastError(map_posix_err(errno));
		free(pFile->lpFileName);
		free(pFile);
		return INVALID_HANDLE_VALUE;
	}

	setvbuf(fp, NULL, _IONBF, 0);

#ifdef __sun
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;

	if (dwShareMode & FILE_SHARE_READ)
		lock.l_type = F_RDLCK;
	if (dwShareMode & FILE_SHARE_WRITE)
		lock.l_type = F_RDLCK;
#else
	if (dwShareMode & FILE_SHARE_READ)
		lock = LOCK_SH;
	if (dwShareMode & FILE_SHARE_WRITE)
		lock = LOCK_EX;
#endif

	if (dwShareMode & (FILE_SHARE_READ | FILE_SHARE_WRITE))
	{
#ifdef __sun
		if (fcntl(fileno(pFile->fp), F_SETLKW, &lock) == -1)
#else
		if (flock(fileno(pFile->fp), lock) < 0)
#endif
		{
#ifdef __sun
			WLog_ERR(TAG, "F_SETLKW failed with %s [0x%08X]",
#else
			WLog_ERR(TAG, "flock failed with %s [0x%08X]",
#endif
				 strerror(errno), errno);
			SetLastError(map_posix_err(errno));
			FileCloseHandle(pFile);
			return INVALID_HANDLE_VALUE;
		}

		pFile->bLocked = TRUE;
	}

	if (fstat(fileno(pFile->fp), &st)==0 && dwFlagsAndAttributes & FILE_ATTRIBUTE_READONLY)
	{
			st.st_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
			fchmod(fileno(pFile->fp), st.st_mode);
	}

	SetLastError(STATUS_SUCCESS);
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

BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
											 LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
#if defined(ANDROID)
#define STATVFS statfs
#else
#define STATVFS statvfs
#endif

	struct STATVFS svfst;
	STATVFS(lpRootPathName, &svfst);
	*lpSectorsPerCluster = svfst.f_frsize;
	*lpBytesPerSector = 1;
	*lpNumberOfFreeClusters = svfst.f_bavail;
	*lpTotalNumberOfClusters = svfst.f_blocks;
	return TRUE;
}

BOOL GetDiskFreeSpaceW(LPCWSTR lpwRootPathName, LPDWORD lpSectorsPerCluster,
											 LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
	LPSTR lpRootPathName;
	BOOL ret;

	if (ConvertFromUnicode(CP_UTF8, 0, lpwRootPathName, -1, &lpRootPathName, 0, NULL, NULL) <= 0)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}
	ret = GetDiskFreeSpaceA(lpRootPathName, lpSectorsPerCluster, lpBytesPerSector,
													 lpNumberOfFreeClusters, lpTotalNumberOfClusters);
	free(lpRootPathName);
	return ret;
}

/**
 * Check if a file name component is valid.
 *
 * Some file names are not valid on Windows. See "Naming Files, Paths, and Namespaces":
 * https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
 */
BOOL ValidFileNameComponent(LPCWSTR lpFileName)
{
	LPCWSTR c = NULL;

	if (!lpFileName)
		return FALSE;

	/* CON */
	if ((lpFileName[0] != L'\0' && (lpFileName[0] == L'C' || lpFileName[0] == L'c')) &&
	    (lpFileName[1] != L'\0' && (lpFileName[1] == L'O' || lpFileName[1] == L'o')) &&
	    (lpFileName[2] != L'\0' && (lpFileName[2] == L'N' || lpFileName[2] == L'n')) &&
	    (lpFileName[3] == L'\0'))
	{
		return FALSE;
	}

	/* PRN */
	if ((lpFileName[0] != L'\0' && (lpFileName[0] == L'P' || lpFileName[0] == L'p')) &&
	    (lpFileName[1] != L'\0' && (lpFileName[1] == L'R' || lpFileName[1] == L'r')) &&
	    (lpFileName[2] != L'\0' && (lpFileName[2] == L'N' || lpFileName[2] == L'n')) &&
	    (lpFileName[3] == L'\0'))
	{
		return FALSE;
	}

	/* AUX */
	if ((lpFileName[0] != L'\0' && (lpFileName[0] == L'A' || lpFileName[0] == L'a')) &&
	    (lpFileName[1] != L'\0' && (lpFileName[1] == L'U' || lpFileName[1] == L'u')) &&
	    (lpFileName[2] != L'\0' && (lpFileName[2] == L'X' || lpFileName[2] == L'x')) &&
	    (lpFileName[3] == L'\0'))
	{
		return FALSE;
	}

	/* NUL */
	if ((lpFileName[0] != L'\0' && (lpFileName[0] == L'N' || lpFileName[0] == L'n')) &&
	    (lpFileName[1] != L'\0' && (lpFileName[1] == L'U' || lpFileName[1] == L'u')) &&
	    (lpFileName[2] != L'\0' && (lpFileName[2] == L'L' || lpFileName[2] == L'l')) &&
	    (lpFileName[3] == L'\0'))
	{
		return FALSE;
	}

	/* LPT0-9 */
	if ((lpFileName[0] != L'\0' && (lpFileName[0] == L'L' || lpFileName[0] == L'l')) &&
	    (lpFileName[1] != L'\0' && (lpFileName[1] == L'P' || lpFileName[1] == L'p')) &&
	    (lpFileName[2] != L'\0' && (lpFileName[2] == L'T' || lpFileName[2] == L't')) &&
	    (lpFileName[3] != L'\0' && (L'0' <= lpFileName[3] && lpFileName[3] <= L'9')) &&
	    (lpFileName[4] == L'\0'))
	{
		return FALSE;
	}

	/* COM0-9 */
	if ((lpFileName[0] != L'\0' && (lpFileName[0] == L'C' || lpFileName[0] == L'c')) &&
	    (lpFileName[1] != L'\0' && (lpFileName[1] == L'O' || lpFileName[1] == L'o')) &&
	    (lpFileName[2] != L'\0' && (lpFileName[2] == L'M' || lpFileName[2] == L'm')) &&
	    (lpFileName[3] != L'\0' && (L'0' <= lpFileName[3] && lpFileName[3] <= L'9')) &&
	    (lpFileName[4] == L'\0'))
	{
		return FALSE;
	}

	/* Reserved characters */
	for (c = lpFileName; *c; c++)
	{
		if ((*c == L'<') || (*c == L'>') || (*c == L':') ||
		    (*c == L'"') || (*c == L'/') || (*c == L'\\') ||
		    (*c == L'|') || (*c == L'?') || (*c == L'*'))
		{
			return FALSE;
		}
	}

	return TRUE;
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


