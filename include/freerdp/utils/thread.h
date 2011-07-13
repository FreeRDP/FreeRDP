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

#ifdef _WIN32

#define freerdp_thread_create(_proc, _arg) do { \
	DWORD thread; \
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_proc, _arg, 0, &thread); \
	while (0)

#else

#include <pthread.h>
#define freerdp_thread_create(_proc, _arg) do { \
	pthread_t thread; \
	pthread_create(&thread, 0, _proc, _arg); \
	pthread_detach(thread); \
	} while (0)

#endif

#endif /* __THREAD_UTILS_H */
