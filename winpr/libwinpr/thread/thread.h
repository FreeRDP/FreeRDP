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

typedef struct
{
	WINPR_ALIGN64 pthread_mutex_t mux;
	WINPR_ALIGN64 pthread_cond_t cond;
	WINPR_ALIGN64 BOOL val;
} mux_condition_bundle;

struct winpr_thread
{
	WINPR_HANDLE common;

	WINPR_ALIGN64 BOOL started;
	WINPR_ALIGN64 WINPR_EVENT_IMPL event;
	WINPR_ALIGN64 BOOL mainProcess;
	WINPR_ALIGN64 BOOL detached;
	WINPR_ALIGN64 BOOL joined;
	WINPR_ALIGN64 BOOL exited;
	WINPR_ALIGN64 DWORD dwExitCode;
	WINPR_ALIGN64 pthread_t thread;
	WINPR_ALIGN64 size_t dwStackSize;
	WINPR_ALIGN64 LPVOID lpParameter;
	WINPR_ALIGN64 pthread_mutex_t mutex;
	mux_condition_bundle isRunning;
	mux_condition_bundle isCreated;
	WINPR_ALIGN64 LPTHREAD_START_ROUTINE lpStartAddress;
	WINPR_ALIGN64 LPSECURITY_ATTRIBUTES lpThreadAttributes;
	WINPR_ALIGN64 APC_QUEUE apc;
#if defined(WITH_DEBUG_THREADS)
	WINPR_ALIGN64 void* create_stack;
	WINPR_ALIGN64 void* exit_stack;
#endif
};

WINPR_THREAD* winpr_GetCurrentThread(VOID);

typedef struct
{
	WINPR_HANDLE common;

	pid_t pid;
	int status;
	DWORD dwExitCode;
	int fd;
} WINPR_PROCESS;

#endif

#endif /* WINPR_THREAD_PRIVATE_H */
