/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/interlocked.h>
#include <winpr/thread.h>

#include "synch.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach/task.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#endif

#ifndef _WIN32

#include "../log.h"
#define TAG WINPR_TAG("synch.critical")

VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	InitializeCriticalSectionEx(lpCriticalSection, 0, 0);
}

BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount,
                                 DWORD Flags)
{
	/**
	 * See http://msdn.microsoft.com/en-us/library/ff541979(v=vs.85).aspx
	 * - The LockCount field indicates the number of times that any thread has
	 *   called the EnterCriticalSection routine for this critical section,
	 *   minus one. This field starts at -1 for an unlocked critical section.
	 *   Each call of EnterCriticalSection increments this value; each call of
	 *   LeaveCriticalSection decrements it.
	 * - The RecursionCount field indicates the number of times that the owning
	 *   thread has called EnterCriticalSection for this critical section.
	 */
	if (Flags != 0)
	{
		WLog_WARN(TAG, "Flags unimplemented");
	}

	lpCriticalSection->DebugInfo = NULL;
	lpCriticalSection->LockCount = -1;
	lpCriticalSection->SpinCount = 0;
	lpCriticalSection->RecursionCount = 0;
	lpCriticalSection->OwningThread = NULL;
	lpCriticalSection->LockSemaphore = (winpr_sem_t*) malloc(sizeof(winpr_sem_t));

	if (!lpCriticalSection->LockSemaphore)
		return FALSE;

#if defined(__APPLE__)

	if (semaphore_create(mach_task_self(), lpCriticalSection->LockSemaphore, SYNC_POLICY_FIFO,
	                     0) != KERN_SUCCESS)
		goto out_fail;

#else

	if (sem_init(lpCriticalSection->LockSemaphore, 0, 0) != 0)
		goto out_fail;

#endif
	SetCriticalSectionSpinCount(lpCriticalSection, dwSpinCount);
	return TRUE;
out_fail:
	free(lpCriticalSection->LockSemaphore);
	return FALSE;
}

BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
	return InitializeCriticalSectionEx(lpCriticalSection, dwSpinCount, 0);
}

DWORD SetCriticalSectionSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
#if !defined(WINPR_CRITICAL_SECTION_DISABLE_SPINCOUNT)
	SYSTEM_INFO sysinfo;
	DWORD dwPreviousSpinCount = lpCriticalSection->SpinCount;

	if (dwSpinCount)
	{
		/* Don't spin on uniprocessor systems! */
		GetNativeSystemInfo(&sysinfo);

		if (sysinfo.dwNumberOfProcessors < 2)
			dwSpinCount = 0;
	}

	lpCriticalSection->SpinCount = dwSpinCount;
	return dwPreviousSpinCount;
#else
	return 0;
#endif
}

static VOID _WaitForCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
#if defined(__APPLE__)
	semaphore_wait(*((winpr_sem_t*) lpCriticalSection->LockSemaphore));
#else
	sem_wait((winpr_sem_t*) lpCriticalSection->LockSemaphore);
#endif
}

static VOID _UnWaitCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
#if defined __APPLE__
	semaphore_signal(*((winpr_sem_t*) lpCriticalSection->LockSemaphore));
#else
	sem_post((winpr_sem_t*) lpCriticalSection->LockSemaphore);
#endif
}

VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
#if !defined(WINPR_CRITICAL_SECTION_DISABLE_SPINCOUNT)
	ULONG SpinCount = lpCriticalSection->SpinCount;

	/* If we're lucky or if the current thread is already owner we can return early */
	if (SpinCount && TryEnterCriticalSection(lpCriticalSection))
		return;

	/* Spin requested times but don't compete with another waiting thread */
	while (SpinCount-- && lpCriticalSection->LockCount < 1)
	{
		/* Atomically try to acquire and check the if the section is free. */
		if (InterlockedCompareExchange(&lpCriticalSection->LockCount, 0, -1) == -1)
		{
			lpCriticalSection->RecursionCount = 1;
			lpCriticalSection->OwningThread = (HANDLE)(ULONG_PTR) GetCurrentThreadId();
			return;
		}

		/* Failed to get the lock. Let the scheduler know that we're spinning. */
		if (sched_yield() != 0)
		{
			/**
			 * On some operating systems sched_yield is a stub.
			 * usleep should at least trigger a context switch if any thread is waiting.
			 * A ThreadYield() would be nice in winpr ...
			 */
			usleep(1);
		}
	}

#endif

	/* First try the fastest possible path to get the lock. */
	if (InterlockedIncrement(&lpCriticalSection->LockCount))
	{
		/* Section is already locked. Check if it is owned by the current thread. */
		if (lpCriticalSection->OwningThread == (HANDLE)(ULONG_PTR) GetCurrentThreadId())
		{
			/* Recursion. No need to wait. */
			lpCriticalSection->RecursionCount++;
			return;
		}

		/* Section is locked by another thread. We have to wait. */
		_WaitForCriticalSection(lpCriticalSection);
	}

	/* We got the lock. Own it ... */
	lpCriticalSection->RecursionCount = 1;
	lpCriticalSection->OwningThread = (HANDLE)(ULONG_PTR) GetCurrentThreadId();
}

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	HANDLE current_thread = (HANDLE)(ULONG_PTR) GetCurrentThreadId();

	/* Atomically acquire the the lock if the section is free. */
	if (InterlockedCompareExchange(&lpCriticalSection->LockCount, 0, -1) == -1)
	{
		lpCriticalSection->RecursionCount = 1;
		lpCriticalSection->OwningThread = current_thread;
		return TRUE;
	}

	/* Section is already locked. Check if it is owned by the current thread. */
	if (lpCriticalSection->OwningThread == current_thread)
	{
		/* Recursion, return success */
		lpCriticalSection->RecursionCount++;
		InterlockedIncrement(&lpCriticalSection->LockCount);
		return TRUE;
	}

	return FALSE;
}

VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	/* Decrement RecursionCount and check if this is the last LeaveCriticalSection call ...*/
	if (--lpCriticalSection->RecursionCount < 1)
	{
		/* Last recursion, clear owner, unlock and if there are other waiting threads ... */
		lpCriticalSection->OwningThread = NULL;

		if (InterlockedDecrement(&lpCriticalSection->LockCount) >= 0)
		{
			/* ...signal the semaphore to unblock the next waiting thread */
			_UnWaitCriticalSection(lpCriticalSection);
		}
	}
	else
	{
		InterlockedDecrement(&lpCriticalSection->LockCount);
	}
}

VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	lpCriticalSection->LockCount = -1;
	lpCriticalSection->SpinCount = 0;
	lpCriticalSection->RecursionCount = 0;
	lpCriticalSection->OwningThread = NULL;

	if (lpCriticalSection->LockSemaphore != NULL)
	{
#if defined __APPLE__
		semaphore_destroy(mach_task_self(), *((winpr_sem_t*) lpCriticalSection->LockSemaphore));
#else
		sem_destroy((winpr_sem_t*) lpCriticalSection->LockSemaphore);
#endif
		free(lpCriticalSection->LockSemaphore);
		lpCriticalSection->LockSemaphore = NULL;
	}
}

#endif
