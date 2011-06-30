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

#include <pthread.h>
#include <semaphore.h>
#include <freerdp/utils/semaphore.h>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#endif

void freerdp_sem_create(void * sem_struct, int iv)
{
#ifdef __APPLE__
	semaphore_create(mach_task_self(), (semaphore_t *)sem_struct, SYNC_POLICY_FIFO, iv);
#else
	int pshared = 0;
	sem_init((sem_t *)sem_struct, pshared, iv);
#endif
}

void freerdp_sem_signal(void * sem_struct)
{
#ifdef __APPLE__
	semaphore_signal(*((semaphore_t *)sem_struct));
#else
	sem_post((sem_t *)sem_struct);
#endif
}

void freerdp_sem_wait(void * sem_struct)
{
#ifdef __APPLE__
	semaphore_wait(*((semaphore_t *)sem_struct));
#else
	sem_wait((sem_t *)sem_struct);
#endif
}

