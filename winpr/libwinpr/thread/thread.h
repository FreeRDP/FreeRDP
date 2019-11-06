/**
 * WinPR: Windows Portable Runtime
 * Process Thread Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Hewlett-Packard Development Company, L.P.
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

typedef void* (*pthread_start_routine)(void*);

struct winpr_thread
{
	WINPR_HANDLE_DEF();

	BOOL started;
	int pipe_fd[2];
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
	LPTHREAD_START_ROUTINE lpStartAddress;
	LPSECURITY_ATTRIBUTES lpThreadAttributes;
#if defined(WITH_DEBUG_THREADS)
	void* create_stack;
	void* exit_stack;
#endif
};
typedef struct winpr_thread WINPR_THREAD;

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
