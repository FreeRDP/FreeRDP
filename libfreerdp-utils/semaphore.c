/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Semaphore Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <freerdp/utils/semaphore.h>

#if defined __APPLE__

#include <pthread.h>
#include <semaphore.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#define freerdp_sem_t semaphore_t

#elif defined _WIN32

#include <windows.h>
#define freerdp_sem_t HANDLE

#else

#include <pthread.h>
#include <semaphore.h>
#define freerdp_sem_t sem_t

#endif

freerdp_sem freerdp_sem_new(int iv)
{
	freerdp_sem_t* sem;

	sem = xnew(freerdp_sem_t);

#if defined __APPLE__
	semaphore_create(mach_task_self(), sem, SYNC_POLICY_FIFO, iv);
#elif defined _WIN32
	*sem = CreateSemaphore(NULL, 0, iv, NULL);
#else
	sem_init(sem, 0, iv);
#endif

	return sem;
}

void freerdp_sem_free(freerdp_sem sem)
{
#if defined __APPLE__
	semaphore_destroy(mach_task_self(), *((freerdp_sem_t*)sem));
#elif defined _WIN32
	CloseHandle(*((freerdp_sem_t*)sem));
#else
	sem_destroy((freerdp_sem_t*)sem);
#endif

	xfree(sem);
}

void freerdp_sem_signal(freerdp_sem sem)
{
#if defined __APPLE__
	semaphore_signal(*((freerdp_sem_t*)sem));
#elif defined _WIN32
	ReleaseSemaphore(*((freerdp_sem_t*)sem), 1, NULL);
#else
	sem_post((freerdp_sem_t*)sem);
#endif
}

void freerdp_sem_wait(freerdp_sem sem)
{
#if defined __APPLE__
	semaphore_wait(*((freerdp_sem_t*)sem));
#elif defined _WIN32
	WaitForSingleObject(*((freerdp_sem_t*)sem), INFINITE);
#else
	sem_wait((freerdp_sem_t*)sem);
#endif
}

