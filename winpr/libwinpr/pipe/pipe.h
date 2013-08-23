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

#ifndef WINPR_PIPE_PRIVATE_H
#define WINPR_PIPE_PRIVATE_H

#ifndef _WIN32

#include <winpr/pipe.h>

#include "../handle/handle.h"

struct winpr_pipe
{
	WINPR_HANDLE_DEF();

	int fd;
};
typedef struct winpr_pipe WINPR_PIPE;

struct winpr_named_pipe
{
	WINPR_HANDLE_DEF();

	int clientfd;
	int serverfd;

	const char* name;
	const char* lpFileName;
	const char* lpFilePath;

	DWORD dwOpenMode;
	DWORD dwPipeMode;
	DWORD nMaxInstances;
	DWORD nOutBufferSize;
	DWORD nInBufferSize;
	DWORD nDefaultTimeOut;
	DWORD dwFlagsAndAttributes;
	LPOVERLAPPED lpOverlapped;
};
typedef struct winpr_named_pipe WINPR_NAMED_PIPE;

#endif

#endif /* WINPR_PIPE_PRIVATE_H */


