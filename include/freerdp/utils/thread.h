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

#ifndef __THREAD_UTILS_H
#define __THREAD_UTILS_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/mutex.h>
#include <freerdp/utils/wait_obj.h>
#ifndef _WIN32
#include <pthread.h>
#endif

typedef struct _freerdp_thread freerdp_thread;

struct _freerdp_thread
{
	freerdp_mutex* mutex;

	struct wait_obj* signals[5];
	int num_signals;

	int status;
};

FREERDP_API freerdp_thread* freerdp_thread_new(void);
FREERDP_API void freerdp_thread_start(freerdp_thread* thread, void* func, void* arg);
FREERDP_API void freerdp_thread_stop(freerdp_thread* thread);
FREERDP_API void freerdp_thread_free(freerdp_thread* thread);

#define freerdp_thread_wait(_t) wait_obj_select(_t->signals, _t->num_signals, -1)
#define freerdp_thread_wait_timeout(_t, _timeout) wait_obj_select(_t->signals, _t->num_signals, _timeout)
#define freerdp_thread_is_stopped(_t) wait_obj_is_set(_t->signals[0])
#define freerdp_thread_is_running(_t) (_t->status == 1)
#define freerdp_thread_quit(_t) do { \
	_t->status = -1; \
	wait_obj_clear(_t->signals[0]); } while (0)
#define freerdp_thread_signal(_t) wait_obj_set(_t->signals[1])
#define freerdp_thread_reset(_t) wait_obj_clear(_t->signals[1])
#define freerdp_thread_lock(_t) freerdp_mutex_lock(_t->mutex)
#define freerdp_thread_unlock(_t) freerdp_mutex_unlock(_t->mutex)

#endif /* __THREAD_UTILS_H */
