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
 * CreateSemaphoreExA
 * CreateSemaphoreExW
 * OpenSemaphoreA
 * OpenSemaphoreW
 * ReleaseSemaphore
 */

#ifndef _WIN32

HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName)
{
	winpr_sem_t* hSemaphore;

	hSemaphore = malloc(sizeof(winpr_sem_t));

#if defined __APPLE__
	semaphore_create(mach_task_self(), hSemaphore, SYNC_POLICY_FIFO, lMaximumCount);
#else
	sem_init(hSemaphore, 0, lMaximumCount);
#endif

	return (HANDLE) hSemaphore;
}

HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName)
{
	winpr_sem_t* hSemaphore;

	hSemaphore = malloc(sizeof(winpr_sem_t));

#if defined __APPLE__
	semaphore_create(mach_task_self(), hSemaphore, SYNC_POLICY_FIFO, lMaximumCount);
#else
	sem_init(hSemaphore, 0, lMaximumCount);
#endif

	return (HANDLE) hSemaphore;
}

HANDLE OpenSemaphoreA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
{
	return NULL;
}

HANDLE OpenSemaphoreW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	return NULL;
}

BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount)
{
#if defined __APPLE__
	semaphore_signal(*((winpr_sem_t*) hSemaphore));
#else
	sem_post((winpr_sem_t*) hSemaphore);
#endif

	return 1;
}

#endif
