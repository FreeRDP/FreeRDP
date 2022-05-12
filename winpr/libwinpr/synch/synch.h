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

#ifndef WINPR_SYNCH_PRIVATE_H
#define WINPR_SYNCH_PRIVATE_H

#include <winpr/config.h>

#include <winpr/platform.h>

#include <winpr/synch.h>

#include "../handle/handle.h"
#include "../thread/apc.h"
#include "event.h"

#ifndef _WIN32

#define WINPR_PIPE_SEMAPHORE 1

#if defined __APPLE__
#include <pthread.h>
#include <sys/time.h>
#include <semaphore.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#define winpr_sem_t semaphore_t
#else
#include <pthread.h>
#include <semaphore.h>
#define winpr_sem_t sem_t
#endif

struct winpr_mutex
{
	WINPR_HANDLE common;
	char* name;
	pthread_mutex_t mutex;
};
typedef struct winpr_mutex WINPR_MUTEX;

struct winpr_semaphore
{
	WINPR_HANDLE common;

	int pipe_fd[2];
	winpr_sem_t* sem;
};
typedef struct winpr_semaphore WINPR_SEMAPHORE;

#ifdef HAVE_SYS_TIMERFD_H
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#define TIMER_IMPL_TIMERFD

#elif defined(WITH_POSIX_TIMER)
#include <fcntl.h>
#define TIMER_IMPL_POSIX

#elif defined(__APPLE__)
#define TIMER_IMPL_DISPATCH
#include <dispatch/dispatch.h>
#else
#error missing timer implementation
#endif

struct winpr_timer
{
	WINPR_HANDLE common;

	int fd;
	BOOL bInit;
	LONG lPeriod;
	BOOL bManualReset;
	PTIMERAPCROUTINE pfnCompletionRoutine;
	LPVOID lpArgToCompletionRoutine;

#ifdef TIMER_IMPL_TIMERFD
	struct itimerspec timeout;
#endif

#ifdef TIMER_IMPL_POSIX
	WINPR_EVENT_IMPL event;
	timer_t tid;
	struct itimerspec timeout;
#endif

#ifdef TIMER_IMPL_DISPATCH
	WINPR_EVENT_IMPL event;
	dispatch_queue_t queue;
	dispatch_source_t source;
	BOOL running;
#endif
	char* name;

	WINPR_APC_ITEM apcItem;
};
typedef struct winpr_timer WINPR_TIMER;

typedef struct winpr_timer_queue_timer WINPR_TIMER_QUEUE_TIMER;

struct winpr_timer_queue
{
	WINPR_HANDLE common;

	pthread_t thread;
	pthread_attr_t attr;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_mutex_t cond_mutex;
	struct sched_param param;

	BOOL bCancelled;
	WINPR_TIMER_QUEUE_TIMER* activeHead;
	WINPR_TIMER_QUEUE_TIMER* inactiveHead;
};
typedef struct winpr_timer_queue WINPR_TIMER_QUEUE;

struct winpr_timer_queue_timer
{
	WINPR_HANDLE common;

	ULONG Flags;
	DWORD DueTime;
	DWORD Period;
	PVOID Parameter;
	WAITORTIMERCALLBACK Callback;

	int FireCount;

	struct timespec StartTime;
	struct timespec ExpirationTime;

	WINPR_TIMER_QUEUE* timerQueue;
	WINPR_TIMER_QUEUE_TIMER* next;
};

#endif

#endif /* WINPR_SYNCH_PRIVATE_H */
