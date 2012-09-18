/**
 * WinPR: Windows Portable Runtime
 * Handle Management
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

#include <winpr/handle.h>

#ifndef _WIN32

typedef struct _HANDLE_TABLE_ENTRY
{
	PVOID Object;
	ULONG ObAttributes;
	ULONG Value;
} HANDLE_TABLE_ENTRY, *PHANDLE_TABLE_ENTRY;

typedef struct _HANDLE_TABLE
{
	LONG Count;
	PHANDLE_TABLE_ENTRY* Handles;
} HANDLE_TABLE, *PHANDLE_TABLE;

#if defined __APPLE__
#include <pthread.h>
#include <semaphore.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#define winpr_sem_t semaphore_t
#else
#include <pthread.h>
#include <semaphore.h>
#define winpr_sem_t sem_t
#endif

BOOL CloseHandle(HANDLE hObject)
{
#if defined __APPLE__
	semaphore_destroy(mach_task_self(), *((winpr_sem_t*) hObject));
#else
	sem_destroy((winpr_sem_t*) hObject);
#endif

	free(hObject);

	return 1;
}

BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
	LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions)
{
	return 1;
}

BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags)
{
	return 1;
}

BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags)
{
	return 1;
}

#endif
