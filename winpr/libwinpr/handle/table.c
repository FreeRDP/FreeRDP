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

#include "../handle/handle.h"

HANDLE_TABLE g_WinPR_HandleTable = { 0, 0, NULL };
pthread_mutex_t g_WinPR_HandleTable_Mutex = PTHREAD_MUTEX_INITIALIZER;

void winpr_HandleTable_New()
{
	size_t size;

	pthread_mutex_lock(&g_WinPR_HandleTable_Mutex);

	if (g_WinPR_HandleTable.MaxCount < 1)
	{
		g_WinPR_HandleTable.Count = 0;
		g_WinPR_HandleTable.MaxCount = 64;

		size = sizeof(HANDLE_TABLE_ENTRY) * g_WinPR_HandleTable.MaxCount;

		g_WinPR_HandleTable.Entries = (PHANDLE_TABLE_ENTRY) malloc(size);
		ZeroMemory(g_WinPR_HandleTable.Entries, size);
	}

	pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);
}

void winpr_HandleTable_Grow()
{
	size_t size;

	pthread_mutex_lock(&g_WinPR_HandleTable_Mutex);

	g_WinPR_HandleTable.MaxCount *= 2;

	size = sizeof(HANDLE_TABLE_ENTRY) * g_WinPR_HandleTable.MaxCount;

	g_WinPR_HandleTable.Entries = (PHANDLE_TABLE_ENTRY) realloc(g_WinPR_HandleTable.Entries, size);
	ZeroMemory((void*) &g_WinPR_HandleTable.Entries[g_WinPR_HandleTable.MaxCount / 2], size / 2);

	pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);
}

void winpr_HandleTable_Free()
{
	pthread_mutex_lock(&g_WinPR_HandleTable_Mutex);

	g_WinPR_HandleTable.Count = 0;
	g_WinPR_HandleTable.MaxCount = 0;

	free(g_WinPR_HandleTable.Entries);
	g_WinPR_HandleTable.Entries = NULL;

	pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);
}

HANDLE winpr_Handle_Insert(ULONG Type, PVOID Object)
{
	int index;

	HandleTable_GetInstance();

	pthread_mutex_lock(&g_WinPR_HandleTable_Mutex);

	for (index = 0; index < (int) g_WinPR_HandleTable.MaxCount; index++)
	{
		if (g_WinPR_HandleTable.Entries[index].Object == NULL)
		{
			g_WinPR_HandleTable.Count++;

			g_WinPR_HandleTable.Entries[index].Type = Type;
			g_WinPR_HandleTable.Entries[index].Object = Object;

			pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);

			return Object;
		}
	}

	pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);

	/* no available entry was found, the table needs to be grown */

	winpr_HandleTable_Grow();

	/* the next call should now succeed */

	return winpr_Handle_Insert(Type, Object);
}

BOOL winpr_Handle_Remove(HANDLE handle)
{
	int index;

	HandleTable_GetInstance();

	pthread_mutex_lock(&g_WinPR_HandleTable_Mutex);

	for (index = 0; index < (int) g_WinPR_HandleTable.MaxCount; index++)
	{
		if (g_WinPR_HandleTable.Entries[index].Object == handle)
		{
			g_WinPR_HandleTable.Entries[index].Type = HANDLE_TYPE_NONE;
			g_WinPR_HandleTable.Entries[index].Object = NULL;
			g_WinPR_HandleTable.Count--;

			pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);

			return TRUE;
		}
	}

	pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);

	return FALSE;
}

ULONG winpr_Handle_GetType(HANDLE handle)
{
	int index;

	HandleTable_GetInstance();

	pthread_mutex_lock(&g_WinPR_HandleTable_Mutex);

	for (index = 0; index < (int) g_WinPR_HandleTable.MaxCount; index++)
	{
		if (g_WinPR_HandleTable.Entries[index].Object == handle)
		{
			pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);
			return g_WinPR_HandleTable.Entries[index].Type;
		}
	}

	pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);

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

	pthread_mutex_lock(&g_WinPR_HandleTable_Mutex);

	for (index = 0; index < (int) g_WinPR_HandleTable.MaxCount; index++)
	{
		if (g_WinPR_HandleTable.Entries[index].Object == handle)
		{
			*pType = g_WinPR_HandleTable.Entries[index].Type;
			*pObject = g_WinPR_HandleTable.Entries[index].Object;

			pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);

			return TRUE;
		}
	}

	pthread_mutex_unlock(&g_WinPR_HandleTable_Mutex);

	return FALSE;
}

#endif

