/**
 * WinPR: Windows Portable Runtime
 * Process Thread Functions
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

#ifndef WINPR_THREAD_PRIVATE_H
#define WINPR_THREAD_PRIVATE_H

#ifndef _WIN32

#include <pthread.h>

#include <winpr/thread.h>

#include "../handle/handle.h"

typedef void *(*pthread_start_routine)(void*);

struct winpr_thread
{
	WINPR_HANDLE_DEF();

	BOOL started;
	DWORD dwExitCode;
	pthread_t thread;
	SIZE_T dwStackSize;
	LPVOID lpParameter;
	pthread_mutex_t mutex;
	LPTHREAD_START_ROUTINE lpStartAddress;
	LPSECURITY_ATTRIBUTES lpThreadAttributes;
};
typedef struct winpr_thread WINPR_THREAD;

#endif

#endif /* WINPR_THREAD_PRIVATE_H */

