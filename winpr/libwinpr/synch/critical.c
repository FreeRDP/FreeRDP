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

/**
 * TODO: EnterCriticalSection allows recursive calls from the same thread.
 *       Implement this using pthreads (see PTHREAD_MUTEX_RECURSIVE_NP)
 *       http://msdn.microsoft.com/en-us/library/windows/desktop/ms682608%28v=vs.85%29.aspx
 */

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

		lpCriticalSection->LockSemaphore = malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(lpCriticalSection->LockSemaphore, NULL);
	}
}

BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
{
	if (Flags != 0) {
		 fprintf(stderr, "warning: InitializeCriticalSectionEx Flags unimplemented\n");
	}
	return InitializeCriticalSectionAndSpinCount(lpCriticalSection, dwSpinCount);
}

BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
	InitializeCriticalSection(lpCriticalSection);
	SetCriticalSectionSpinCount(lpCriticalSection, dwSpinCount);
	return TRUE;
}

DWORD SetCriticalSectionSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
	DWORD dwPreviousSpinCount = lpCriticalSection->SpinCount;
	lpCriticalSection->SpinCount = dwSpinCount;
	return dwPreviousSpinCount;
}

VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	/**
	 * Linux NPTL thread synchronization primitives are implemented using
	 * the futex system calls ... no need for performing a trylock loop.
	 */
#if !defined(__linux__)
	ULONG spin = lpCriticalSection->SpinCount;
	while (spin--)
	{
		if (pthread_mutex_trylock((pthread_mutex_t*)lpCriticalSection->LockSemaphore) == 0)
			return;
		pthread_yield();
	}
#endif
	pthread_mutex_lock((pthread_mutex_t*)lpCriticalSection->LockSemaphore);
}

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	return (pthread_mutex_trylock((pthread_mutex_t*)lpCriticalSection->LockSemaphore) == 0 ? TRUE : FALSE);
}

VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	pthread_mutex_unlock((pthread_mutex_t*)lpCriticalSection->LockSemaphore);
}

VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	pthread_mutex_destroy((pthread_mutex_t*)lpCriticalSection->LockSemaphore);
	free(lpCriticalSection->LockSemaphore);
}

#endif
