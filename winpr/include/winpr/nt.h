/**
 * WinPR: Windows Portable Runtime
 * Windows Native System Services
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_NT_H
#define WINPR_NT_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

typedef struct _PEB PEB;
typedef struct _PEB* PPEB;

typedef struct _TEB TEB;
typedef struct _TEB* PTEB;

/**
 * Process Environment Block
 */

struct _THREAD_BLOCK_ID
{
	DWORD ThreadId;
	TEB* ThreadEnvironmentBlock;
};
typedef struct _THREAD_BLOCK_ID THREAD_BLOCK_ID;

struct _PEB
{
	DWORD ThreadCount;
	DWORD ThreadArraySize;
	THREAD_BLOCK_ID* Threads;
};

/*
 * Thread Environment Block
 */

struct _TEB
{
	PEB* ProcessEnvironmentBlock;

	DWORD LastErrorValue;
	PVOID TlsSlots[64];
};

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API PTEB NtCurrentTeb(void);

#ifdef __cplusplus
}
#endif

#endif

#endif /* WINPR_NT_H */

