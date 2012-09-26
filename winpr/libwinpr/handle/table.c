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

#include <winpr/crt.h>
#include <winpr/handle.h>

#ifndef _WIN32

#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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

static HANDLE_TABLE HandleTable = { 0, 0, NULL };

#define HandleTable_GetInstance() \
	if (HandleTable.MaxCount < 1) \
		winpr_HandleTable_New()

void winpr_HandleTable_New()
{
	size_t size;

	pthread_mutex_lock(&mutex);

	if (HandleTable.MaxCount < 1)
	{
		HandleTable.Count = 0;
		HandleTable.MaxCount = 64;

		size = sizeof(HANDLE_TABLE_ENTRY) * HandleTable.MaxCount;

		HandleTable.Entries = (PHANDLE_TABLE_ENTRY) malloc(size);
		ZeroMemory(HandleTable.Entries, size);
	}

	pthread_mutex_unlock(&mutex);
}

void winpr_HandleTable_Grow()
{
	size_t size;

	pthread_mutex_lock(&mutex);

	HandleTable.MaxCount *= 2;

	size = sizeof(HANDLE_TABLE_ENTRY) * HandleTable.MaxCount;

	HandleTable.Entries = (PHANDLE_TABLE_ENTRY) realloc(HandleTable.Entries, size);
	ZeroMemory((void*) &HandleTable.Entries[HandleTable.MaxCount / 2], size / 2);

	pthread_mutex_unlock(&mutex);
}

void winpr_HandleTable_Free()
{
	pthread_mutex_lock(&mutex);

	HandleTable.Count = 0;
	HandleTable.MaxCount = 0;

	free(HandleTable.Entries);
	HandleTable.Entries = NULL;

	pthread_mutex_unlock(&mutex);
}

HANDLE winpr_Handle_Insert(ULONG Type, PVOID Object)
{
	int index;

	HandleTable_GetInstance();

	pthread_mutex_lock(&mutex);

	for (index = 0; index < (int) HandleTable.MaxCount; index++)
	{
		if (HandleTable.Entries[index].Object == NULL)
		{
			HandleTable.Count++;

			HandleTable.Entries[index].Type = Type;
			HandleTable.Entries[index].Object = Object;

			pthread_mutex_unlock(&mutex);

			return Object;
		}
	}

	pthread_mutex_unlock(&mutex);

	/* no available entry was found, the table needs to be grown */

	winpr_HandleTable_Grow();

	/* the next call should now succeed */

	return winpr_Handle_Insert(Type, Object);
}

BOOL winpr_Handle_Remove(HANDLE handle)
{
	int index;

	HandleTable_GetInstance();

	pthread_mutex_lock(&mutex);

	for (index = 0; index < (int) HandleTable.MaxCount; index++)
	{
		if (HandleTable.Entries[index].Object == handle)
		{
			HandleTable.Entries[index].Type = HANDLE_TYPE_NONE;
			HandleTable.Entries[index].Object = NULL;
			HandleTable.Count--;

			pthread_mutex_unlock(&mutex);

			return TRUE;
		}
	}

	pthread_mutex_unlock(&mutex);

	return FALSE;
}

ULONG winpr_Handle_GetType(HANDLE handle)
{
	int index;

	HandleTable_GetInstance();

	pthread_mutex_lock(&mutex);

	for (index = 0; index < (int) HandleTable.MaxCount; index++)
	{
		if (HandleTable.Entries[index].Object == handle)
		{
			pthread_mutex_unlock(&mutex);
			return HandleTable.Entries[index].Type;
		}
	}

	pthread_mutex_unlock(&mutex);

	return HANDLE_TYPE_NONE;
}

PVOID winpr_Handle_GetObject(HANDLE handle)
{
	HandleTable_GetInstance();

	return handle;
}

BOOL winpr_Handle_GetInfo(HANDLE handle, ULONG* pType, PVOID* pObject)
{
	int index;

	HandleTable_GetInstance();

	pthread_mutex_lock(&mutex);

	for (index = 0; index < (int) HandleTable.MaxCount; index++)
	{
		if (HandleTable.Entries[index].Object == handle)
		{
			*pType = HandleTable.Entries[index].Type;
			*pObject = HandleTable.Entries[index].Object;

			pthread_mutex_unlock(&mutex);

			return TRUE;
		}
	}

	pthread_mutex_unlock(&mutex);

	return FALSE;
}

#endif

