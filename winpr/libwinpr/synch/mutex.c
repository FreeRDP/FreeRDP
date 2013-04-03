/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
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

#include <winpr/synch.h>

#include "synch.h"

/**
 * CreateMutexA
 * CreateMutexW
 * CreateMutexExA
 * CreateMutexExW
 * OpenMutexA
 * OpenMutexW
 * ReleaseMutex
 */

#ifndef _WIN32

HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName)
{
	HANDLE handle = NULL;
	pthread_mutex_t* pMutex;

	pMutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));

	if (pMutex)
	{
		pthread_mutex_init(pMutex, 0);
		handle = winpr_Handle_Insert(HANDLE_TYPE_MUTEX, pMutex);
		if (bInitialOwner)
			pthread_mutex_lock(pMutex);
	}

	return handle;
}

HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName)
{
	return CreateMutexW(lpMutexAttributes, bInitialOwner, NULL);
}

HANDLE CreateMutexExA(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCTSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	return CreateMutexW(lpMutexAttributes, FALSE, NULL);
}

HANDLE CreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	return CreateMutexW(lpMutexAttributes, FALSE, NULL);
}

HANDLE OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle,LPCSTR lpName)
{
	return NULL;
}

HANDLE OpenMutexW(DWORD dwDesiredAccess, BOOL bInheritHandle,LPCWSTR lpName)
{
	return NULL;
}

BOOL ReleaseMutex(HANDLE hMutex)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hMutex, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_MUTEX)
	{
		pthread_mutex_unlock((pthread_mutex_t*) Object);
		return TRUE;
	}

	return FALSE;
}

#endif
