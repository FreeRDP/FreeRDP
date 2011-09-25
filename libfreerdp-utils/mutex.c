/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Mutex Utils
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

#include <freerdp/utils/memory.h>
#include <freerdp/utils/mutex.h>

#ifdef _WIN32
#include <windows.h>
#define freerdp_mutex_t HANDLE
#else
#include <pthread.h>
#define freerdp_mutex_t pthread_mutex_t
#endif

freerdp_mutex freerdp_mutex_new(void)
{
#ifdef _WIN32
	freerdp_mutex_t mutex;
	mutex = CreateMutex(NULL, FALSE, NULL);
	return (freerdp_mutex) mutex;
#else
	freerdp_mutex_t* mutex;
	mutex = xnew(freerdp_mutex_t);
	pthread_mutex_init(mutex, 0);
	return mutex;
#endif
}

void freerdp_mutex_free(freerdp_mutex mutex)
{
#ifdef _WIN32
	CloseHandle((freerdp_mutex_t) mutex);
#else
	pthread_mutex_destroy((freerdp_mutex_t*) mutex);
	xfree(mutex);
#endif
}

void freerdp_mutex_lock(freerdp_mutex mutex)
{
#ifdef _WIN32
	WaitForSingleObject((freerdp_mutex_t) mutex, INFINITE);
#else
	pthread_mutex_lock(mutex);
#endif
}

void freerdp_mutex_unlock(freerdp_mutex mutex)
{
#ifdef _WIN32
	ReleaseMutex((freerdp_mutex_t) mutex);
#else
	pthread_mutex_unlock(mutex);
#endif
}
