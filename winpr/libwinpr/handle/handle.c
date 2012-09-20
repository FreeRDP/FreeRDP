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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

BOOL CloseHandle(HANDLE hObject)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hObject, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_THREAD)
	{
		return TRUE;
	}
	else if (Type == HANDLE_TYPE_MUTEX)
	{
		pthread_mutex_destroy((pthread_mutex_t*) Object);
		winpr_Handle_Remove(Object);
		free(Object);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_EVENT)
	{
		WINPR_EVENT* event;

		event = (WINPR_EVENT*) Object;

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

		winpr_Handle_Remove(Object);
		free(event);

		return TRUE;
	}
	else if (Type == HANDLE_TYPE_SEMAPHORE)
	{
#if defined __APPLE__
		semaphore_destroy(mach_task_self(), *((winpr_sem_t*) Object));
#else
		sem_destroy((winpr_sem_t*) Object);
#endif
		winpr_Handle_Remove(Object);
		free(Object);

		return TRUE;
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
