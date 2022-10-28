/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/assert.h>
#include <winpr/sysinfo.h>

#include <winpr/synch.h>

#ifndef _WIN32
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#endif

#include "event.h"
#include "synch.h"

#ifndef _WIN32

#include "../handle/handle.h"
#include "../thread/thread.h"

#include "../log.h"
#define TAG WINPR_TAG("synch.timer")

static BOOL TimerCloseHandle(HANDLE handle);

static BOOL TimerIsHandled(HANDLE handle)
{
	return WINPR_HANDLE_IS_HANDLED(handle, HANDLE_TYPE_TIMER, FALSE);
}

static int TimerGetFd(HANDLE handle)
{
	WINPR_TIMER* timer = (WINPR_TIMER*)handle;

	if (!TimerIsHandled(handle))
		return -1;

	return timer->fd;
}

static DWORD TimerCleanupHandle(HANDLE handle)
{
	SSIZE_T length;
	UINT64 expirations;
	WINPR_TIMER* timer = (WINPR_TIMER*)handle;

	if (!TimerIsHandled(handle))
		return WAIT_FAILED;

	if (timer->bManualReset)
		return WAIT_OBJECT_0;

#ifdef TIMER_IMPL_TIMERFD
	do
	{
		length = read(timer->fd, (void*)&expirations, sizeof(UINT64));
	} while (length < 0 && errno == EINTR);

	if (length != 8)
	{
		if (length < 0)
		{
			switch (errno)
			{
				case ETIMEDOUT:
				case EAGAIN:
					return WAIT_TIMEOUT;

				default:
					break;
			}

			WLog_ERR(TAG, "timer read() failure [%d] %s", errno, strerror(errno));
		}
		else
		{
			WLog_ERR(TAG, "timer read() failure - incorrect number of bytes read");
		}

		return WAIT_FAILED;
	}
#else
	if (!winpr_event_reset(&timer->event))
	{
		WLog_ERR(TAG, "timer reset() failure");
		return WAIT_FAILED;
	}
#endif

	return WAIT_OBJECT_0;
}

typedef struct
{
	WINPR_APC_ITEM apcItem;
	WINPR_TIMER* timer;
} TimerDeleter;

static void TimerPostDelete_APC(LPVOID arg)
{
	TimerDeleter* deleter = (TimerDeleter*)arg;
	WINPR_ASSERT(deleter);
	free(deleter->timer);
	deleter->apcItem.markedForFree = TRUE;
	deleter->apcItem.markedForRemove = TRUE;
}

BOOL TimerCloseHandle(HANDLE handle)
{
	WINPR_TIMER* timer;
	timer = (WINPR_TIMER*)handle;

	if (!TimerIsHandled(handle))
		return FALSE;

#ifdef TIMER_IMPL_TIMERFD
	if (timer->fd != -1)
		close(timer->fd);
#endif

#ifdef TIMER_IMPL_POSIX
	timer_delete(timer->tid);
#endif

#ifdef TIMER_IMPL_DISPATCH
	dispatch_release(timer->queue);
	dispatch_release(timer->source);
#endif

#if defined(TIMER_IMPL_POSIX) || defined(TIMER_IMPL_DISPATCH)
	winpr_event_uninit(&timer->event);
#endif

	free(timer->name);
	if (timer->apcItem.linked)
	{
		TimerDeleter* deleter;
		WINPR_APC_ITEM* apcItem;

		switch (apc_remove(&timer->apcItem))
		{
			case APC_REMOVE_OK:
				break;
			case APC_REMOVE_DELAY_FREE:
			{
				WINPR_THREAD* thread = winpr_GetCurrentThread();
				if (!thread)
					return FALSE;

				deleter = calloc(1, sizeof(*deleter));
				if (!deleter)
				{
					WLog_ERR(TAG, "unable to allocate a timer deleter");
					return TRUE;
				}

				deleter->timer = timer;
				apcItem = &deleter->apcItem;
				apcItem->type = APC_TYPE_HANDLE_FREE;
				apcItem->alwaysSignaled = TRUE;
				apcItem->completion = TimerPostDelete_APC;
				apcItem->completionArgs = deleter;
				apc_register(thread, apcItem);
				return TRUE;
			}
			case APC_REMOVE_ERROR:
			default:
				WLog_ERR(TAG, "unable to remove timer from APC list");
				break;
		}
	}

	free(timer);
	return TRUE;
}

#ifdef TIMER_IMPL_POSIX

static void WaitableTimerSignalHandler(int signum, siginfo_t* siginfo, void* arg)
{
	WINPR_TIMER* timer = siginfo->si_value.sival_ptr;
	UINT64 data = 1;
	WINPR_UNUSED(arg);

	if (!timer || (signum != SIGALRM))
		return;

	if (!winpr_event_set(&timer->event))
		WLog_ERR(TAG, "error when notifying event");
}

static INIT_ONCE TimerSignalHandler_InitOnce = INIT_ONCE_STATIC_INIT;

static BOOL InstallTimerSignalHandler(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context)
{
	struct sigaction action;
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGALRM);
	action.sa_flags = SA_RESTART | SA_SIGINFO;
	action.sa_sigaction = WaitableTimerSignalHandler;
	sigaction(SIGALRM, &action, NULL);
	return TRUE;
}
#endif

#ifdef TIMER_IMPL_DISPATCH
static void WaitableTimerHandler(void* arg)
{
	UINT64 data = 1;
	WINPR_TIMER* timer = (WINPR_TIMER*)arg;

	if (!timer)
		return;

	if (!winpr_event_set(&timer->event))
		WLog_ERR(TAG, "failed to write to pipe");

	if (timer->lPeriod == 0)
	{
		if (timer->running)
			dispatch_suspend(timer->source);

		timer->running = FALSE;
	}
}
#endif

static int InitializeWaitableTimer(WINPR_TIMER* timer)
{
	int result = 0;

#ifdef TIMER_IMPL_TIMERFD
	timer->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (timer->fd <= 0)
		return -1;
#elif defined(TIMER_IMPL_POSIX)
	struct sigevent sigev = { 0 };
	InitOnceExecuteOnce(&TimerSignalHandler_InitOnce, InstallTimerSignalHandler, NULL, NULL);
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = SIGALRM;
	sigev.sigev_value.sival_ptr = (void*)timer;

	if ((timer_create(CLOCK_MONOTONIC, &sigev, &(timer->tid))) != 0)
	{
		WLog_ERR(TAG, "timer_create");
		return -1;
	}
#elif !defined(TIMER_IMPL_DISPATCH)
	WLog_ERR(TAG, "%s: os specific implementation is missing", __FUNCTION__);
	result = -1;
#endif

	timer->bInit = TRUE;
	return result;
}

static BOOL timer_drain_fd(int fd)
{
	UINT64 expr;
	SSIZE_T ret;

	do
	{
		ret = read(fd, &expr, sizeof(expr));
	} while (ret < 0 && errno == EINTR);

	return ret >= 0;
}

static HANDLE_OPS ops = { TimerIsHandled,
	                      TimerCloseHandle,
	                      TimerGetFd,
	                      TimerCleanupHandle,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL };

/**
 * Waitable Timer
 */

HANDLE CreateWaitableTimerA(LPSECURITY_ATTRIBUTES lpTimerAttributes, BOOL bManualReset,
                            LPCSTR lpTimerName)
{
	HANDLE handle = NULL;
	WINPR_TIMER* timer;

	if (lpTimerAttributes)
		WLog_WARN(TAG, "%s [%s] does not support lpTimerAttributes", __FUNCTION__, lpTimerName);

	timer = (WINPR_TIMER*)calloc(1, sizeof(WINPR_TIMER));

	if (timer)
	{
		WINPR_HANDLE_SET_TYPE_AND_MODE(timer, HANDLE_TYPE_TIMER, WINPR_FD_READ);
		handle = (HANDLE)timer;
		timer->fd = -1;
		timer->lPeriod = 0;
		timer->bManualReset = bManualReset;
		timer->pfnCompletionRoutine = NULL;
		timer->lpArgToCompletionRoutine = NULL;
		timer->bInit = FALSE;

		if (lpTimerName)
			timer->name = strdup(lpTimerName);

		timer->common.ops = &ops;
#if defined(TIMER_IMPL_DISPATCH) || defined(TIMER_IMPL_POSIX)
		if (!winpr_event_init(&timer->event))
			goto fail;
		timer->fd = timer->event.fds[0];
#endif

#if defined(TIMER_IMPL_DISPATCH)
		timer->queue = dispatch_queue_create(TAG, DISPATCH_QUEUE_SERIAL);

		if (!timer->queue)
			goto fail;

		timer->source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, timer->queue);

		if (!timer->source)
			goto fail;

		dispatch_set_context(timer->source, timer);
		dispatch_source_set_event_handler_f(timer->source, WaitableTimerHandler);
#endif
	}

	return handle;

#if defined(TIMER_IMPL_DISPATCH) || defined(TIMER_IMPL_POSIX)
fail:
	TimerCloseHandle(handle);
	return NULL;
#endif
}

HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpTimerAttributes, BOOL bManualReset,
                            LPCWSTR lpTimerName)
{
	HANDLE handle;
	LPSTR name = NULL;

	if (lpTimerName)
	{
		name = ConvertWCharToUtf8Alloc(lpTimerName, NULL);
		if (!name)
			return NULL;
	}

	handle = CreateWaitableTimerA(lpTimerAttributes, bManualReset, name);
	free(name);
	return handle;
}

HANDLE CreateWaitableTimerExA(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCSTR lpTimerName,
                              DWORD dwFlags, DWORD dwDesiredAccess)
{
	BOOL bManualReset = (dwFlags & CREATE_WAITABLE_TIMER_MANUAL_RESET) ? TRUE : FALSE;

	if (dwDesiredAccess != 0)
		WLog_WARN(TAG, "%s [%s] does not support dwDesiredAccess 0x%08" PRIx32, __FUNCTION__,
		          lpTimerName, dwDesiredAccess);

	return CreateWaitableTimerA(lpTimerAttributes, bManualReset, lpTimerName);
}

HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName,
                              DWORD dwFlags, DWORD dwDesiredAccess)
{
	HANDLE handle;
	LPSTR name = NULL;

	if (lpTimerName)
	{
		name = ConvertWCharToUtf8Alloc(lpTimerName, NULL);
		if (!name)
			return NULL;
	}

	handle = CreateWaitableTimerExA(lpTimerAttributes, name, dwFlags, dwDesiredAccess);
	free(name);
	return handle;
}

static void timerAPC(LPVOID arg)
{
	WINPR_TIMER* timer = (WINPR_TIMER*)arg;
	WINPR_ASSERT(timer);
	if (!timer->lPeriod)
	{
		/* this is a one time shot timer with a completion, let's remove us from
		  the APC list */
		switch (apc_remove(&timer->apcItem))
		{
			case APC_REMOVE_OK:
			case APC_REMOVE_DELAY_FREE:
				break;
			case APC_REMOVE_ERROR:
			default:
				WLog_ERR(TAG, "error removing the APC routine");
		}
	}

	if (timer->pfnCompletionRoutine)
		timer->pfnCompletionRoutine(timer->lpArgToCompletionRoutine, 0, 0);

#ifdef TIMER_IMPL_TIMERFD
	while (timer_drain_fd(timer->fd))
		;
#else
	winpr_event_reset(&timer->event);
#endif
}

BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
                      PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine,
                      BOOL fResume)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_TIMER* timer;
	LONGLONG seconds = 0;
	LONGLONG nanoseconds = 0;
	int status = 0;

	if (!winpr_Handle_GetInfo(hTimer, &Type, &Object))
		return FALSE;

	if (Type != HANDLE_TYPE_TIMER)
		return FALSE;

	if (!lpDueTime)
		return FALSE;

	if (lPeriod < 0)
		return FALSE;

	if (fResume)
	{
		WLog_ERR(TAG, "%s does not support fResume", __FUNCTION__);
		return FALSE;
	}

	timer = (WINPR_TIMER*)Object;
	timer->lPeriod = lPeriod; /* milliseconds */
	timer->pfnCompletionRoutine = pfnCompletionRoutine;
	timer->lpArgToCompletionRoutine = lpArgToCompletionRoutine;

	if (!timer->bInit)
	{
		if (InitializeWaitableTimer(timer) < 0)
			return FALSE;
	}

#if defined(TIMER_IMPL_TIMERFD) || defined(TIMER_IMPL_POSIX)
	ZeroMemory(&(timer->timeout), sizeof(struct itimerspec));

	if (lpDueTime->QuadPart < 0)
	{
		LONGLONG due = lpDueTime->QuadPart * (-1);
		/* due time is in 100 nanosecond intervals */
		seconds = (due / 10000000);
		nanoseconds = ((due % 10000000) * 100);
	}
	else if (lpDueTime->QuadPart == 0)
	{
		seconds = nanoseconds = 0;
	}
	else
	{
		WLog_ERR(TAG, "absolute time not implemented");
		return FALSE;
	}

	if (lPeriod > 0)
	{
		timer->timeout.it_interval.tv_sec = (lPeriod / 1000);              /* seconds */
		timer->timeout.it_interval.tv_nsec = ((lPeriod % 1000) * 1000000); /* nanoseconds */
	}

	if (lpDueTime->QuadPart != 0)
	{
		timer->timeout.it_value.tv_sec = seconds;      /* seconds */
		timer->timeout.it_value.tv_nsec = nanoseconds; /* nanoseconds */
	}
	else
	{
		timer->timeout.it_value.tv_sec = timer->timeout.it_interval.tv_sec;   /* seconds */
		timer->timeout.it_value.tv_nsec = timer->timeout.it_interval.tv_nsec; /* nanoseconds */
	}

#ifdef TIMER_IMPL_TIMERFD
	status = timerfd_settime(timer->fd, 0, &(timer->timeout), NULL);
	if (status)
	{
		WLog_ERR(TAG, "timerfd_settime failure: %d", status);
		return FALSE;
	}
#else
	status = timer_settime(timer->tid, 0, &(timer->timeout), NULL);
	if (status != 0)
	{
		WLog_ERR(TAG, "timer_settime failure");
		return FALSE;
	}
#endif
#endif

#ifdef TIMER_IMPL_DISPATCH
	if (lpDueTime->QuadPart < 0)
	{
		LONGLONG due = lpDueTime->QuadPart * (-1);
		/* due time is in 100 nanosecond intervals */
		seconds = (due / 10000000);
		nanoseconds = due * 100;
	}
	else if (lpDueTime->QuadPart == 0)
	{
		seconds = nanoseconds = 0;
	}
	else
	{
		WLog_ERR(TAG, "absolute time not implemented");
		return FALSE;
	}

	if (!winpr_event_reset(&timer->event))
	{
		WLog_ERR(TAG, "error when resetting timer event");
	}

	{
		if (timer->running)
			dispatch_suspend(timer->source);

		dispatch_time_t start = dispatch_time(DISPATCH_TIME_NOW, nanoseconds);
		uint64_t interval = DISPATCH_TIME_FOREVER;

		if (lPeriod > 0)
			interval = lPeriod * 1000000;

		dispatch_source_set_timer(timer->source, start, interval, 0);
		dispatch_resume(timer->source);
		timer->running = TRUE;
	}
#endif

	if (pfnCompletionRoutine)
	{
		WINPR_APC_ITEM* apcItem = &timer->apcItem;

		/* install our APC routine that will call the completion */
		apcItem->type = APC_TYPE_TIMER;
		apcItem->alwaysSignaled = FALSE;
		apcItem->pollFd = timer->fd;
		apcItem->pollMode = WINPR_FD_READ;
		apcItem->completion = timerAPC;
		apcItem->completionArgs = timer;

		if (!apcItem->linked)
		{
			WINPR_THREAD* thread = winpr_GetCurrentThread();
			if (!thread)
				return FALSE;

			apc_register(thread, apcItem);
		}
	}
	else
	{
		if (timer->apcItem.linked)
		{
			apc_remove(&timer->apcItem);
		}
	}
	return TRUE;
}

BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
                        PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine,
                        PREASON_CONTEXT WakeContext, ULONG TolerableDelay)
{
	return SetWaitableTimer(hTimer, lpDueTime, lPeriod, pfnCompletionRoutine,
	                        lpArgToCompletionRoutine, FALSE);
}

HANDLE OpenWaitableTimerA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpTimerName)
{
	/* TODO: Implement */
	WLog_ERR(TAG, "%s not implemented", __FUNCTION__);
	return NULL;
}

HANDLE OpenWaitableTimerW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpTimerName)
{
	/* TODO: Implement */
	WLog_ERR(TAG, "%s not implemented", __FUNCTION__);
	return NULL;
}

BOOL CancelWaitableTimer(HANDLE hTimer)
{
	ULONG Type;
	WINPR_HANDLE* Object;

	if (!winpr_Handle_GetInfo(hTimer, &Type, &Object))
		return FALSE;

	if (Type != HANDLE_TYPE_TIMER)
		return FALSE;

#if defined(__APPLE__)
	{
		WINPR_TIMER* timer = (WINPR_TIMER*)Object;
		if (timer->running)
			dispatch_suspend(timer->source);

		timer->running = FALSE;
	}
#endif
	return TRUE;
}

/*
 * Returns inner file descriptor for usage with select()
 * This file descriptor is not usable on Windows
 */

int GetTimerFileDescriptor(HANDLE hTimer)
{
#ifndef _WIN32
	WINPR_HANDLE* hdl;
	ULONG type;

	if (!winpr_Handle_GetInfo(hTimer, &type, &hdl) || type != HANDLE_TYPE_TIMER)
	{
		WLog_ERR(TAG, "GetTimerFileDescriptor: hTimer is not an timer");
		SetLastError(ERROR_INVALID_PARAMETER);
		return -1;
	}

	return winpr_Handle_getFd(hTimer);
#else
	return -1;
#endif
}

/**
 * Timer-Queue Timer
 */

/**
 * Design, Performance, and Optimization of Timer Strategies for Real-time ORBs:
 * http://www.cs.wustl.edu/~schmidt/Timer_Queue.html
 */

static void timespec_add_ms(struct timespec* tspec, UINT32 ms)
{
	INT64 ns;
	WINPR_ASSERT(tspec);
	ns = tspec->tv_nsec + (ms * 1000000LL);
	tspec->tv_sec += (ns / 1000000000LL);
	tspec->tv_nsec = (ns % 1000000000LL);
}

static void timespec_gettimeofday(struct timespec* tspec)
{
	struct timeval tval;
	WINPR_ASSERT(tspec);
	gettimeofday(&tval, NULL);
	tspec->tv_sec = tval.tv_sec;
	tspec->tv_nsec = tval.tv_usec * 1000;
}

static INT64 timespec_compare(const struct timespec* tspec1, const struct timespec* tspec2)
{
	WINPR_ASSERT(tspec1);
	WINPR_ASSERT(tspec2);
	if (tspec1->tv_sec == tspec2->tv_sec)
		return (tspec1->tv_nsec - tspec2->tv_nsec);
	else
		return (tspec1->tv_sec - tspec2->tv_sec);
}

static void timespec_copy(struct timespec* dst, struct timespec* src)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);
	dst->tv_sec = src->tv_sec;
	dst->tv_nsec = src->tv_nsec;
}

static void InsertTimerQueueTimer(WINPR_TIMER_QUEUE_TIMER** pHead, WINPR_TIMER_QUEUE_TIMER* timer)
{
	WINPR_TIMER_QUEUE_TIMER* node;

	WINPR_ASSERT(pHead);
	WINPR_ASSERT(timer);

	if (!(*pHead))
	{
		*pHead = timer;
		timer->next = NULL;
		return;
	}

	node = *pHead;

	while (node->next)
	{
		if (timespec_compare(&(timer->ExpirationTime), &(node->ExpirationTime)) > 0)
		{
			if (timespec_compare(&(timer->ExpirationTime), &(node->next->ExpirationTime)) < 0)
				break;
		}

		node = node->next;
	}

	if (node->next)
	{
		timer->next = node->next->next;
		node->next = timer;
	}
	else
	{
		node->next = timer;
		timer->next = NULL;
	}
}

static void RemoveTimerQueueTimer(WINPR_TIMER_QUEUE_TIMER** pHead, WINPR_TIMER_QUEUE_TIMER* timer)
{
	BOOL found = FALSE;
	WINPR_TIMER_QUEUE_TIMER* node;
	WINPR_TIMER_QUEUE_TIMER* prevNode;

	WINPR_ASSERT(pHead);
	WINPR_ASSERT(timer);
	if (timer == *pHead)
	{
		*pHead = timer->next;
		timer->next = NULL;
		return;
	}

	node = *pHead;
	prevNode = NULL;

	while (node)
	{
		if (node == timer)
		{
			found = TRUE;
			break;
		}

		prevNode = node;
		node = node->next;
	}

	if (found)
	{
		if (prevNode)
		{
			prevNode->next = timer->next;
		}

		timer->next = NULL;
	}
}

static int FireExpiredTimerQueueTimers(WINPR_TIMER_QUEUE* timerQueue)
{
	struct timespec CurrentTime;
	WINPR_TIMER_QUEUE_TIMER* node;

	WINPR_ASSERT(timerQueue);

	if (!timerQueue->activeHead)
		return 0;

	timespec_gettimeofday(&CurrentTime);
	node = timerQueue->activeHead;

	while (node)
	{
		if (timespec_compare(&CurrentTime, &(node->ExpirationTime)) >= 0)
		{
			node->Callback(node->Parameter, TRUE);
			node->FireCount++;
			timerQueue->activeHead = node->next;
			node->next = NULL;

			if (node->Period)
			{
				timespec_add_ms(&(node->ExpirationTime), node->Period);
				InsertTimerQueueTimer(&(timerQueue->activeHead), node);
			}
			else
			{
				InsertTimerQueueTimer(&(timerQueue->inactiveHead), node);
			}

			node = timerQueue->activeHead;
		}
		else
		{
			break;
		}
	}

	return 0;
}

static void* TimerQueueThread(void* arg)
{
	int status;
	struct timespec timeout;
	WINPR_TIMER_QUEUE* timerQueue = (WINPR_TIMER_QUEUE*)arg;

	WINPR_ASSERT(timerQueue);
	while (1)
	{
		pthread_mutex_lock(&(timerQueue->cond_mutex));
		timespec_gettimeofday(&timeout);

		if (!timerQueue->activeHead)
		{
			timespec_add_ms(&timeout, 50);
		}
		else
		{
			if (timespec_compare(&timeout, &(timerQueue->activeHead->ExpirationTime)) < 0)
			{
				timespec_copy(&timeout, &(timerQueue->activeHead->ExpirationTime));
			}
		}

		status = pthread_cond_timedwait(&(timerQueue->cond), &(timerQueue->cond_mutex), &timeout);
		FireExpiredTimerQueueTimers(timerQueue);
		pthread_mutex_unlock(&(timerQueue->cond_mutex));

		if ((status != ETIMEDOUT) && (status != 0))
			break;

		if (timerQueue->bCancelled)
			break;
	}

	return NULL;
}

static int StartTimerQueueThread(WINPR_TIMER_QUEUE* timerQueue)
{
	WINPR_ASSERT(timerQueue);
	pthread_cond_init(&(timerQueue->cond), NULL);
	pthread_mutex_init(&(timerQueue->cond_mutex), NULL);
	pthread_mutex_init(&(timerQueue->mutex), NULL);
	pthread_attr_init(&(timerQueue->attr));
	timerQueue->param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_attr_setschedparam(&(timerQueue->attr), &(timerQueue->param));
	pthread_attr_setschedpolicy(&(timerQueue->attr), SCHED_FIFO);
	pthread_create(&(timerQueue->thread), &(timerQueue->attr), TimerQueueThread, timerQueue);
	return 0;
}

HANDLE CreateTimerQueue(void)
{
	HANDLE handle = NULL;
	WINPR_TIMER_QUEUE* timerQueue;
	timerQueue = (WINPR_TIMER_QUEUE*)calloc(1, sizeof(WINPR_TIMER_QUEUE));

	if (timerQueue)
	{
		WINPR_HANDLE_SET_TYPE_AND_MODE(timerQueue, HANDLE_TYPE_TIMER_QUEUE, WINPR_FD_READ);
		handle = (HANDLE)timerQueue;
		timerQueue->activeHead = NULL;
		timerQueue->inactiveHead = NULL;
		timerQueue->bCancelled = FALSE;
		StartTimerQueueThread(timerQueue);
	}

	return handle;
}

BOOL DeleteTimerQueueEx(HANDLE TimerQueue, HANDLE CompletionEvent)
{
	void* rvalue;
	WINPR_TIMER_QUEUE* timerQueue;
	WINPR_TIMER_QUEUE_TIMER* node;
	WINPR_TIMER_QUEUE_TIMER* nextNode;

	if (!TimerQueue)
		return FALSE;

	timerQueue = (WINPR_TIMER_QUEUE*)TimerQueue;
	/* Cancel and delete timer queue timers */
	pthread_mutex_lock(&(timerQueue->cond_mutex));
	timerQueue->bCancelled = TRUE;
	pthread_cond_signal(&(timerQueue->cond));
	pthread_mutex_unlock(&(timerQueue->cond_mutex));
	pthread_join(timerQueue->thread, &rvalue);
	/**
	 * Quote from MSDN regarding CompletionEvent:
	 * If this parameter is INVALID_HANDLE_VALUE, the function waits for
	 * all callback functions to complete before returning.
	 * If this parameter is NULL, the function marks the timer for
	 * deletion and returns immediately.
	 *
	 * Note: The current WinPR implementation implicitly waits for any
	 * callback functions to complete (see pthread_join above)
	 */
	{
		/* Move all active timers to the inactive timer list */
		node = timerQueue->activeHead;

		while (node)
		{
			InsertTimerQueueTimer(&(timerQueue->inactiveHead), node);
			node = node->next;
		}

		timerQueue->activeHead = NULL;
		/* Once all timers are inactive, free them */
		node = timerQueue->inactiveHead;

		while (node)
		{
			nextNode = node->next;
			free(node);
			node = nextNode;
		}

		timerQueue->inactiveHead = NULL;
	}
	/* Delete timer queue */
	pthread_cond_destroy(&(timerQueue->cond));
	pthread_mutex_destroy(&(timerQueue->cond_mutex));
	pthread_mutex_destroy(&(timerQueue->mutex));
	pthread_attr_destroy(&(timerQueue->attr));
	free(timerQueue);

	if (CompletionEvent && (CompletionEvent != INVALID_HANDLE_VALUE))
		SetEvent(CompletionEvent);

	return TRUE;
}

BOOL DeleteTimerQueue(HANDLE TimerQueue)
{
	return DeleteTimerQueueEx(TimerQueue, NULL);
}

BOOL CreateTimerQueueTimer(PHANDLE phNewTimer, HANDLE TimerQueue, WAITORTIMERCALLBACK Callback,
                           PVOID Parameter, DWORD DueTime, DWORD Period, ULONG Flags)
{
	struct timespec CurrentTime;
	WINPR_TIMER_QUEUE* timerQueue;
	WINPR_TIMER_QUEUE_TIMER* timer;

	if (!TimerQueue)
		return FALSE;

	timespec_gettimeofday(&CurrentTime);
	timerQueue = (WINPR_TIMER_QUEUE*)TimerQueue;
	timer = (WINPR_TIMER_QUEUE_TIMER*)malloc(sizeof(WINPR_TIMER_QUEUE_TIMER));

	if (!timer)
		return FALSE;

	WINPR_HANDLE_SET_TYPE_AND_MODE(timer, HANDLE_TYPE_TIMER_QUEUE_TIMER, WINPR_FD_READ);
	*((UINT_PTR*)phNewTimer) = (UINT_PTR)(HANDLE)timer;
	timespec_copy(&(timer->StartTime), &CurrentTime);
	timespec_add_ms(&(timer->StartTime), DueTime);
	timespec_copy(&(timer->ExpirationTime), &(timer->StartTime));
	timer->Flags = Flags;
	timer->DueTime = DueTime;
	timer->Period = Period;
	timer->Callback = Callback;
	timer->Parameter = Parameter;
	timer->timerQueue = (WINPR_TIMER_QUEUE*)TimerQueue;
	timer->FireCount = 0;
	timer->next = NULL;
	pthread_mutex_lock(&(timerQueue->cond_mutex));
	InsertTimerQueueTimer(&(timerQueue->activeHead), timer);
	pthread_cond_signal(&(timerQueue->cond));
	pthread_mutex_unlock(&(timerQueue->cond_mutex));
	return TRUE;
}

BOOL ChangeTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, ULONG DueTime, ULONG Period)
{
	struct timespec CurrentTime;
	WINPR_TIMER_QUEUE* timerQueue;
	WINPR_TIMER_QUEUE_TIMER* timer;

	if (!TimerQueue || !Timer)
		return FALSE;

	timespec_gettimeofday(&CurrentTime);
	timerQueue = (WINPR_TIMER_QUEUE*)TimerQueue;
	timer = (WINPR_TIMER_QUEUE_TIMER*)Timer;
	pthread_mutex_lock(&(timerQueue->cond_mutex));
	RemoveTimerQueueTimer(&(timerQueue->activeHead), timer);
	RemoveTimerQueueTimer(&(timerQueue->inactiveHead), timer);
	timer->DueTime = DueTime;
	timer->Period = Period;
	timer->next = NULL;
	timespec_copy(&(timer->StartTime), &CurrentTime);
	timespec_add_ms(&(timer->StartTime), DueTime);
	timespec_copy(&(timer->ExpirationTime), &(timer->StartTime));
	InsertTimerQueueTimer(&(timerQueue->activeHead), timer);
	pthread_cond_signal(&(timerQueue->cond));
	pthread_mutex_unlock(&(timerQueue->cond_mutex));
	return TRUE;
}

BOOL DeleteTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, HANDLE CompletionEvent)
{
	WINPR_TIMER_QUEUE* timerQueue;
	WINPR_TIMER_QUEUE_TIMER* timer;

	if (!TimerQueue || !Timer)
		return FALSE;

	timerQueue = (WINPR_TIMER_QUEUE*)TimerQueue;
	timer = (WINPR_TIMER_QUEUE_TIMER*)Timer;
	pthread_mutex_lock(&(timerQueue->cond_mutex));
	/**
	 * Quote from MSDN regarding CompletionEvent:
	 * If this parameter is INVALID_HANDLE_VALUE, the function waits for
	 * all callback functions to complete before returning.
	 * If this parameter is NULL, the function marks the timer for
	 * deletion and returns immediately.
	 *
	 * Note: The current WinPR implementation implicitly waits for any
	 * callback functions to complete (see cond_mutex usage)
	 */
	RemoveTimerQueueTimer(&(timerQueue->activeHead), timer);
	pthread_cond_signal(&(timerQueue->cond));
	pthread_mutex_unlock(&(timerQueue->cond_mutex));
	free(timer);

	if (CompletionEvent && (CompletionEvent != INVALID_HANDLE_VALUE))
		SetEvent(CompletionEvent);

	return TRUE;
}

#endif
