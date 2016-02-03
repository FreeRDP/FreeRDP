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
#include <winpr/debug.h>

#include "../log.h"
#define TAG WINPR_TAG("sync.wait")

/**
 * WaitForSingleObject
 * WaitForSingleObjectEx
 * WaitForMultipleObjectsEx
 * SignalObjectAndWait
 */

#ifndef _WIN32

#include <stdlib.h>
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

/* Drop in replacement for pthread_mutex_timedlock
 */
#if !defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)
#include <pthread.h>

static int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *timeout)
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

#ifdef HAVE_POLL_H
static DWORD handle_mode_to_pollevent(ULONG mode)
{
	DWORD event = 0;
	if (mode & WINPR_FD_READ)
		event |= POLLIN;
	if (mode & WINPR_FD_WRITE)
		event |= POLLOUT;

	return event;
}
#endif

static void ts_add_ms(struct timespec *ts, DWORD dwMilliseconds)
{
	ts->tv_sec += dwMilliseconds / 1000L;
	ts->tv_nsec += (dwMilliseconds % 1000L) * 1000000L;
	ts->tv_sec += ts->tv_nsec / 1000000000L;
	ts->tv_nsec = ts->tv_nsec % 1000000000L;
}

static int waitOnFd(int fd, ULONG mode, DWORD dwMilliseconds)
{
	int status;
#ifdef HAVE_POLL_H
	struct pollfd pollfds;
	pollfds.fd = fd;
	pollfds.events = handle_mode_to_pollevent(mode);
	pollfds.revents = 0;

	do
	{
		status = poll(&pollfds, 1, dwMilliseconds);
	}
	while ((status < 0) && (errno == EINTR));

#else
	struct timeval timeout;
	fd_set rfds, wfds;
	fd_set* prfds = NULL;
	fd_set* pwfds = NULL;
	fd_set* pefds = NULL;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);
	ZeroMemory(&timeout, sizeof(timeout));

	if (mode & WINPR_FD_READ)
		prfds = &rfds;
	if (mode & WINPR_FD_WRITE)
		pwfds = &wfds;

	if ((dwMilliseconds != INFINITE) && (dwMilliseconds != 0))
	{
		timeout.tv_sec = dwMilliseconds / 1000;
		timeout.tv_usec = (dwMilliseconds % 1000) * 1000;
	}

	do
	{
		status = select(fd + 1, prfds, pwfds, pefds, (dwMilliseconds == INFINITE) ? NULL : &timeout);
	}
	while (status < 0 && (errno == EINTR));

#endif
	return status;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
	ULONG Type;
	WINPR_HANDLE* Object;

	if (!winpr_Handle_GetInfo(hHandle, &Type, &Object))
	{
		WLog_ERR(TAG, "invalid hHandle.");
		SetLastError(ERROR_INVALID_HANDLE);
		return WAIT_FAILED;
	}

	if (Type == HANDLE_TYPE_PROCESS)
	{
		WINPR_PROCESS *process;
		process = (WINPR_PROCESS *) Object;

		if (process->pid != waitpid(process->pid, &(process->status), 0))
		{
			WLog_ERR(TAG, "waitpid failure [%d] %s", errno, strerror(errno));
			SetLastError(ERROR_INTERNAL_ERROR);
			return WAIT_FAILED;
		}

		process->dwExitCode = (DWORD) process->status;
		return WAIT_OBJECT_0;
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

		return WAIT_OBJECT_0;
	}
	else
	{
		int status;
		int fd = winpr_Handle_getFd(Object);
		if (fd < 0)
		{
			WLog_ERR(TAG, "winpr_Handle_getFd did not return a fd!");
			SetLastError(ERROR_INVALID_HANDLE);
			return WAIT_FAILED;
		}


		status = waitOnFd(fd, Object->Mode, dwMilliseconds);

		if (status < 0)
		{
			WLog_ERR(TAG, "waitOnFd() failure [%d] %s", errno, strerror(errno));
			SetLastError(ERROR_INTERNAL_ERROR);
			return WAIT_FAILED;
		}

		if (status != 1)
			return WAIT_TIMEOUT;

		return winpr_Handle_cleanup(Object);
	}

	SetLastError(ERROR_INTERNAL_ERROR);
	return WAIT_FAILED;
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
	DWORD *poll_map = NULL;
	BOOL *signalled_idx = NULL;
	int fd = -1;
	int index;
	int status;
	ULONG Type;
	BOOL signal_handled = FALSE;
	WINPR_HANDLE* Object;
#ifdef HAVE_POLL_H
	struct pollfd *pollfds;
#else
	int maxfd;
	fd_set rfds;
	fd_set wfds;
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
#ifndef HAVE_POLL_H
		fd_set* prfds = NULL;
		fd_set* pwfds = NULL;
		maxfd = 0;

		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		ZeroMemory(&timeout, sizeof(timeout));
#endif
		if (bWaitAll && (dwMilliseconds != INFINITE))
			clock_gettime(CLOCK_MONOTONIC, &starttime);

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
				SetLastError(ERROR_INVALID_HANDLE);
				return WAIT_FAILED;
			}

			fd = winpr_Handle_getFd(Object);

			if (fd == -1)
			{
				WLog_ERR(TAG, "invalid file descriptor");
				SetLastError(ERROR_INVALID_HANDLE);
				return WAIT_FAILED;
			}

#ifdef HAVE_POLL_H
			pollfds[polled].fd = fd;
			pollfds[polled].events = handle_mode_to_pollevent(Object->Mode);
			pollfds[polled].revents = 0;
#else
			FD_SET(fd, &rfds);
			FD_SET(fd, &wfds);

			if (Object->Mode & WINPR_FD_READ)
				prfds = &rfds;
			if (Object->Mode & WINPR_FD_WRITE)
				pwfds = &wfds;

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
			status = select(maxfd + 1, prfds, pwfds, 0,
					(dwMilliseconds == INFINITE) ? NULL : &timeout);
		}
		while (status < 0 && errno == EINTR);

#endif

		if (status < 0)
		{
#ifdef HAVE_POLL_H
			WLog_ERR(TAG, "poll() handle %d (%d) failure [%d] %s", index, nCount, errno,
					 strerror(errno));
#else
			WLog_ERR(TAG, "select() handle %d (%d) failure [%d] %s", index, nCount, errno,
					 strerror(errno));
#endif
			winpr_log_backtrace(TAG, WLOG_ERROR, 20);
			SetLastError(ERROR_INTERNAL_ERROR);
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

		signal_handled = FALSE;
		for (index = 0; index < polled; index++)
		{
			DWORD idx;
			BOOL signal_set = FALSE;

			if (bWaitAll)
				idx = poll_map[index];
			else
				idx = index;

			if (!winpr_Handle_GetInfo(lpHandles[idx], &Type, &Object))
			{
					WLog_ERR(TAG, "invalid hHandle.");
					SetLastError(ERROR_INVALID_HANDLE);
					return WAIT_FAILED;
			}

			fd = winpr_Handle_getFd(lpHandles[idx]);

			if (fd == -1)
			{
				WLog_ERR(TAG, "invalid file descriptor");
				SetLastError(ERROR_INVALID_HANDLE);
				return WAIT_FAILED;
			}

#ifdef HAVE_POLL_H
			signal_set = pollfds[index].revents & pollfds[index].events;
#else
			if (Object->Mode & WINPR_FD_READ)
				signal_set = FD_ISSET(fd, &rfds) ? 1 : 0;
			if (Object->Mode & WINPR_FD_WRITE)
				signal_set |= FD_ISSET(fd, &wfds) ? 1 : 0;
#endif
			if (signal_set)
			{
				DWORD rc = winpr_Handle_cleanup(lpHandles[idx]);
				if (rc != WAIT_OBJECT_0)
					return rc;

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

				signal_handled = TRUE;
			}
		}
	}
	while (bWaitAll || !signal_handled);

	WLog_ERR(TAG, "failed (unknown error)");
	SetLastError(ERROR_INTERNAL_ERROR);
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

