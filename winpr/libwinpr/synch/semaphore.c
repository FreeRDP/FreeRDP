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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef _WIN32

#include "../handle/handle.h"

HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName)
{
	HANDLE handle;
	WINPR_SEMAPHORE* semaphore;

	semaphore = (WINPR_SEMAPHORE*) malloc(sizeof(WINPR_SEMAPHORE));

	semaphore->pipe_fd[0] = -1;
	semaphore->pipe_fd[0] = -1;
	semaphore->sem = (winpr_sem_t*) NULL;

	if (semaphore)
	{
#ifdef WINPR_PIPE_SEMAPHORE

		if (pipe(semaphore->pipe_fd) < 0)
		{
			fprintf(stderr, "CreateSemaphoreW: failed to create semaphore\n");
			return NULL;
		}

		while (lInitialCount > 0)
		{
			if (write(semaphore->pipe_fd[1], "-", 1) != 1)
				return FALSE;

			lInitialCount--;
		}

#else
		semaphore->sem = (winpr_sem_t*) malloc(sizeof(winpr_sem_t));
#if defined __APPLE__
		semaphore_create(mach_task_self(), semaphore->sem, SYNC_POLICY_FIFO, lMaximumCount);
#else
		sem_init(semaphore->sem, 0, lMaximumCount);
#endif

#endif
	}

	WINPR_HANDLE_SET_TYPE(semaphore, HANDLE_TYPE_SEMAPHORE);
	handle = (HANDLE) semaphore;

	return handle;
}

HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName)
{
	return CreateSemaphoreW(lpSemaphoreAttributes, lInitialCount, lMaximumCount, NULL);
}

HANDLE OpenSemaphoreW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	return NULL;
}

HANDLE OpenSemaphoreA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
{
	return NULL;
}

BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount)
{
	ULONG Type;
	PVOID Object;
	WINPR_SEMAPHORE* semaphore;

	if (!winpr_Handle_GetInfo(hSemaphore, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_SEMAPHORE)
	{
		semaphore = (WINPR_SEMAPHORE*) Object;

#ifdef WINPR_PIPE_SEMAPHORE

		if (semaphore->pipe_fd[0] != -1)
		{
			while (lReleaseCount > 0)
			{
				if (write(semaphore->pipe_fd[1], "-", 1) != 1)
					return FALSE;

				lReleaseCount--;
			}
		}

#else

#if defined __APPLE__
		semaphore_signal(*((winpr_sem_t*) semaphore->sem));
#else
		sem_post((winpr_sem_t*) semaphore->sem);
#endif

#endif
		return TRUE;
	}

	return FALSE;
}

#endif
