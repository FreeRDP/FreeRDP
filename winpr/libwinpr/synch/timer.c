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

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/sysinfo.h>

#include <winpr/synch.h>

#ifndef _WIN32
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#endif

#include "synch.h"

#ifndef _WIN32

#include "../handle/handle.h"

#ifdef WITH_POSIX_TIMER

static BOOL g_WaitableTimerSignalHandlerInstalled = FALSE;

void WaitableTimerSignalHandler(int signum, siginfo_t* siginfo, void* arg)
{
	WINPR_TIMER* timer = siginfo->si_value.sival_ptr;

	if (!timer || (signum != SIGALRM))
		return;

	if (timer->pfnCompletionRoutine)
	{
		timer->pfnCompletionRoutine(timer->lpArgToCompletionRoutine, 0, 0);

		if (timer->lPeriod)
		{
			timer->timeout.it_interval.tv_sec = (timer->lPeriod / 1000); /* seconds */
			timer->timeout.it_interval.tv_nsec = ((timer->lPeriod % 1000) * 1000000); /* nanoseconds */

			if ((timer_settime(timer->tid, 0, &(timer->timeout), NULL)) != 0)
			{
				perror("timer_settime");
			}
		}
	}
}

int InstallWaitableTimerSignalHandler()
{
	if (!g_WaitableTimerSignalHandlerInstalled)
	{
		struct sigaction action;

		sigemptyset(&action.sa_mask);
		sigaddset(&action.sa_mask, SIGALRM);

		action.sa_flags = SA_RESTART | SA_SIGINFO;
		action.sa_sigaction = (void*) &WaitableTimerSignalHandler;

		sigaction(SIGALRM, &action, NULL);

		g_WaitableTimerSignalHandlerInstalled = TRUE;
	}

	return 0;
}

#endif

int InitializeWaitableTimer(WINPR_TIMER* timer)
{
	if (!timer->lpArgToCompletionRoutine)
	{
#ifdef HAVE_TIMERFD_H
		int status;
		
		timer->fd = timerfd_create(CLOCK_MONOTONIC, 0);

		if (timer->fd <= 0)
		{
			free(timer);
			return -1;
		}

		status = fcntl(timer->fd, F_SETFL, O_NONBLOCK);

		if (status)
		{
			close(timer->fd);
			return -1;
		}
#endif
	}
	else
	{
#ifdef WITH_POSIX_TIMER
		struct sigevent sigev;

		InstallWaitableTimerSignalHandler();

		ZeroMemory(&sigev, sizeof(struct sigevent));

		sigev.sigev_notify = SIGEV_SIGNAL;
		sigev.sigev_signo = SIGALRM;
		sigev.sigev_value.sival_ptr = (void*) timer;

		if ((timer_create(CLOCK_MONOTONIC, &sigev, &(timer->tid))) != 0)
		{
			perror("timer_create");
			return -1;
		}
#endif
	}

	timer->bInit = TRUE;

	return 0;
}

/**
 * Waitable Timer
 */

HANDLE CreateWaitableTimerA(LPSECURITY_ATTRIBUTES lpTimerAttributes, BOOL bManualReset, LPCSTR lpTimerName)
{
	HANDLE handle = NULL;
	WINPR_TIMER* timer;

	timer = (WINPR_TIMER*) malloc(sizeof(WINPR_TIMER));

	if (timer)
	{
		WINPR_HANDLE_SET_TYPE(timer, HANDLE_TYPE_TIMER);
		handle = (HANDLE) timer;

		timer->fd = -1;
		timer->lPeriod = 0;
		timer->bManualReset = bManualReset;
		timer->pfnCompletionRoutine = NULL;
		timer->lpArgToCompletionRoutine = NULL;
		timer->bInit = FALSE;
	}

	return handle;
}

HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpTimerAttributes, BOOL bManualReset, LPCWSTR lpTimerName)
{
	return NULL;
}

HANDLE CreateWaitableTimerExA(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	BOOL bManualReset;

	bManualReset = (dwFlags & CREATE_WAITABLE_TIMER_MANUAL_RESET) ? TRUE : FALSE;

	return CreateWaitableTimerA(lpTimerAttributes, bManualReset, lpTimerName);
}

HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	return NULL;
}

BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, BOOL fResume)
{
	ULONG Type;
	PVOID Object;
	int status = 0;
	WINPR_TIMER* timer;
	LONGLONG seconds = 0;
	LONGLONG nanoseconds = 0;

	if (!winpr_Handle_GetInfo(hTimer, &Type, &Object))
		return FALSE;

	if (Type != HANDLE_TYPE_TIMER)
		return FALSE;

	if (!lpDueTime)
		return FALSE;

	if (lPeriod < 0)
		return FALSE;

	timer = (WINPR_TIMER*) Object;

	timer->lPeriod = lPeriod; /* milliseconds */
	timer->pfnCompletionRoutine = pfnCompletionRoutine;
	timer->lpArgToCompletionRoutine = lpArgToCompletionRoutine;

	if (!timer->bInit)
	{
		if (InitializeWaitableTimer(timer) < 0)
			return FALSE;
	}

#ifdef WITH_POSIX_TIMER
	
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
		printf("SetWaitableTimer: implement absolute time\n");
		return FALSE;
	}

	if (lPeriod > 0)
	{
		timer->timeout.it_interval.tv_sec = (lPeriod / 1000); /* seconds */
		timer->timeout.it_interval.tv_nsec = ((lPeriod % 1000) * 1000000); /* nanoseconds */
	}

	if (lpDueTime->QuadPart != 0)
	{
		timer->timeout.it_value.tv_sec = seconds; /* seconds */
		timer->timeout.it_value.tv_nsec = nanoseconds; /* nanoseconds */
	}
	else
	{
		timer->timeout.it_value.tv_sec = timer->timeout.it_interval.tv_sec; /* seconds */
		timer->timeout.it_value.tv_nsec = timer->timeout.it_interval.tv_nsec; /* nanoseconds */
	}

	if (!timer->pfnCompletionRoutine)
	{
#ifdef HAVE_TIMERFD_H
		status = timerfd_settime(timer->fd, 0, &(timer->timeout), NULL);

		if (status)
		{
			printf("SetWaitableTimer timerfd_settime failure: %d\n", status);
			return FALSE;
		}
#endif
	}
	else
	{
		if ((timer_settime(timer->tid, 0, &(timer->timeout), NULL)) != 0)
		{
			perror("timer_settime");
			return FALSE;
		}
	}

#endif

	return TRUE;
}

BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, PREASON_CONTEXT WakeContext, ULONG TolerableDelay)
{
	ULONG Type;
	PVOID Object;
	WINPR_TIMER* timer;

	if (!winpr_Handle_GetInfo(hTimer, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_TIMER)
	{
		timer = (WINPR_TIMER*) Object;
		return TRUE;
	}

	return TRUE;
}

HANDLE OpenWaitableTimerA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpTimerName)
{
	return NULL;
}

HANDLE OpenWaitableTimerW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpTimerName)
{
	return NULL;
}

BOOL CancelWaitableTimer(HANDLE hTimer)
{
	return TRUE;
}

/**
 * Timer-Queue Timer
 */

/**
 * Design, Performance, and Optimization of Timer Strategies for Real-time ORBs:
 * http://www.cs.wustl.edu/~schmidt/Timer_Queue.html
 */

void timespec_add_ms(struct timespec* tspec, UINT32 ms)
{
	UINT64 ns = tspec->tv_nsec + (ms * 1000000);
	tspec->tv_sec += (ns / 1000000000);
	tspec->tv_nsec = (ns % 1000000000);
}

UINT64 timespec_to_ms(struct timespec* tspec)
{
	UINT64 ms;
	ms = tspec->tv_sec * 1000;
	ms += tspec->tv_nsec / 1000000;
	return ms;
}

static void timespec_gettimeofday(struct timespec* tspec)
{
	struct timeval tval;
	gettimeofday(&tval, NULL);
	tspec->tv_sec = tval.tv_sec;
	tspec->tv_nsec = tval.tv_usec * 1000;
}

static int timespec_compare(const struct timespec* tspec1, const struct timespec* tspec2)
{
	if (tspec1->tv_sec == tspec2->tv_sec)
		return (tspec1->tv_nsec - tspec2->tv_nsec);
	else
		return (tspec1->tv_sec - tspec2->tv_sec);
}

static void timespec_copy(struct timespec* dst, struct timespec* src)
{
	dst->tv_sec = src->tv_sec;
	dst->tv_nsec = src->tv_nsec;
}

void InsertTimerQueueTimer(WINPR_TIMER_QUEUE_TIMER** pHead, WINPR_TIMER_QUEUE_TIMER* timer)
{
	WINPR_TIMER_QUEUE_TIMER* node;
	
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

void RemoveTimerQueueTimer(WINPR_TIMER_QUEUE_TIMER** pHead, WINPR_TIMER_QUEUE_TIMER* timer)
{
	BOOL found = FALSE;
	WINPR_TIMER_QUEUE_TIMER* node;
	WINPR_TIMER_QUEUE_TIMER* prevNode;

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

int FireExpiredTimerQueueTimers(WINPR_TIMER_QUEUE* timerQueue)
{
	struct timespec CurrentTime;
	WINPR_TIMER_QUEUE_TIMER* node;

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
	WINPR_TIMER_QUEUE* timerQueue = (WINPR_TIMER_QUEUE*) arg;

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

		if (timerQueue->bCancelled)
			break;
	}

	return NULL;
}

int StartTimerQueueThread(WINPR_TIMER_QUEUE* timerQueue)
{
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

	timerQueue = (WINPR_TIMER_QUEUE*) malloc(sizeof(WINPR_TIMER_QUEUE));

	if (timerQueue)
	{
		ZeroMemory(timerQueue, sizeof(WINPR_TIMER_QUEUE));
		
		WINPR_HANDLE_SET_TYPE(timerQueue, HANDLE_TYPE_TIMER_QUEUE);
		handle = (HANDLE) timerQueue;

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

	timerQueue = (WINPR_TIMER_QUEUE*) TimerQueue;

	/* Cancel and delete timer queue timers */

	pthread_mutex_lock(&(timerQueue->cond_mutex));

	timerQueue->bCancelled = TRUE;

	pthread_cond_signal(&(timerQueue->cond));
	pthread_mutex_unlock(&(timerQueue->cond_mutex));

	pthread_join(timerQueue->thread, &rvalue);

	if (CompletionEvent == INVALID_HANDLE_VALUE)
	{
		/* Wait for all callback functions to complete before returning */
	}
	else
	{
		/* Cancel all timers and return immediately */

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

BOOL CreateTimerQueueTimer(PHANDLE phNewTimer, HANDLE TimerQueue,
		WAITORTIMERCALLBACK Callback, PVOID Parameter, DWORD DueTime, DWORD Period, ULONG Flags)
{
	struct timespec CurrentTime;
	WINPR_TIMER_QUEUE* timerQueue;
	WINPR_TIMER_QUEUE_TIMER* timer;

	if (!TimerQueue)
		return FALSE;

	timespec_gettimeofday(&CurrentTime);

	timerQueue = (WINPR_TIMER_QUEUE*) TimerQueue;
	timer = (WINPR_TIMER_QUEUE_TIMER*) malloc(sizeof(WINPR_TIMER_QUEUE_TIMER));

	if (!timer)
		return FALSE;

	WINPR_HANDLE_SET_TYPE(timer, HANDLE_TYPE_TIMER_QUEUE_TIMER);
	*((UINT_PTR*) phNewTimer) = (UINT_PTR) (HANDLE) timer;

	timespec_copy(&(timer->StartTime), &CurrentTime);
	timespec_add_ms(&(timer->StartTime), DueTime);
	timespec_copy(&(timer->ExpirationTime), &(timer->StartTime));

	timer->Flags = Flags;
	timer->DueTime = DueTime;
	timer->Period = Period;
	timer->Callback = Callback;
	timer->Parameter = Parameter;
	
	timer->timerQueue = (WINPR_TIMER_QUEUE*) TimerQueue;

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

	timerQueue = (WINPR_TIMER_QUEUE*) TimerQueue;
	timer = (WINPR_TIMER_QUEUE_TIMER*) Timer;

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

	timerQueue = (WINPR_TIMER_QUEUE*) TimerQueue;
	timer = (WINPR_TIMER_QUEUE_TIMER*) Timer;

	pthread_mutex_lock(&(timerQueue->cond_mutex));

	if (CompletionEvent == INVALID_HANDLE_VALUE)
	{
		/* Wait for all callback functions to complete before returning */
	}
	else
	{
		/* Cancel timer and return immediately */

		RemoveTimerQueueTimer(&(timerQueue->activeHead), timer);
	}

	pthread_cond_signal(&(timerQueue->cond));
	pthread_mutex_unlock(&(timerQueue->cond_mutex));

	free(timer);

	if (CompletionEvent && (CompletionEvent != INVALID_HANDLE_VALUE))
		SetEvent(CompletionEvent);

	return TRUE;
}

#endif
