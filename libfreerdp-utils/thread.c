/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#ifdef _MSC_VER
#include <process.h>
#endif
#endif

#include <freerdp/utils/sleep.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>

freerdp_thread* freerdp_thread_new(void)
{
	freerdp_thread* thread;

	thread = xnew(freerdp_thread);
	thread->mutex = freerdp_mutex_new();
	thread->signals[0] = wait_obj_new();
	thread->signals[1] = wait_obj_new();
	thread->num_signals = 2;

	return thread;
}

void freerdp_thread_start(freerdp_thread* thread, void* func, void* arg)
{
	thread->status = 1;

#ifdef _WIN32
	{
#	ifdef _MSC_VER
		CloseHandle((HANDLE)_beginthreadex(NULL, 0, func, arg, 0, NULL));
#else
		CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL));
#endif
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

	wait_obj_set(thread->signals[0]);

	while (thread->status > 0 && i < 1000)
	{
		i++;
		freerdp_usleep(100000);
	}
}

void freerdp_thread_free(freerdp_thread* thread)
{
	int i;

	for (i = 0; i < thread->num_signals; i++)
		wait_obj_free(thread->signals[i]);
	thread->num_signals = 0;

	freerdp_mutex_free(thread->mutex);
	thread->mutex = NULL;

	xfree(thread);
}
