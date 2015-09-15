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

#include "../handle/handle.h"
#include <errno.h>
#include <fcntl.h>

struct winpr_file
{
	WINPR_HANDLE_DEF();

	int fd;
};

typedef struct winpr_file WINPR_FILE;

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

	free(handle);
	return TRUE;
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


static HANDLE_OPS ops = {
		FileIsHandled,
		FileCloseHandle,
		FileGetFd,
		NULL, /* CleanupHandle */
		FileRead,
		FileWrite
};

static WINPR_FILE *FileHandle_New()
{
	WINPR_FILE *pFile;
	HANDLE hFile;

	pFile = (WINPR_FILE*) calloc(1, sizeof(WINPR_FILE));
	if (!pFile)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}
	pFile->fd = -1;
	pFile->ops = &ops;

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
	pFile = FileHandle_New();
	if (!pFile)
		return INVALID_HANDLE_VALUE;

	pFile->fd = fd;
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

	pFile = FileHandle_New();
	if (!pFile)
		return INVALID_HANDLE_VALUE;
	pFile->fd = fd;
	return (HANDLE)pFile;
#endif /* WIN32 */
}

