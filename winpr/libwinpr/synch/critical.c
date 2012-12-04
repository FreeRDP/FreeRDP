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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/synch.h>

#include "synch.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef _WIN32

VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	if (lpCriticalSection)
	{
		lpCriticalSection->DebugInfo = NULL;

		lpCriticalSection->LockCount = 0;
		lpCriticalSection->RecursionCount = 0;
		lpCriticalSection->SpinCount = 0;

		lpCriticalSection->OwningThread = NULL;
		lpCriticalSection->LockSemaphore = NULL;

		lpCriticalSection->LockSemaphore = (winpr_sem_t*) malloc(sizeof(winpr_sem_t));
#if defined __APPLE__
		semaphore_create(mach_task_self(), lpCriticalSection->LockSemaphore, SYNC_POLICY_FIFO, 1);
#else
		sem_init(lpCriticalSection->LockSemaphore, 0, 1);
#endif
	}
}

BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
{
	return TRUE;
}

BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
	return TRUE;
}

DWORD SetCriticalSectionSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
	return 0;
}

VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
#if defined __APPLE__
	semaphore_wait(*((winpr_sem_t*) lpCriticalSection->LockSemaphore));
#else
	sem_wait((winpr_sem_t*) lpCriticalSection->LockSemaphore);
#endif
}

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	return TRUE;
}

VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
#if defined __APPLE__
	semaphore_signal(*((winpr_sem_t*) lpCriticalSection->LockSemaphore));
#else
	sem_post((winpr_sem_t*) lpCriticalSection->LockSemaphore);
#endif
}

VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
#if defined __APPLE__
	semaphore_destroy(mach_task_self(), *((winpr_sem_t*) lpCriticalSection->LockSemaphore));
#else
	sem_destroy((winpr_sem_t*) lpCriticalSection->LockSemaphore);
#endif
}

#endif
