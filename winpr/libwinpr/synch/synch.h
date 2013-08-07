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

#ifndef _WIN32

#include "../handle/handle.h"

#define WINPR_PIPE_SEMAPHORE	1

#if defined __APPLE__
#include <pthread.h>
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
	LONG lPeriod;
	BOOL bManualReset;
	PTIMERAPCROUTINE pfnCompletionRoutine;
	LPVOID lpArgToCompletionRoutine;

#ifdef HAVE_TIMERFD_H
	struct itimerspec timeout;
#endif
};
typedef struct winpr_timer WINPR_TIMER;

#endif

#endif /* WINPR_SYNCH_PRIVATE_H */
