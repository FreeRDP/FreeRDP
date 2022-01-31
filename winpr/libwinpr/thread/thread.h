/**
 * WinPR: Windows Portable Runtime
 * Process Thread Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Hewlett-Packard Development Company, L.P.
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

#ifndef WINPR_THREAD_PRIVATE_H
#define WINPR_THREAD_PRIVATE_H

#ifndef _WIN32

#include <pthread.h>

#include <winpr/thread.h>

#include "../handle/handle.h"
#include "../synch/event.h"
#include "apc.h"

typedef void* (*pthread_start_routine)(void*);
typedef struct winpr_APC_item WINPR_APC_ITEM;

struct winpr_thread
{
	WINPR_HANDLE_DEF();

	BOOL started;
	WINPR_EVENT_IMPL event;
	BOOL mainProcess;
	BOOL detached;
	BOOL joined;
	BOOL exited;
	DWORD dwExitCode;
	pthread_t thread;
	SIZE_T dwStackSize;
	LPVOID lpParameter;
	pthread_mutex_t mutex;
	pthread_mutex_t threadIsReadyMutex;
	pthread_cond_t threadIsReady;
	pthread_mutex_t threadReadyMutex;
	pthread_cond_t threadReady;
	LPTHREAD_START_ROUTINE lpStartAddress;
	LPSECURITY_ATTRIBUTES lpThreadAttributes;
	APC_QUEUE apc;
#if defined(WITH_DEBUG_THREADS)
	void* create_stack;
	void* exit_stack;
#endif
};
typedef struct winpr_thread WINPR_THREAD;

WINPR_THREAD* winpr_GetCurrentThread(VOID);

struct winpr_process
{
	WINPR_HANDLE_DEF();

	pid_t pid;
	int status;
	DWORD dwExitCode;
};
typedef struct winpr_process WINPR_PROCESS;

#endif

#endif /* WINPR_THREAD_PRIVATE_H */
