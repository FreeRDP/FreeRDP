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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>
#include <winpr/synch.h>

#include "synch.h"

/**
 * WaitForSingleObject
 * WaitForSingleObjectEx
 * WaitForMultipleObjectsEx
 * SignalObjectAndWait
 */

#ifndef _WIN32

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hHandle, &Type, &Object))
		return WAIT_FAILED;

	if (Type == HANDLE_TYPE_THREAD)
	{
		if (dwMilliseconds != INFINITE)
			printf("WaitForSingleObject: timeout not implemented for thread wait\n");

		pthread_join((pthread_t) Object, NULL);
	}
	if (Type == HANDLE_TYPE_MUTEX)
	{
		if (dwMilliseconds != INFINITE)
			printf("WaitForSingleObject: timeout not implemented for mutex wait\n");

		pthread_mutex_lock((pthread_mutex_t*) Object);
	}
	else if (Type == HANDLE_TYPE_EVENT)
	{
		int status;
		fd_set rfds;
		WINPR_EVENT* event;
		struct timeval timeout;

		event = (WINPR_EVENT*) Object;

		FD_ZERO(&rfds);
		FD_SET(event->pipe_fd[0], &rfds);
		ZeroMemory(&timeout, sizeof(timeout));

		if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
		{
			timeout.tv_usec = dwMilliseconds * 1000;
		}

		status = select(event->pipe_fd[0] + 1, &rfds, 0, 0,
				(dwMilliseconds == INFINITE) ? NULL : &timeout);

		if (status < 0)
			return WAIT_FAILED;

		if (status != 1)
			return WAIT_TIMEOUT;
	}
	else if (Type == HANDLE_TYPE_SEMAPHORE)
	{
#if defined __APPLE__
		semaphore_wait(*((winpr_sem_t*) Object));
#else
		sem_wait((winpr_sem_t*) Object);
#endif
	}

	return WAIT_OBJECT_0;
}

DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable)
{
	return WAIT_OBJECT_0;
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
	return 0;
}

DWORD WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable)
{
	return 0;
}

DWORD SignalObjectAndWait(HANDLE hObjectToSignal, HANDLE hObjectToWaitOn, DWORD dwMilliseconds, BOOL bAlertable)
{
	return 0;
}

#endif
