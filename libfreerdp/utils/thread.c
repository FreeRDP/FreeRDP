/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Thread Utils
 *
 * Copyright 2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <winpr/crt.h>
#include <winpr/windows.h>

#ifdef _WIN32
#ifdef _MSC_VER
#include <process.h>
#endif
#endif

#include <freerdp/utils/sleep.h>
#include <freerdp/utils/thread.h>

freerdp_thread* freerdp_thread_new(void)
{
	freerdp_thread* thread;

	thread = (freerdp_thread*) malloc(sizeof(freerdp_thread));
	ZeroMemory(thread, sizeof(freerdp_thread));

	thread->mutex = CreateMutex(NULL, FALSE, NULL);
	thread->signals[0] = CreateEvent(NULL, TRUE, FALSE, NULL);
	thread->signals[1] = CreateEvent(NULL, TRUE, FALSE, NULL);
	thread->num_signals = 2;

	return thread;
}

void freerdp_thread_start(freerdp_thread* thread, void* func, void* arg)
{
	thread->status = 1;

#ifdef _WIN32
	{
		CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) func, arg, 0, NULL));
	}
#else
	{
		pthread_t th;
		pthread_create(&th, 0, func, arg);
		pthread_detach(th);
	}
#endif
}

void freerdp_thread_stop(freerdp_thread* thread)
{
	int i = 0;

	SetEvent(thread->signals[0]);

	while ((thread->status > 0) && (i < 1000))
	{
		i++;
		freerdp_usleep(100000);
	}
}

void freerdp_thread_free(freerdp_thread* thread)
{
	int i;

	for (i = 0; i < thread->num_signals; i++)
		CloseHandle(thread->signals[i]);

	thread->num_signals = 0;

	CloseHandle(thread->mutex);
	thread->mutex = NULL;

	free(thread);
}
