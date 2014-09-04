/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Hardening <contact@hardening-consulting.com>
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

#ifdef HAVE_PTHREAD_GNU_EXT
#define _GNU_SOURCE
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#else
#ifndef _WIN32
#include <sys/select.h>
#endif
#endif

#include <assert.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/platform.h>

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

#include <time.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "../handle/handle.h"

#include "../pipe/pipe.h"

#ifdef __MACH__

#include <mach/mach_time.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0

int clock_gettime(int clk_id, struct timespec *t)
{
	UINT64 time;
	double seconds;
	double nseconds;
	mach_timebase_info_data_t timebase;

	mach_timebase_info(&timebase);
	time = mach_absolute_time();

	nseconds = ((double) time * (double) timebase.numer) / ((double) timebase.denom);
	seconds = ((double) time * (double) timebase.numer) / ((double) timebase.denom * 1e9);

	t->tv_sec = seconds;
	t->tv_nsec = nseconds;

	return 0;
}

#endif

/* Drop in replacement for the linux pthread_timedjoin_np and
 * pthread_mutex_timedlock functions.
 */
#if !defined(HAVE_PTHREAD_GNU_EXT)
#include <pthread.h>

static long long ts_difftime(const struct timespec *o,
		const struct timespec *n)
{
	long long oldValue = o->tv_sec * 1000000000LL + o->tv_nsec;
	long long newValue = n->tv_sec * 1000000000LL + n->tv_nsec;

	return newValue - oldValue;
}

static int pthread_timedjoin_np(pthread_t td, void **res,
		struct timespec *timeout)
{
	struct timespec timenow;
	struct timespec sleepytime;
	/* This is just to avoid a completely busy wait */
	sleepytime.tv_sec = 0;
	sleepytime.tv_nsec = 10000000; /* 10ms */

	do
	{
		if (pthread_kill(td, 0))
			return pthread_join(td, res);

		nanosleep(&sleepytime, NULL);
 
		clock_gettime(CLOCK_MONOTONIC, &timenow);

		if (ts_difftime(timeout, &timenow) >= 0)
		{
			return ETIMEDOUT;
		}
	}
	while (TRUE);

	return ETIMEDOUT;
}

#if defined(__FreeBSD__)
	/*the only way to get it work is to remove the static*/
	int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *timeout)
#else
	static int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *timeout)
#endif
{
	struct timespec timenow;
	struct timespec sleepytime;
	int retcode;

	/* This is just to avoid a completely busy wait */
	sleepytime.tv_sec = 0;
	sleepytime.tv_nsec = 10000000; /* 10ms */

	while ((retcode = pthread_mutex_trylock (mutex)) == EBUSY)
	{
		clock_gettime(CLOCK_MONOTONIC, &timenow);

		if (ts_difftime(timeout, &timenow) >= 0)
		{
			return ETIMEDOUT;
		}

		nanosleep (&sleepytime, NULL);
	}

	return retcode;
}
#endif

static void ts_add_ms(struct timespec *ts, DWORD dwMilliseconds)
{
	ts->tv_sec += dwMilliseconds / 1000L;
	ts->tv_nsec += (dwMilliseconds % 1000L) * 1000000L;

	ts->tv_sec += ts->tv_nsec / 1000000000L;
	ts->tv_nsec = ts->tv_nsec % 1000000000L;
}

static int waitOnFd(int fd, DWORD dwMilliseconds)
{
	int status;

#ifdef HAVE_POLL_H
	struct pollfd pollfds;

	pollfds.fd = fd;
	pollfds.events = POLLIN;
	pollfds.revents = 0;

	do
	{
		status = poll(&pollfds, 1, dwMilliseconds);
	}
	while ((status < 0) && (errno == EINTR));

#else
	struct timeval timeout;
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	ZeroMemory(&timeout, sizeof(timeout));

	if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
	{
		timeout.tv_sec = dwMilliseconds / 1000;
		timeout.tv_usec = (dwMilliseconds % 1000) * 1000;
	}

	do
	{
		status = select(fd + 1, &rfds, NULL, NULL, (dwMilliseconds == INFINITE) ? NULL : &timeout);
	}
	while (status < 0 && (errno == EINTR));
#endif

	return status;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hHandle, &Type, &Object))
	{
		fprintf(stderr, "WaitForSingleObject failed: invalid hHandle.\n");
		return WAIT_FAILED;
	}

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
				struct timespec timeout;

				/* pthread_timedjoin_np returns ETIMEDOUT in case the timeout is 0,
				 * so set it to the smallest value to get a proper return value. */
				if (dwMilliseconds == 0)
					dwMilliseconds ++;

				clock_gettime(CLOCK_MONOTONIC, &timeout);
				ts_add_ms(&timeout, dwMilliseconds);

				status = pthread_timedjoin_np(thread->thread, &thread_status, &timeout);

				if (ETIMEDOUT == status)
					return WAIT_TIMEOUT;
			}
			else
				status = pthread_join(thread->thread, &thread_status);

			thread->started = FALSE;

			if (status != 0)
			{
				fprintf(stderr, "WaitForSingleObject: pthread_join failure: [%d] %s\n",
						status, strerror(status));
			}

			if (thread_status)
				thread->dwExitCode = ((DWORD) (size_t) thread_status);
		}
	}
	else if (Type == HANDLE_TYPE_PROCESS)
	{
		WINPR_PROCESS* process;

		process = (WINPR_PROCESS*) Object;

		if (waitpid(process->pid, &(process->status), 0) != -1)
		{
			fprintf(stderr, "WaitForSingleObject: waitpid failure [%d] %s\n", errno, strerror(errno));
			return WAIT_FAILED;
		}

		process->dwExitCode = (DWORD) process->status;
	}
	else if (Type == HANDLE_TYPE_MUTEX)
	{
		WINPR_MUTEX* mutex;

		mutex = (WINPR_MUTEX*) Object;

		if (dwMilliseconds != INFINITE)
		{
			int status;
			struct timespec timeout;

			clock_gettime(CLOCK_MONOTONIC, &timeout);
			ts_add_ms(&timeout, dwMilliseconds);	

			status = pthread_mutex_timedlock(&mutex->mutex, &timeout);

			if (ETIMEDOUT == status)
				return WAIT_TIMEOUT;
		}
		else
		{
			pthread_mutex_lock(&mutex->mutex);
		}
	}
	else if (Type == HANDLE_TYPE_EVENT)
	{
		int status;
		WINPR_EVENT* event;

		event = (WINPR_EVENT*) Object;

		status = waitOnFd(event->pipe_fd[0], dwMilliseconds);

		if (status < 0)
		{
			fprintf(stderr, "WaitForSingleObject: event select() failure [%d] %s\n", errno, strerror(errno));
			return WAIT_FAILED;
		}

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

			status = waitOnFd(semaphore->pipe_fd[0], dwMilliseconds);
			if (status < 0)
			{
				fprintf(stderr, "WaitForSingleObject: semaphore select() failure [%d] %s\n", errno, strerror(errno));
				return WAIT_FAILED;
			}

			if (status != 1)
				return WAIT_TIMEOUT;

			length = read(semaphore->pipe_fd[0], &length, 1);

			if (length != 1)
			{
				fprintf(stderr, "WaitForSingleObject: semaphore read failure [%d] %s\n", errno, strerror(errno));
				return WAIT_FAILED;
			}
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
			UINT64 expirations;

			status = waitOnFd(timer->fd, dwMilliseconds);
			if (status < 0)
			{
				fprintf(stderr, "WaitForSingleObject: timer select() failure [%d] %s\n", errno, strerror(errno));
				return WAIT_FAILED;
			}

			if (status != 1)
				return WAIT_TIMEOUT;

			status = read(timer->fd, (void*) &expirations, sizeof(UINT64));

			if (status != 8)
			{
				if (status == -1)
				{
					if (errno == ETIMEDOUT)
						return WAIT_TIMEOUT;

					fprintf(stderr, "WaitForSingleObject: timer read() failure [%d] %s\n", errno, strerror(errno));
				}
				else
				{
					fprintf(stderr, "WaitForSingleObject: timer read() failure - incorrect number of bytes read");
				}

				return WAIT_FAILED;
			}
		}
		else
		{
			fprintf(stderr, "WaitForSingleObject: invalid timer file descriptor\n");
			return WAIT_FAILED;
		}

#else
		fprintf(stderr, "WaitForSingleObject: file descriptors not supported\n");
		return WAIT_FAILED;
#endif
	}
	else if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		int fd;
		int status;
		WINPR_NAMED_PIPE* pipe = (WINPR_NAMED_PIPE*) Object;

		fd = (pipe->ServerMode) ? pipe->serverfd : pipe->clientfd;

		if (fd == -1)
		{
			fprintf(stderr, "WaitForSingleObject: invalid pipe file descriptor\n");
			return WAIT_FAILED;
		}

		status = waitOnFd(fd, dwMilliseconds);
		if (status < 0)
		{
			fprintf(stderr, "WaitForSingleObject: named pipe select() failure [%d] %s\n", errno, strerror(errno));
			return WAIT_FAILED;
		}

		if (status != 1)
		{
			return WAIT_TIMEOUT;
		}
	}
	else
	{
		fprintf(stderr, "WaitForSingleObject: unknown handle type %d\n", (int) Type);
	}

	return WAIT_OBJECT_0;
}

DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable)
{
	fprintf(stderr, "[ERROR] %s: Function not implemented.\n", __func__);
	assert(0);
	return WAIT_OBJECT_0;
}

#define MAXIMUM_WAIT_OBJECTS 64

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
	int fd = -1;
	int index;
	int status;
	ULONG Type;
	PVOID Object;
#ifdef HAVE_POLL_H
	struct pollfd *pollfds;
#else
	int maxfd;
	fd_set fds;
	struct timeval timeout;
#endif

	if (!nCount || (nCount > MAXIMUM_WAIT_OBJECTS))
	{
		fprintf(stderr, "%s: invalid handles count(%d)\n", __FUNCTION__, nCount);
		return WAIT_FAILED;
	}

#ifdef HAVE_POLL_H
	pollfds = alloca(nCount * sizeof(struct pollfd));
#else
	maxfd = 0;
	FD_ZERO(&fds);
	ZeroMemory(&timeout, sizeof(timeout));

#endif

	if (bWaitAll)
	{
		fprintf(stderr, "%s: bWaitAll not yet implemented\n", __FUNCTION__);
		assert(0);
	}

	for (index = 0; index < nCount; index++)
	{
		if (!winpr_Handle_GetInfo(lpHandles[index], &Type, &Object))
		{
			fprintf(stderr, "%s: invalid handle\n", __FUNCTION__);

			return WAIT_FAILED;
		}

		if (Type == HANDLE_TYPE_EVENT)
		{
			fd = ((WINPR_EVENT*) Object)->pipe_fd[0];

			if (fd == -1)
			{
				fprintf(stderr, "%s: invalid event file descriptor\n", __FUNCTION__);
				return WAIT_FAILED;
			}
		}
		else if (Type == HANDLE_TYPE_SEMAPHORE)
		{
#ifdef WINPR_PIPE_SEMAPHORE
			fd = ((WINPR_SEMAPHORE*) Object)->pipe_fd[0];
#else
			fprintf(stderr, "%s: semaphore not supported\n", __FUNCTION__);
			return WAIT_FAILED;
#endif
		}
		else if (Type == HANDLE_TYPE_TIMER)
		{
			WINPR_TIMER* timer = (WINPR_TIMER*) Object;
			fd = timer->fd;

			if (fd == -1)
			{
				fprintf(stderr, "%s: invalid timer file descriptor\n", __FUNCTION__);
				return WAIT_FAILED;
			}
		}
		else if (Type == HANDLE_TYPE_NAMED_PIPE)
		{
			WINPR_NAMED_PIPE* pipe = (WINPR_NAMED_PIPE*) Object;
			fd = (pipe->ServerMode) ? pipe->serverfd : pipe->clientfd;

			if (fd == -1)
			{
				fprintf(stderr, "%s: invalid timer file descriptor\n", __FUNCTION__);
				return WAIT_FAILED;
			}
		}
		else
		{
			fprintf(stderr, "%s: unknown handle type %d\n", __FUNCTION__, (int) Type);
			return WAIT_FAILED;
		}

		if (fd == -1)
		{
			fprintf(stderr, "%s: invalid file descriptor\n", __FUNCTION__);
			return WAIT_FAILED;
		}

#ifdef HAVE_POLL_H
		pollfds[index].fd = fd;
		pollfds[index].events = POLLIN;
		pollfds[index].revents = 0;
#else
		FD_SET(fd, &fds);

		if (fd > maxfd)
			maxfd = fd;
#endif
	}

#ifdef HAVE_POLL_H
	do
	{
		status = poll(pollfds, nCount, dwMilliseconds);
	}
	while (status < 0 && errno == EINTR);
#else
	if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
	{
		timeout.tv_sec = dwMilliseconds / 1000;
		timeout.tv_usec = (dwMilliseconds % 1000) * 1000;
	}

	do
	{
		status = select(maxfd + 1, &fds, 0, 0,
				(dwMilliseconds == INFINITE) ? NULL : &timeout);
	}
	while (status < 0 && errno == EINTR);
#endif

	if (status < 0)
	{
		fprintf(stderr, "%s: select() failure [%d] %s\n", __FUNCTION__, errno, strerror(errno));
		return WAIT_FAILED;
	}

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
			fd = (pipe->ServerMode) ? pipe->serverfd : pipe->clientfd;
		}

#ifdef HAVE_POLL_H
		if (pollfds[index].revents & POLLIN)
#else
		if (FD_ISSET(fd, &fds))
#endif
		{
			if (Type == HANDLE_TYPE_SEMAPHORE)
			{
				int length;

				length = read(fd, &length, 1);

				if (length != 1)
				{
					fprintf(stderr, "%s: semaphore read() failure [%d] %s\n", __FUNCTION__, errno, strerror(errno));
					return WAIT_FAILED;
				}
			}
			else if (Type == HANDLE_TYPE_TIMER)
			{
				int length;
				UINT64 expirations;

				length = read(fd, (void*) &expirations, sizeof(UINT64));

				if (length != 8)
				{
					if (length == -1)
					{
						if (errno == ETIMEDOUT)
							return WAIT_TIMEOUT;

						fprintf(stderr, "%s: timer read() failure [%d] %s\n", __FUNCTION__, errno, strerror(errno));
					}
					else
					{
						fprintf(stderr, "%s: timer read() failure - incorrect number of bytes read", __FUNCTION__);
					}

					return WAIT_FAILED;
				}
			}

			return (WAIT_OBJECT_0 + index);
		}
	}

	fprintf(stderr, "%s: failed (unknown error)\n", __FUNCTION__);
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

