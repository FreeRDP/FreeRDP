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

#include <errno.h>
#include "../handle/handle.h"
#include "../log.h"
#define TAG WINPR_TAG("synch.semaphore")

static BOOL SemaphoreCloseHandle(HANDLE handle);

static BOOL SemaphoreIsHandled(HANDLE handle)
{
	WINPR_TIMER* pSemaphore = (WINPR_TIMER*) handle;

	if (!pSemaphore || (pSemaphore->Type != HANDLE_TYPE_SEMAPHORE))
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

static int SemaphoreGetFd(HANDLE handle)
{
	WINPR_SEMAPHORE *sem = (WINPR_SEMAPHORE *)handle;

	if (!SemaphoreIsHandled(handle))
		return -1;

	return sem->pipe_fd[0];
}

static DWORD SemaphoreCleanupHandle(HANDLE handle)
{
	int length;
	WINPR_SEMAPHORE *sem = (WINPR_SEMAPHORE *)handle;

	if (!SemaphoreIsHandled(handle))
		return WAIT_FAILED;

	length = read(sem->pipe_fd[0], &length, 1);

	if (length != 1)
	{
		WLog_ERR(TAG, "semaphore read() failure [%d] %s", errno, strerror(errno));
		return WAIT_FAILED;
	}

	return WAIT_OBJECT_0;
}

BOOL SemaphoreCloseHandle(HANDLE handle) {
	WINPR_SEMAPHORE *semaphore = (WINPR_SEMAPHORE *) handle;

	if (!SemaphoreIsHandled(handle))
		return FALSE;

#ifdef WINPR_PIPE_SEMAPHORE

	if (semaphore->pipe_fd[0] != -1)
	{
		close(semaphore->pipe_fd[0]);
		semaphore->pipe_fd[0] = -1;

		if (semaphore->pipe_fd[1] != -1)
		{
			close(semaphore->pipe_fd[1]);
			semaphore->pipe_fd[1] = -1;
		}
	}

#else
#if defined __APPLE__
	semaphore_destroy(mach_task_self(), *((winpr_sem_t*) semaphore->sem));
#else
	sem_destroy((winpr_sem_t*) semaphore->sem);
#endif
#endif
	free(semaphore);
	return TRUE;
}

static HANDLE_OPS ops = {
		SemaphoreIsHandled,
		SemaphoreCloseHandle,
		SemaphoreGetFd,
		SemaphoreCleanupHandle
};

HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName)
{
	HANDLE handle;
	WINPR_SEMAPHORE* semaphore;

	semaphore = (WINPR_SEMAPHORE*) calloc(1, sizeof(WINPR_SEMAPHORE));

	if (!semaphore)
		return NULL;

	semaphore->pipe_fd[0] = -1;
	semaphore->pipe_fd[1] = -1;
	semaphore->sem = (winpr_sem_t*) NULL;
	semaphore->ops = &ops;

#ifdef WINPR_PIPE_SEMAPHORE

	if (pipe(semaphore->pipe_fd) < 0)
	{
		WLog_ERR(TAG, "failed to create semaphore");
		free(semaphore);
		return NULL;
	}

	while (lInitialCount > 0)
	{
		if (write(semaphore->pipe_fd[1], "-", 1) != 1)
		{
			close(semaphore->pipe_fd[0]);
			close(semaphore->pipe_fd[1]);
			free(semaphore);
			return NULL;
		}

		lInitialCount--;
	}

#else
	semaphore->sem = (winpr_sem_t*) malloc(sizeof(winpr_sem_t));
	if (!semaphore->sem)
	{
		WLog_ERR(TAG, "failed to allocate semaphore memory");
		free(semaphore);
		return NULL;
	}
#if defined __APPLE__
	if (semaphore_create(mach_task_self(), semaphore->sem, SYNC_POLICY_FIFO, lMaximumCount) != KERN_SUCCESS)
#else
	if (sem_init(semaphore->sem, 0, lMaximumCount) == -1)
#endif
	{
		WLog_ERR(TAG, "failed to create semaphore");
		free(semaphore->sem);
		free(semaphore);
		return NULL;
	}
#endif

	WINPR_HANDLE_SET_TYPE_AND_MODE(semaphore, HANDLE_TYPE_SEMAPHORE, WINPR_FD_READ);
	handle = (HANDLE) semaphore;
	return handle;
}

HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName)
{
	return CreateSemaphoreW(lpSemaphoreAttributes, lInitialCount, lMaximumCount, NULL);
}

HANDLE OpenSemaphoreW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	WLog_ERR(TAG, "not implemented");
	return NULL;
}

HANDLE OpenSemaphoreA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
{
	WLog_ERR(TAG, "not implemented");
	return NULL;
}

BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount)
{
	ULONG Type;
	WINPR_HANDLE* Object;
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

	WLog_ERR(TAG, "calling %s on a handle that is not a semaphore", __FUNCTION__);
	return FALSE;
}

#endif
