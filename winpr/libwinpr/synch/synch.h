/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
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

#ifndef WINPR_SYNCH_PRIVATE_H
#define WINPR_SYNCH_PRIVATE_H

#include <winpr/synch.h>

#if defined __APPLE__
#include <pthread.h>
#include <semaphore.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#define winpr_sem_t semaphore_t
#else
#include <pthread.h>
#include <semaphore.h>
#define winpr_sem_t sem_t
#endif

#endif /* WINPR_SYNCH_PRIVATE_H */
