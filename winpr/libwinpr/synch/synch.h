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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/platform.h>

#include <winpr/synch.h>

#ifdef __linux__
#define WITH_POSIX_TIMER	1
#endif

#include "../handle/handle.h"

#ifndef _WIN32

#define WINPR_PIPE_SEMAPHORE	1

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
	WINPR_HANDLE_DEF();

	pthread_mutex_t mutex;
};
typedef struct winpr_mutex WINPR_MUTEX;

struct winpr_semaphore
{
	WINPR_HANDLE_DEF();

	int pipe_fd[2];
	winpr_sem_t* sem;
};
typedef struct winpr_semaphore WINPR_SEMAPHORE;

struct winpr_event
{
	WINPR_HANDLE_DEF();

	int pipe_fd[2];
	BOOL bAttached;
	BOOL bManualReset;
};
typedef struct winpr_event WINPR_EVENT;

#ifdef HAVE_TIMERFD_H
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#endif

struct winpr_timer
{
	WINPR_HANDLE_DEF();

	int fd;
	BOOL bInit;
	LONG lPeriod;
	BOOL bManualReset;
	PTIMERAPCROUTINE pfnCompletionRoutine;
	LPVOID lpArgToCompletionRoutine;
	
#ifdef WITH_POSIX_TIMER
	timer_t tid;
	struct itimerspec timeout;
#endif
};
typedef struct winpr_timer WINPR_TIMER;

typedef struct winpr_timer_queue_timer WINPR_TIMER_QUEUE_TIMER;

struct winpr_timer_queue
{
	WINPR_HANDLE_DEF();
	
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
	WINPR_HANDLE_DEF();

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
