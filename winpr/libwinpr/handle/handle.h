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

#ifndef WINPR_HANDLE_PRIVATE_H
#define WINPR_HANDLE_PRIVATE_H

#include <pthread.h>

#include <winpr/handle.h>

#define HANDLE_TYPE_NONE			0
#define HANDLE_TYPE_THREAD			1
#define HANDLE_TYPE_EVENT			2
#define HANDLE_TYPE_MUTEX			3
#define HANDLE_TYPE_SEMAPHORE			4
#define HANDLE_TYPE_TIMER			5
#define HANDLE_TYPE_NAMED_PIPE			6
#define HANDLE_TYPE_ANONYMOUS_PIPE		7

typedef struct _HANDLE_TABLE_ENTRY
{
	ULONG Type;
	PVOID Object;
} HANDLE_TABLE_ENTRY, *PHANDLE_TABLE_ENTRY;

typedef struct _HANDLE_TABLE
{
	LONG Count;
	LONG MaxCount;
	PHANDLE_TABLE_ENTRY Entries;
} HANDLE_TABLE, *PHANDLE_TABLE;

#define WINPR_HANDLE_DEF() \
	int winpr_Index;

struct winpr_handle
{
	WINPR_HANDLE_DEF();
};
typedef struct winpr_handle WINPR_HANDLE;

#define HandleTable_GetInstance() \
	if (g_WinPR_HandleTable.MaxCount < 1) \
		winpr_HandleTable_New()

extern HANDLE_TABLE g_WinPR_HandleTable;
extern pthread_mutex_t g_WinPR_HandleTable_Mutex;

WINPR_API HANDLE winpr_Handle_Insert(ULONG Type, PVOID Object);
WINPR_API BOOL winpr_Handle_Remove(HANDLE handle);

WINPR_API ULONG winpr_Handle_GetType(HANDLE handle);
WINPR_API PVOID winpr_Handle_GetObject(HANDLE handle);
WINPR_API BOOL winpr_Handle_GetInfo(HANDLE handle, ULONG* pType, PVOID* pObject);

#endif /* WINPR_HANDLE_PRIVATE_H */
