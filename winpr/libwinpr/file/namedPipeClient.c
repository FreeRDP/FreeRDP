/**
 * WinPR: Windows Portable Runtime
 * File Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Hewlett-Packard Development Company, L.P.
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 bernhard.miklautz@thincast.com
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

#include "../log.h"
#define TAG WINPR_TAG("file")

#ifndef _WIN32


#ifdef ANDROID
#include <sys/vfs.h>
#else
#include <sys/statvfs.h>
#endif

#include "../handle/handle.h"

#include "../pipe/pipe.h"

static HANDLE_CREATOR _NamedPipeClientHandleCreator;

static BOOL NamedPipeClientIsHandled(HANDLE handle)
{
	WINPR_NAMED_PIPE* pFile = (WINPR_NAMED_PIPE*) handle;

	if (!pFile || (pFile->Type != HANDLE_TYPE_NAMED_PIPE) || (pFile == INVALID_HANDLE_VALUE))
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	return TRUE;
}

BOOL NamedPipeClientCloseHandle(HANDLE handle)
{
	WINPR_NAMED_PIPE* pNamedPipe = (WINPR_NAMED_PIPE*) handle;


	if (!NamedPipeClientIsHandled(handle))
		return FALSE;

	if (pNamedPipe->clientfd != -1)
	{
		//WLOG_DBG(TAG, "closing clientfd %d", pNamedPipe->clientfd);
		close(pNamedPipe->clientfd);
	}

	if (pNamedPipe->serverfd != -1)
	{
		//WLOG_DBG(TAG, "closing serverfd %d", pNamedPipe->serverfd);
		close(pNamedPipe->serverfd);
	}

	if (pNamedPipe->pfnUnrefNamedPipe)
		pNamedPipe->pfnUnrefNamedPipe(pNamedPipe);

	free(pNamedPipe->lpFileName);
	free(pNamedPipe->lpFilePath);
	free(pNamedPipe->name);
	free(pNamedPipe);

	return TRUE;
}

static int NamedPipeClientGetFd(HANDLE handle)
{
	WINPR_NAMED_PIPE *file = (WINPR_NAMED_PIPE *)handle;

	if (!NamedPipeClientIsHandled(handle))
		return -1;

	if (file->ServerMode)
		return file->serverfd;
	else
		return file->clientfd;
}

static HANDLE_OPS ops = {
		NamedPipeClientIsHandled,
		NamedPipeClientCloseHandle,
		NamedPipeClientGetFd,
		NULL, /* CleanupHandle */
		NamedPipeRead,
		NULL, /* FileReadEx */
		NULL, /* FileReadScatter */
		NamedPipeWrite,
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
		NULL  /* SetFileTime */
};

static HANDLE NamedPipeClientCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
				   DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	char* name;
	int status;
	HANDLE hNamedPipe;
	struct sockaddr_un s;
	WINPR_NAMED_PIPE* pNamedPipe;

	if (!lpFileName)
		return INVALID_HANDLE_VALUE;

	if (!IsNamedPipeFileNameA(lpFileName))
		return INVALID_HANDLE_VALUE;

	name = GetNamedPipeNameWithoutPrefixA(lpFileName);

	if (!name)
		return INVALID_HANDLE_VALUE;

	free(name);
	pNamedPipe = (WINPR_NAMED_PIPE*) calloc(1, sizeof(WINPR_NAMED_PIPE));
	if (!pNamedPipe)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return INVALID_HANDLE_VALUE;
	}

	hNamedPipe = (HANDLE) pNamedPipe;
	WINPR_HANDLE_SET_TYPE_AND_MODE(pNamedPipe, HANDLE_TYPE_NAMED_PIPE, WINPR_FD_READ);
	pNamedPipe->name = _strdup(lpFileName);
	if (!pNamedPipe->name)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		free(pNamedPipe);
		return INVALID_HANDLE_VALUE;
	}
	pNamedPipe->dwOpenMode = 0;
	pNamedPipe->dwPipeMode = 0;
	pNamedPipe->nMaxInstances = 0;
	pNamedPipe->nOutBufferSize = 0;
	pNamedPipe->nInBufferSize = 0;
	pNamedPipe->nDefaultTimeOut = 0;
	pNamedPipe->dwFlagsAndAttributes = dwFlagsAndAttributes;
	pNamedPipe->lpFileName = GetNamedPipeNameWithoutPrefixA(lpFileName);
	if (!pNamedPipe->lpFileName)
	{
		free((void *)pNamedPipe->name);
		free(pNamedPipe);
		return INVALID_HANDLE_VALUE;

	}
	pNamedPipe->lpFilePath = GetNamedPipeUnixDomainSocketFilePathA(lpFileName);
	if (!pNamedPipe->lpFilePath)
	{
		free((void *)pNamedPipe->lpFileName);
		free((void *)pNamedPipe->name);
		free(pNamedPipe);
		return INVALID_HANDLE_VALUE;

	}
	pNamedPipe->clientfd = socket(PF_LOCAL, SOCK_STREAM, 0);
	pNamedPipe->serverfd = -1;
	pNamedPipe->ServerMode = FALSE;
	ZeroMemory(&s, sizeof(struct sockaddr_un));
	s.sun_family = AF_UNIX;
	strcpy(s.sun_path, pNamedPipe->lpFilePath);
	status = connect(pNamedPipe->clientfd, (struct sockaddr*) &s, sizeof(struct sockaddr_un));

	pNamedPipe->ops = &ops;

	if (status != 0)
	{
		close(pNamedPipe->clientfd);
		free((char*) pNamedPipe->name);
		free((char*) pNamedPipe->lpFileName);
		free((char*) pNamedPipe->lpFilePath);
		free(pNamedPipe);
		return INVALID_HANDLE_VALUE;
	}

	if (dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED)
	{
#if 0
		int flags = fcntl(pNamedPipe->clientfd, F_GETFL);

		if (flags != -1)
			fcntl(pNamedPipe->clientfd, F_SETFL, flags | O_NONBLOCK);

#endif
	}

	return hNamedPipe;
}

HANDLE_CREATOR *GetNamedPipeClientHandleCreator(void)
{
	_NamedPipeClientHandleCreator.IsHandled = IsNamedPipeFileNameA;
	_NamedPipeClientHandleCreator.CreateFileA = NamedPipeClientCreateFileA;
	return &_NamedPipeClientHandleCreator;
}

#endif

/* Extended API */

#define NAMED_PIPE_PREFIX_PATH		"\\\\.\\pipe\\"

BOOL IsNamedPipeFileNameA(LPCSTR lpName)
{
	if (strncmp(lpName, NAMED_PIPE_PREFIX_PATH, sizeof(NAMED_PIPE_PREFIX_PATH) - 1) != 0)
		return FALSE;

	return TRUE;
}

char* GetNamedPipeNameWithoutPrefixA(LPCSTR lpName)
{
	char* lpFileName;

	if (!lpName)
		return NULL;

	if (!IsNamedPipeFileNameA(lpName))
		return NULL;

	lpFileName = _strdup(&lpName[strlen(NAMED_PIPE_PREFIX_PATH)]);
	return lpFileName;
}

char* GetNamedPipeUnixDomainSocketBaseFilePathA()
{
	char* lpTempPath;
	char* lpPipePath;
	lpTempPath = GetKnownPath(KNOWN_PATH_TEMP);
	if (!lpTempPath)
		return NULL;
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

int GetNamePipeFileDescriptor(HANDLE hNamedPipe)
{
#ifndef _WIN32
	int fd;
	WINPR_NAMED_PIPE* pNamedPipe;
	pNamedPipe = (WINPR_NAMED_PIPE*) hNamedPipe;

	if (!pNamedPipe || pNamedPipe->Type != HANDLE_TYPE_NAMED_PIPE)
		return -1;

	fd = (pNamedPipe->ServerMode) ? pNamedPipe->serverfd : pNamedPipe->clientfd;
	return fd;
#else
	return -1;
#endif
}

