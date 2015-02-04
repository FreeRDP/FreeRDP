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

#include "../log.h"
#define TAG WINPR_TAG("sync.wait")

/**
 * WaitForSingleObject
 * WaitForSingleObjectEx
 * WaitForMultipleObjectsEx
 * SignalObjectAndWait
 */

#ifndef _WIN32

#include <alloca.h>
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


static long long ts_difftime(const struct timespec *o,
							 const struct timespec *n)
{
	long long oldValue = o->tv_sec * 1000000000LL + o->tv_nsec;
	long long newValue = n->tv_sec * 1000000000LL + n->tv_nsec;
	return newValue - oldValue;
}

/* Drop in replacement for the linux pthread_timedjoin_np and
 * pthread_mutex_timedlock functions.
 */
#if !defined(HAVE_PTHREAD_GNU_EXT)
#include <pthread.h>

#if defined(__FreeBSD__) || defined(sun)
/*the only way to get it work is to remove the static*/
int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *timeout)
#else
static int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *timeout)
#endif
{
	struct timespec timenow;
	struct timespec sleepytime;
	unsigned long long diff;
	int retcode;
	/* This is just to avoid a completely busy wait */
	clock_gettime(CLOCK_MONOTONIC, &timenow);
	diff = ts_difftime(&timenow, timeout);
	sleepytime.tv_sec = diff / 1000000000LL;
	sleepytime.tv_nsec = diff % 1000000000LL;

	while ((retcode = pthread_mutex_trylock(mutex)) == EBUSY)
	{
		clock_gettime(CLOCK_MONOTONIC, &timenow);

		if (ts_difftime(timeout, &timenow) >= 0)
		{
			return ETIMEDOUT;
		}

		nanosleep(&sleepytime, NULL);
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
		WLog_ERR(TAG, "invalid hHandle.");
		return WAIT_FAILED;
	}

	if (Type == HANDLE_TYPE_THREAD)
	{
		int status;
		WINPR_THREAD *thread = (WINPR_THREAD *)Object;
		status = waitOnFd(thread->pipe_fd[0], dwMilliseconds);

		if (status < 0)
		{
			WLog_ERR(TAG, "waitOnFd() failure [%d] %s", errno, strerror(errno));
			return WAIT_FAILED;
		}

		if (status != 1)
			return WAIT_TIMEOUT;

		pthread_mutex_lock(&thread->mutex);

		if (!thread->joined)
		{
			status = pthread_join(thread->thread, NULL);

			if (status != 0)
			{
				WLog_ERR(TAG, "pthread_join failure: [%d] %s",
						 status, strerror(status));
				pthread_mutex_unlock(&thread->mutex);
				return WAIT_FAILED;
			}
			else
				thread->joined = TRUE;
		}

		pthread_mutex_unlock(&thread->mutex);
	}
	else if (Type == HANDLE_TYPE_PROCESS)
	{
		WINPR_PROCESS *process;
		process = (WINPR_PROCESS *) Object;

		if (waitpid(process->pid, &(process->status), 0) != -1)
		{
			WLog_ERR(TAG, "waitpid failure [%d] %s", errno, strerror(errno));
			return WAIT_FAILED;
		}

		process->dwExitCode = (DWORD) process->status;
	}
	else if (Type == HANDLE_TYPE_MUTEX)
	{
		WINPR_MUTEX *mutex;
		mutex = (WINPR_MUTEX *) Object;

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
		WINPR_EVENT *event;
		event = (WINPR_EVENT *) Object;
		status = waitOnFd(event->pipe_fd[0], dwMilliseconds);

		if (status < 0)
		{
			WLog_ERR(TAG, "event select() failure [%d] %s", errno, strerror(errno));
			return WAIT_FAILED;
		}

		if (status != 1)
			return WAIT_TIMEOUT;
	}
	else if (Type == HANDLE_TYPE_SEMAPHORE)
	{
		WINPR_SEMAPHORE *semaphore;
		semaphore = (WINPR_SEMAPHORE *) Object;
#ifdef WINPR_PIPE_SEMAPHORE

		if (semaphore->pipe_fd[0] != -1)
		{
			int status;
			int length;
			status = waitOnFd(semaphore->pipe_fd[0], dwMilliseconds);

			if (status < 0)
			{
				WLog_ERR(TAG, "semaphore select() failure [%d] %s", errno, strerror(errno));
				return WAIT_FAILED;
			}

			if (status != 1)
				return WAIT_TIMEOUT;

			length = read(semaphore->pipe_fd[0], &length, 1);

			if (length != 1)
			{
				WLog_ERR(TAG, "semaphore read failure [%d] %s", errno, strerror(errno));
				return WAIT_FAILED;
			}
		}

#else
#if defined __APPLE__
		semaphore_wait(*((winpr_sem_t *) semaphore->sem));
#else
		sem_wait((winpr_sem_t *) semaphore->sem);
#endif
#endif
	}
	else if (Type == HANDLE_TYPE_TIMER)
	{
		WINPR_TIMER *timer;
		timer = (WINPR_TIMER *) Object;
#ifdef HAVE_EVENTFD_H

		if (timer->fd != -1)
		{
			int status;
			UINT64 expirations;
			status = waitOnFd(timer->fd, dwMilliseconds);

			if (status < 0)
			{
				WLog_ERR(TAG, "timer select() failure [%d] %s", errno, strerror(errno));
				return WAIT_FAILED;
			}

			if (status != 1)
				return WAIT_TIMEOUT;

			status = read(timer->fd, (void *) &expirations, sizeof(UINT64));

			if (status != 8)
			{
				if (status == -1)
				{
					if (errno == ETIMEDOUT)
						return WAIT_TIMEOUT;

					WLog_ERR(TAG, "timer read() failure [%d] %s", errno, strerror(errno));
				}
				else
				{
					WLog_ERR(TAG, "timer read() failure - incorrect number of bytes read");
				}

				return WAIT_FAILED;
			}
		}
		else
		{
			WLog_ERR(TAG, "invalid timer file descriptor");
			return WAIT_FAILED;
		}

#else
		WLog_ERR(TAG, "file descriptors not supported");
		return WAIT_FAILED;
#endif
	}
	else if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		int fd;
		int status;
		WINPR_NAMED_PIPE *pipe = (WINPR_NAMED_PIPE *) Object;
		fd = (pipe->ServerMode) ? pipe->serverfd : pipe->clientfd;

		if (fd == -1)
		{
			WLog_ERR(TAG, "invalid pipe file descriptor");
			return WAIT_FAILED;
		}

		status = waitOnFd(fd, dwMilliseconds);

		if (status < 0)
		{
			WLog_ERR(TAG, "named pipe select() failure [%d] %s", errno, strerror(errno));
			return WAIT_FAILED;
		}

		if (status != 1)
		{
			return WAIT_TIMEOUT;
		}
	}
	else
	{
		WLog_ERR(TAG, "unknown handle type %d", (int) Type);
	}

	return WAIT_OBJECT_0;
}

DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable)
{
	WLog_ERR(TAG, "Function not implemented.");
	assert(0);
	return WAIT_OBJECT_0;
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds)
{
	struct timespec starttime;
	struct timespec timenow;
	unsigned long long diff;
	DWORD signalled;
	DWORD polled;
	DWORD *poll_map;
	BOOL *signalled_idx;
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
		WLog_ERR(TAG, "invalid handles count(%d)", nCount);
		return WAIT_FAILED;
	}

	if (bWaitAll)
	{
		signalled_idx = alloca(nCount * sizeof(BOOL));
		memset(signalled_idx, FALSE, nCount * sizeof(BOOL));
		poll_map = alloca(nCount * sizeof(DWORD));
		memset(poll_map, 0, nCount * sizeof(DWORD));
	}

#ifdef HAVE_POLL_H
	pollfds = alloca(nCount * sizeof(struct pollfd));
#endif
	signalled = 0;

	do
	{
		if (bWaitAll && (dwMilliseconds != INFINITE))
			clock_gettime(CLOCK_MONOTONIC, &starttime);

#ifndef HAVE_POLL_H
		maxfd = 0;
		FD_ZERO(&fds);
		ZeroMemory(&timeout, sizeof(timeout));
#endif
		polled = 0;

		for (index = 0; index < nCount; index++)
		{
			if (bWaitAll)
			{
				if (signalled_idx[index])
					continue;

				poll_map[polled] = index;
			}

			if (!winpr_Handle_GetInfo(lpHandles[index], &Type, &Object))
			{
				WLog_ERR(TAG, "invalid event file descriptor");
				return WAIT_FAILED;
			}

			if (Type == HANDLE_TYPE_EVENT)
			{
				fd = ((WINPR_EVENT *) Object)->pipe_fd[0];

				if (fd == -1)
				{
					WLog_ERR(TAG, "invalid event file descriptor");
					return WAIT_FAILED;
				}
			}
			else if (Type == HANDLE_TYPE_SEMAPHORE)
			{
#ifdef WINPR_PIPE_SEMAPHORE
				fd = ((WINPR_SEMAPHORE *) Object)->pipe_fd[0];
#else
				WLog_ERR(TAG, "semaphore not supported");
				return WAIT_FAILED;
#endif
			}
			else if (Type == HANDLE_TYPE_TIMER)
			{
				WINPR_TIMER *timer = (WINPR_TIMER *) Object;
				fd = timer->fd;

				if (fd == -1)
				{
					WLog_ERR(TAG, "invalid timer file descriptor");
					return WAIT_FAILED;
				}
			}
			else if (Type == HANDLE_TYPE_THREAD)
			{
				WINPR_THREAD *thread = (WINPR_THREAD *) Object;
				fd = thread->pipe_fd[0];

				if (fd == -1)
				{
					WLog_ERR(TAG, "invalid thread file descriptor");
					return WAIT_FAILED;
				}
			}
			else if (Type == HANDLE_TYPE_NAMED_PIPE)
			{
				WINPR_NAMED_PIPE *pipe = (WINPR_NAMED_PIPE *) Object;
				fd = (pipe->ServerMode) ? pipe->serverfd : pipe->clientfd;

				if (fd == -1)
				{
					WLog_ERR(TAG, "invalid timer file descriptor");
					return WAIT_FAILED;
				}
			}
			else
			{
				WLog_ERR(TAG, "unknown handle type %d", (int) Type);
				return WAIT_FAILED;
			}

			if (fd == -1)
			{
				WLog_ERR(TAG, "invalid file descriptor");
				return WAIT_FAILED;
			}

#ifdef HAVE_POLL_H
			pollfds[polled].fd = fd;
			pollfds[polled].events = POLLIN;
			pollfds[polled].revents = 0;
#else
			FD_SET(fd, &fds);

			if (fd > maxfd)
				maxfd = fd;

#endif
			polled++;
		}

#ifdef HAVE_POLL_H

		do
		{
			status = poll(pollfds, polled, dwMilliseconds);
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
#ifdef HAVE_POLL_H
			WLog_ERR(TAG, "poll() failure [%d] %s", errno,
					 strerror(errno));
#else
			WLog_ERR(TAG, "select() failure [%d] %s", errno,
					 strerror(errno));
#endif
			return WAIT_FAILED;
		}

		if (status == 0)
			return WAIT_TIMEOUT;

		if (bWaitAll && (dwMilliseconds != INFINITE))
		{
			clock_gettime(CLOCK_MONOTONIC, &timenow);
			diff = ts_difftime(&timenow, &starttime);

			if (diff / 1000 > dwMilliseconds)
				return WAIT_TIMEOUT;
			else
				dwMilliseconds -= (diff / 1000);
		}

		for (index = 0; index < polled; index++)
		{
			DWORD idx;

			if (bWaitAll)
				idx = poll_map[index];
			else
				idx = index;

			winpr_Handle_GetInfo(lpHandles[idx], &Type, &Object);

			if (Type == HANDLE_TYPE_EVENT)
			{
				fd = ((WINPR_EVENT *) Object)->pipe_fd[0];
			}
			else if (Type == HANDLE_TYPE_SEMAPHORE)
			{
				fd = ((WINPR_SEMAPHORE *) Object)->pipe_fd[0];
			}
			else if (Type == HANDLE_TYPE_TIMER)
			{
				WINPR_TIMER *timer = (WINPR_TIMER *) Object;
				fd = timer->fd;
			}
			else if (Type == HANDLE_TYPE_THREAD)
			{
				WINPR_THREAD *thread = (WINPR_THREAD *) Object;
				fd = thread->pipe_fd[0];
			}
			else if (Type == HANDLE_TYPE_NAMED_PIPE)
			{
				WINPR_NAMED_PIPE *pipe = (WINPR_NAMED_PIPE *) Object;
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
						WLog_ERR(TAG, "semaphore read() failure [%d] %s", errno, strerror(errno));
						return WAIT_FAILED;
					}
				}
				else if (Type == HANDLE_TYPE_TIMER)
				{
					int length;
					UINT64 expirations;
					length = read(fd, (void *) &expirations, sizeof(UINT64));

					if (length != 8)
					{
						if (length == -1)
						{
							if (errno == ETIMEDOUT)
								return WAIT_TIMEOUT;

							WLog_ERR(TAG, "timer read() failure [%d] %s", errno, strerror(errno));
						}
						else
						{
							WLog_ERR(TAG, "timer read() failure - incorrect number of bytes read");
						}

						return WAIT_FAILED;
					}
				}
				else if (Type == HANDLE_TYPE_THREAD)
				{
					WINPR_THREAD *thread = (WINPR_THREAD *)Object;
					pthread_mutex_lock(&thread->mutex);

					if (!thread->joined)
					{
						int status;
						status = pthread_join(thread->thread, NULL);

						if (status != 0)
						{
							WLog_ERR(TAG, "pthread_join failure: [%d] %s",
									 status, strerror(status));
							pthread_mutex_unlock(&thread->mutex);
							return WAIT_FAILED;
						}
						else
							thread->joined = TRUE;
					}

					pthread_mutex_unlock(&thread->mutex);
				}

				if (bWaitAll)
				{
					signalled_idx[idx] = TRUE;

					/* Continue checks from last position. */
					for (; signalled < nCount; signalled++)
					{
						if (!signalled_idx[signalled])
							break;
					}
				}

				if (!bWaitAll)
					return (WAIT_OBJECT_0 + index);

				if (bWaitAll && (signalled >= nCount))
					return (WAIT_OBJECT_0);
			}
		}
	}
	while (bWaitAll);

	WLog_ERR(TAG, "failed (unknown error)");
	return WAIT_FAILED;
}

DWORD WaitForMultipleObjectsEx(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable)
{
	return WaitForMultipleObjects(nCount, lpHandles, bWaitAll, dwMilliseconds);
}

DWORD SignalObjectAndWait(HANDLE hObjectToSignal, HANDLE hObjectToWaitOn, DWORD dwMilliseconds, BOOL bAlertable)
{
	WLog_ERR(TAG, "Function not implemented.");
	assert(0);
	return 0;
}

#endif

