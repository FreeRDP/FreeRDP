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

#include <assert.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#include "synch.h"
#include "../thread/thread.h"
#include <winpr/thread.h>

/**
 * WaitForSingleObject
 * WaitForSingleObjectEx
 * WaitForMultipleObjectsEx
 * SignalObjectAndWait
 */

#ifndef _WIN32

#include "../handle/handle.h"

#include "../pipe/pipe.h"

static void ts_add_ms(struct timespec *ts, DWORD dwMilliseconds)
{
	ts->tv_sec += dwMilliseconds / 1000L;
	ts->tv_nsec += (dwMilliseconds % 1000L) * 1000000L;

	while(ts->tv_nsec >= 1000000000L)
	{
		ts->tv_sec ++;
		ts->tv_nsec -= 1000000000L;
	}
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hHandle, &Type, &Object))
		return WAIT_FAILED;

	if (Type == HANDLE_TYPE_THREAD)
	{
		int status = 0;
		WINPR_THREAD* thread;
		void* thread_status = NULL;

		thread = (WINPR_THREAD*) Object;

		if (thread->started)
		{
			if (dwMilliseconds != INFINITE)
			{
#if HAVE_PTHREAD_GNU_EXT
				struct timespec timeout;

				clock_gettime(CLOCK_REALTIME, &timeout);
				ts_add_ms(&timeout, dwMilliseconds);

				status = pthread_timedjoin_np(thread->thread, &thread_status, &timeout);
#else
				fprintf(stderr, "[ERROR] %s: Thread timeouts not implemented.\n", __func__);
				assert(0);
#endif
			}
			else
				status = pthread_join(thread->thread, &thread_status);

			if (status != 0)
				fprintf(stderr, "WaitForSingleObject: pthread_join failure: [%d] %s\n",
						status, strerror(status));

			if (thread_status)
				thread->dwExitCode = ((DWORD) (size_t) thread_status);
		}
	}
	else if (Type == HANDLE_TYPE_MUTEX)
	{
		WINPR_MUTEX* mutex;

		mutex = (WINPR_MUTEX*) Object;

#if HAVE_PTHREAD_GNU_EXT
		if (dwMilliseconds != INFINITE)
		{
			struct timespec timeout;

			clock_gettime(CLOCK_REALTIME, &timeout);
			ts_add_ms(&timeout, dwMilliseconds);	

			pthread_mutex_timedlock(&mutex->mutex, &timeout);
		}
		else
#endif
		{
			pthread_mutex_lock(&mutex->mutex);
		}
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

		status = select(event->pipe_fd[0] + 1, &rfds, NULL, NULL,
				(dwMilliseconds == INFINITE) ? NULL : &timeout);

		if (status < 0)
			return WAIT_FAILED;

		if (status != 1)
			return WAIT_TIMEOUT;
	}
	else if (Type == HANDLE_TYPE_SEMAPHORE)
	{
		WINPR_SEMAPHORE* semaphore;

		semaphore = (WINPR_SEMAPHORE*) Object;

#ifdef WINPR_PIPE_SEMAPHORE
		if (semaphore->pipe_fd[0] != -1)
		{
			int status;
			int length;
			fd_set rfds;
			struct timeval timeout;

			FD_ZERO(&rfds);
			FD_SET(semaphore->pipe_fd[0], &rfds);
			ZeroMemory(&timeout, sizeof(timeout));

			if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
			{
				timeout.tv_usec = dwMilliseconds * 1000;
			}

			status = select(semaphore->pipe_fd[0] + 1, &rfds, 0, 0,
					(dwMilliseconds == INFINITE) ? NULL : &timeout);

			if (status < 0)
				return WAIT_FAILED;

			if (status != 1)
				return WAIT_TIMEOUT;

			length = read(semaphore->pipe_fd[0], &length, 1);

			if (length != 1)
				return WAIT_FAILED;
		}
#else

#if defined __APPLE__
		semaphore_wait(*((winpr_sem_t*) semaphore->sem));
#else
		sem_wait((winpr_sem_t*) semaphore->sem);
#endif

#endif
	}
	else if (Type == HANDLE_TYPE_TIMER)
	{
		WINPR_TIMER* timer;

		timer = (WINPR_TIMER*) Object;

#ifdef HAVE_EVENTFD_H
		if (timer->fd != -1)
		{
			int status;
			fd_set rfds;
			UINT64 expirations;
			struct timeval timeout;

			FD_ZERO(&rfds);
			FD_SET(timer->fd, &rfds);
			ZeroMemory(&timeout, sizeof(timeout));

			if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
			{
				timeout.tv_usec = dwMilliseconds * 1000;
			}

			status = select(timer->fd + 1, &rfds, 0, 0,
					(dwMilliseconds == INFINITE) ? NULL : &timeout);

			if (status < 0)
				return WAIT_FAILED;

			if (status != 1)
				return WAIT_TIMEOUT;

			status = read(timer->fd, (void*) &expirations, sizeof(UINT64));

			if (status != 8)
				return WAIT_TIMEOUT;
		}
		else
		{
			return WAIT_FAILED;
		}
#else
		return WAIT_FAILED;
#endif
	}
	else if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		int status;
		fd_set rfds;
		struct timeval timeout;
		WINPR_NAMED_PIPE* pipe = (WINPR_NAMED_PIPE*) Object;

		FD_ZERO(&rfds);
		FD_SET(pipe->clientfd, &rfds);
		ZeroMemory(&timeout, sizeof(timeout));

		if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
		{
			timeout.tv_usec = dwMilliseconds * 1000;
		}

		status = select(pipe->clientfd + 1, &rfds, NULL, NULL,
				(dwMilliseconds == INFINITE) ? NULL : &timeout);

		if (status < 0)
			return WAIT_FAILED;

		if (status != 1)
			return WAIT_TIMEOUT;
	}
	else
	{
		fprintf(stderr, "WaitForSingleObject: unknown handle type %lu\n", Type);
	}

	return WAIT_OBJECT_0;
}

DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable)
{
	fprintf(stderr, "[ERROR] %s: Function not implemented.\n", __func__);
	assert(0);
	return WAIT_OBJECT_0;
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
	int fd = -1;
	int maxfd;
	int index;
	int status;
	fd_set fds;
	ULONG Type;
	PVOID Object;
	struct timeval timeout;

	if (!nCount)
		return WAIT_FAILED;

	maxfd = 0;
	FD_ZERO(&fds);
	ZeroMemory(&timeout, sizeof(timeout));

	if (bWaitAll)
	{
		fprintf(stderr, "WaitForMultipleObjects: bWaitAll not yet implemented\n");
		assert(0);
	}

	for (index = 0; index < nCount; index++)
	{
		if (!winpr_Handle_GetInfo(lpHandles[index], &Type, &Object))
			return WAIT_FAILED;

		if (Type == HANDLE_TYPE_EVENT)
		{
			fd = ((WINPR_EVENT*) Object)->pipe_fd[0];
		}
		else if (Type == HANDLE_TYPE_SEMAPHORE)
		{
#ifdef WINPR_PIPE_SEMAPHORE
			fd = ((WINPR_SEMAPHORE*) Object)->pipe_fd[0];
#else
			return WAIT_FAILED;
#endif
		}
		else if (Type == HANDLE_TYPE_TIMER)
		{
			WINPR_TIMER* timer = (WINPR_TIMER*) Object;
			fd = timer->fd;
		}
		else if (Type == HANDLE_TYPE_NAMED_PIPE)
		{
			WINPR_NAMED_PIPE* pipe = (WINPR_NAMED_PIPE*) Object;
			fd = pipe->clientfd;
		}
		else
		{
			return WAIT_FAILED;
		}

		if (fd == -1)
			return WAIT_FAILED;

		FD_SET(fd, &fds);

		if (fd > maxfd)
			maxfd = fd;
	}

	if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
	{
		timeout.tv_usec = dwMilliseconds * 1000;
	}

	status = select(maxfd + 1, &fds, 0, 0,
			(dwMilliseconds == INFINITE) ? NULL : &timeout);

	if (status < 0)
		return WAIT_FAILED;

	if (status == 0)
		return WAIT_TIMEOUT;

	for (index = 0; index < nCount; index++)
	{
		winpr_Handle_GetInfo(lpHandles[index], &Type, &Object);

		if (Type == HANDLE_TYPE_EVENT)
		{
			fd = ((WINPR_EVENT*) Object)->pipe_fd[0];
		}
		else if (Type == HANDLE_TYPE_SEMAPHORE)
		{
			fd = ((WINPR_SEMAPHORE*) Object)->pipe_fd[0];
		}
		else if (Type == HANDLE_TYPE_TIMER)
		{
			WINPR_TIMER* timer = (WINPR_TIMER*) Object;
			fd = timer->fd;
		}
		else if (Type == HANDLE_TYPE_NAMED_PIPE)
		{
			WINPR_NAMED_PIPE* pipe = (WINPR_NAMED_PIPE*) Object;
			fd = pipe->clientfd;
		}

		if (FD_ISSET(fd, &fds))
		{
			if (Type == HANDLE_TYPE_SEMAPHORE)
			{
				int length;

				length = read(fd, &length, 1);

				if (length != 1)
					return WAIT_FAILED;
			}
			else if (Type == HANDLE_TYPE_TIMER)
			{
				int length;
				UINT64 expirations;

				length = read(fd, (void*) &expirations, sizeof(UINT64));

				if (length != 8)
					return WAIT_FAILED;
			}

			return (WAIT_OBJECT_0 + index);
		}
	}

	return WAIT_FAILED;
}

DWORD WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable)
{
	fprintf(stderr, "[ERROR] %s: Function not implemented.\n", __func__);
	assert(0);
	return 0;
}

DWORD SignalObjectAndWait(HANDLE hObjectToSignal, HANDLE hObjectToWaitOn, DWORD dwMilliseconds, BOOL bAlertable)
{
	fprintf(stderr, "[ERROR] %s: Function not implemented.\n", __func__);
	assert(0);
	return 0;
}

#endif
