/**
 * WinPR: Windows Portable Runtime
 * Pipe Functions
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

#include <winpr/pipe.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef _WIN32

#include "../handle/handle.h"

#include "pipe.h"

BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize)
{
	int pipe_fd[2];
	WINPR_PIPE* pReadPipe;
	WINPR_PIPE* pWritePipe;

	pipe_fd[0] = -1;
	pipe_fd[1] = -1;

	if (pipe(pipe_fd) < 0)
	{
		printf("CreatePipe: failed to create pipe\n");
		return FALSE;
	}

	pReadPipe = (WINPR_PIPE*) malloc(sizeof(WINPR_PIPE));
	pWritePipe = (WINPR_PIPE*) malloc(sizeof(WINPR_PIPE));

	if (!pReadPipe || !pWritePipe)
		return FALSE;

	pReadPipe->fd = pipe_fd[0];
	pWritePipe->fd = pipe_fd[1];

	WINPR_HANDLE_SET_TYPE(pReadPipe, HANDLE_TYPE_ANONYMOUS_PIPE);
	*((ULONG_PTR*) hReadPipe) = (ULONG_PTR) pReadPipe;

	WINPR_HANDLE_SET_TYPE(pWritePipe, HANDLE_TYPE_ANONYMOUS_PIPE);
	*((ULONG_PTR*) hWritePipe) = (ULONG_PTR) pWritePipe;

	return TRUE;
}

#endif
