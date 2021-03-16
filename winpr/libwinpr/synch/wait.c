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
#include "pollset.h"
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

/* clock_gettime is not implemented on OSX prior to 10.12 */
#ifdef __MACH__

#include <mach/mach_time.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 0
#endif

/* clock_gettime is not implemented on OSX prior to 10.12 */
int _mach_clock_gettime(int clk_id, struct timespec* t);

int _mach_clock_gettime(int clk_id, struct timespec* t)
{
	UINT64 time;
	double seconds;
	double nseconds;
	mach_timebase_info_data_t timebase;
	mach_timebase_info(&timebase);
	time = mach_absolute_time();
	nseconds = ((double)time * (double)timebase.numer) / ((double)timebase.denom);
	seconds = ((double)time * (double)timebase.numer) / ((double)timebase.denom * 1e9);
	t->tv_sec = seconds;
	t->tv_nsec = nseconds;
	return 0;
}

/* if clock_gettime is declared, then __CLOCK_AVAILABILITY will be defined */
#ifdef __CLOCK_AVAILABILITY
/* If we compiled with Mac OSX 10.12 or later, then clock_gettime will be declared
 * * but it may be NULL at runtime. So we need to check before using it. */
int _mach_safe_clock_gettime(int clk_id, struct timespec* t);

int _mach_safe_clock_gettime(int clk_id, struct timespec* t)
{
	if (clock_gettime)
	{
		return clock_gettime(clk_id, t);
	}

	return _mach_clock_gettime(clk_id, t);
}

#define clock_gettime _mach_safe_clock_gettime
#else
#define clock_gettime _mach_clock_gettime
#endif

#endif

static long long ts_difftime(const struct timespec* o, const struct timespec* n)
{
	long long oldValue = o->tv_sec * 1000000000LL + o->tv_nsec;
	long long newValue = n->tv_sec * 1000000000LL + n->tv_nsec;
	return newValue - oldValue;
}

/* Drop in replacement for pthread_mutex_timedlock
 */
#if !defined(HAVE_PTHREAD_MUTEX_TIMEDLOCK)
#include <pthread.h>

static int pthread_mutex_timedlock(pthread_mutex_t* mutex, const struct timespec* timeout)
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


static void ts_add_ms(struct timespec* ts, DWORD dwMilliseconds)
{
	ts->tv_sec += dwMilliseconds / 1000L;
	ts->tv_nsec += (dwMilliseconds % 1000L) * 1000000L;
	ts->tv_sec += ts->tv_nsec / 1000000000L;
	ts->tv_nsec = ts->tv_nsec % 1000000000L;
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
		WINPR_PROCESS* process = (WINPR_PROCESS*)Object;

		if (process->pid != waitpid(process->pid, &(process->status), 0))
		{
			WLog_ERR(TAG, "waitpid failure [%d] %s", errno, strerror(errno));
			SetLastError(ERROR_INTERNAL_ERROR);
			return WAIT_FAILED;
		}

		process->dwExitCode = (DWORD)process->status;
		return WAIT_OBJECT_0;
	}

	if (Type == HANDLE_TYPE_MUTEX)
	{
		WINPR_MUTEX* mutex;
		mutex = (WINPR_MUTEX*)Object;

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
		WINPR_POLL_SET pollset;
		int fd = winpr_Handle_getFd(Object);

		if (fd < 0)
		{
			WLog_ERR(TAG, "winpr_Handle_getFd did not return a fd!");
			SetLastError(ERROR_INVALID_HANDLE);
			return WAIT_FAILED;
		}

		if (!pollset_init(&pollset, 1))
		{
			WLog_ERR(TAG, "unable to initialize pollset");
			SetLastError(ERROR_INTERNAL_ERROR);
			return WAIT_FAILED;
		}

		if (!pollset_add(&pollset, fd, Object->Mode))
		{
			pollset_uninit(&pollset);
			return WAIT_FAILED;
		}

		status = pollset_poll(&pollset, dwMilliseconds);
		pollset_uninit(&pollset);

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
	if (bAlertable)
	{
		/* TODO: Implement */
		WLog_ERR(TAG, "%s: Not implemented: bAlertable", __FUNCTION__);
		return WAIT_FAILED;
	}
	return WaitForSingleObject(hHandle, dwMilliseconds);
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll,
                             DWORD dwMilliseconds)
{
	struct timespec starttime;
	struct timespec timenow;
	unsigned long long diff;
	DWORD signalled;
	DWORD polled;
	DWORD* poll_map = NULL;
	BOOL* signalled_idx = NULL;
	int fd = -1;
	DWORD index;
	int status;
	ULONG Type;
	BOOL signal_handled = FALSE;
	WINPR_HANDLE* Object;
	WINPR_POLL_SET pollset;
	DWORD ret = WAIT_FAILED;

	if (!nCount || (nCount > MAXIMUM_WAIT_OBJECTS))
	{
		WLog_ERR(TAG, "invalid handles count(%" PRIu32 ")", nCount);
		return WAIT_FAILED;
	}

	if (!pollset_init(&pollset, nCount))
	{
		WLog_ERR(TAG, "unable to initialize pollset for nCount=%" PRIu32 "", nCount);
		return WAIT_FAILED;
	}

	if (bWaitAll)
	{
		signalled_idx = alloca(nCount * sizeof(BOOL));
		memset(signalled_idx, FALSE, nCount * sizeof(BOOL));
		poll_map = alloca(nCount * sizeof(DWORD));
		memset(poll_map, 0, nCount * sizeof(DWORD));
	}

	signalled = 0;

	if (bWaitAll && (dwMilliseconds != INFINITE))
		clock_gettime(CLOCK_MONOTONIC, &starttime);

	do
	{
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
				goto out;
			}

			fd = winpr_Handle_getFd(Object);
			if (fd == -1)
			{
				WLog_ERR(TAG, "invalid file descriptor");
				SetLastError(ERROR_INVALID_HANDLE);
				goto out;
			}

			if (!pollset_add(&pollset, fd, Object->Mode))
			{
				WLog_ERR(TAG, "unable to register fd in pollset");
				SetLastError(ERROR_INVALID_HANDLE);
				goto out;
			}

			polled++;
		}

		status = pollset_poll(&pollset, dwMilliseconds);
		if (status < 0)
		{
#ifdef HAVE_POLL_H
			WLog_ERR(TAG, "poll() handle %d (%" PRIu32 ") failure [%d] %s", index, nCount, errno,
			         strerror(errno));
#else
			WLog_ERR(TAG, "select() handle %d (%" PRIu32 ") failure [%d] %s", index, nCount, errno,
			         strerror(errno));
#endif
			winpr_log_backtrace(TAG, WLOG_ERROR, 20);
			SetLastError(ERROR_INTERNAL_ERROR);
			goto out;
		}

		if (status == 0)
		{
			ret = WAIT_TIMEOUT;
			goto out;
		}

		if (bWaitAll && (dwMilliseconds != INFINITE))
		{
			clock_gettime(CLOCK_MONOTONIC, &timenow);
			diff = ts_difftime(&timenow, &starttime);

			if (diff / 1000 > dwMilliseconds)
			{
				ret = WAIT_TIMEOUT;
				goto out;
			}

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

			signal_set = pollset_isSignaled(&pollset, index);

			if (signal_set)
			{
				DWORD rc = winpr_Handle_cleanup(lpHandles[idx]);
				if (rc != WAIT_OBJECT_0)
				{
					WLog_ERR(TAG, "error in cleanup function for handle at index=%d", idx);
					ret = rc;
					goto out;
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
				{
					ret = (WAIT_OBJECT_0 + index);
					goto out;
				}

				if (signalled >= nCount)
				{
					ret = WAIT_OBJECT_0;
					goto out;
				}

				signal_handled = TRUE;
			}
		}

		pollset_reset(&pollset);
	} while (bWaitAll || !signal_handled);

	WLog_ERR(TAG, "failed (unknown error)");
	SetLastError(ERROR_INTERNAL_ERROR);

out:
	pollset_uninit(&pollset);
	return ret;
}

DWORD WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll,
                               DWORD dwMilliseconds, BOOL bAlertable)
{
	if (bAlertable)
	{
		/* TODO: Implement */
		WLog_ERR(TAG, "%s: Not implemented: bAlertable", __FUNCTION__);
		return WAIT_FAILED;
	}

	return WaitForMultipleObjects(nCount, lpHandles, bWaitAll, dwMilliseconds);
}

DWORD SignalObjectAndWait(HANDLE hObjectToSignal, HANDLE hObjectToWaitOn, DWORD dwMilliseconds,
                          BOOL bAlertable)
{
	if (!SetEvent(hObjectToSignal))
		return WAIT_FAILED;

	return WaitForSingleObjectEx(hObjectToWaitOn, dwMilliseconds, bAlertable);
}

#endif
