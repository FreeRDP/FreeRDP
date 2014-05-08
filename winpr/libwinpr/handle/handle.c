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

#include "../synch/synch.h"
#include "../thread/thread.h"
#include "../pipe/pipe.h"
#include "../security/security.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../handle/handle.h"

BOOL CloseHandle(HANDLE hObject)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hObject, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_THREAD)
	{
		WINPR_THREAD* thread;

		thread = (WINPR_THREAD*) Object;
		free(thread);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_PROCESS)
	{
		WINPR_PROCESS* process;

		process = (WINPR_PROCESS*) Object;
		free(process);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_MUTEX)
	{
		WINPR_MUTEX* mutex;

		mutex = (WINPR_MUTEX*) Object;

		pthread_mutex_destroy(&mutex->mutex);

		free(Object);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_EVENT)
	{
		WINPR_EVENT* event;

		event = (WINPR_EVENT*) Object;

		if (!event->bAttached)
		{
			if (event->pipe_fd[0] != -1)
			{
				close(event->pipe_fd[0]);
				event->pipe_fd[0] = -1;
			}
			if (event->pipe_fd[1] != -1)
			{
				close(event->pipe_fd[1]);
				event->pipe_fd[1] = -1;
			}
		}

		free(Object);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_SEMAPHORE)
	{
		WINPR_SEMAPHORE* semaphore;

		semaphore = (WINPR_SEMAPHORE*) Object;

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
		free(Object);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_TIMER)
	{
		WINPR_TIMER* timer;

		timer = (WINPR_TIMER*) Object;

#ifdef __linux__
		if (timer->fd != -1)
			close(timer->fd);
#endif

		free(Object);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_ANONYMOUS_PIPE)
	{
		WINPR_PIPE* pipe;

		pipe = (WINPR_PIPE*) Object;

		if (pipe->fd != -1)
		{
			close(pipe->fd);
		}

		free(Object);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		WINPR_NAMED_PIPE* pipe;

		pipe = (WINPR_NAMED_PIPE*) Object;

		if (--pipe->dwRefCount == 0)
		{
			pipe->pfnRemoveBaseNamedPipeFromList(pipe);

			if (pipe->pBaseNamedPipe)
			{
				CloseHandle((HANDLE) pipe->pBaseNamedPipe);
			}

			if (pipe->clientfd != -1)
				close(pipe->clientfd);

			if (pipe->serverfd != -1)
				close(pipe->serverfd);

			free((char *)pipe->lpFileName);
			free((char *)pipe->lpFilePath);
			free((char *)pipe->name);
			free(pipe);
		}

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_ACCESS_TOKEN)
	{
		WINPR_ACCESS_TOKEN* token;

		token = (WINPR_ACCESS_TOKEN*) Object;

		if (token->Username)
			free(token->Username);

		if (token->Domain)
			free(token->Domain);

		free(token);
	}

	return FALSE;
}

BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
	LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions)
{
	return TRUE;
}

BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags)
{
	return TRUE;
}

BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags)
{
	return TRUE;
}

#endif
