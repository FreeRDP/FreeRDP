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

#ifndef FREERDP_UTILS_THREAD_H
#define FREERDP_UTILS_THREAD_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifndef _WIN32
#include <pthread.h>
#endif

#include <winpr/synch.h>

typedef struct _freerdp_thread freerdp_thread;

struct _freerdp_thread
{
	HANDLE mutex;

	HANDLE signals[5];
	int num_signals;

	int status;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API freerdp_thread* freerdp_thread_new(void);
FREERDP_API void freerdp_thread_start(freerdp_thread* thread, void* func, void* arg);
FREERDP_API void freerdp_thread_stop(freerdp_thread* thread);
FREERDP_API void freerdp_thread_free(freerdp_thread* thread);

#ifdef __cplusplus
}
#endif

#define freerdp_thread_wait(_t) ((WaitForMultipleObjects(_t->num_signals, _t->signals, FALSE, INFINITE) == WAIT_FAILED) ? -1 : 0)
#define freerdp_thread_wait_timeout(_t, _timeout) ((WaitForMultipleObjects(_t->num_signals, _t->signals, FALSE, _timeout) == WAIT_FAILED) ? -1 : 0)
#define freerdp_thread_is_stopped(_t) (WaitForSingleObject(_t->signals[0], 0) == WAIT_OBJECT_0)
#define freerdp_thread_is_running(_t) (_t->status == 1)
#define freerdp_thread_quit(_t) do { \
	_t->status = -1; \
	ResetEvent(_t->signals[0]); } while (0)
#define freerdp_thread_signal(_t) SetEvent(_t->signals[1])
#define freerdp_thread_reset(_t) ResetEvent(_t->signals[1])
#define freerdp_thread_lock(_t) WaitForSingleObject(_t->mutex, INFINITE)
#define freerdp_thread_unlock(_t) ReleaseMutex(_t->mutex)

#endif /* FREERDP_UTILS_THREAD_H */
