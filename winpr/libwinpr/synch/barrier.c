/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/crt.h>

#ifdef WINPR_SYNCHRONIZATION_BARRIER

#include <assert.h>
#include <winpr/sysinfo.h>
#include <winpr/library.h>
#include <winpr/interlocked.h>
#include <winpr/thread.h>

/**
 * WinPR uses the internal RTL_BARRIER struct members exactly like Windows:
 *
 * DWORD Reserved1:          number of threads that have not yet entered the barrier
 * DWORD Reserved2:          number of threads required to enter the barrier
 * ULONG_PTR Reserved3[2];   two synchronization events (manual reset events)
 * DWORD Reserved4;          number of processors
 * DWORD Reserved5;          spincount
 */

#ifdef _WIN32

static HMODULE g_Kernel32 = NULL;
static BOOL g_NativeBarrier = FALSE;
static INIT_ONCE g_InitOnce = INIT_ONCE_STATIC_INIT;

typedef BOOL (WINAPI * fnInitializeSynchronizationBarrier)(LPSYNCHRONIZATION_BARRIER lpBarrier, LONG lTotalThreads, LONG lSpinCount);
typedef BOOL (WINAPI * fnEnterSynchronizationBarrier)(LPSYNCHRONIZATION_BARRIER lpBarrier, DWORD dwFlags);
typedef BOOL (WINAPI * fnDeleteSynchronizationBarrier)(LPSYNCHRONIZATION_BARRIER lpBarrier);

static fnInitializeSynchronizationBarrier pfnInitializeSynchronizationBarrier = NULL;
static fnEnterSynchronizationBarrier pfnEnterSynchronizationBarrier = NULL;
static fnDeleteSynchronizationBarrier pfnDeleteSynchronizationBarrier = NULL;

static BOOL CALLBACK InitOnce_Barrier(PINIT_ONCE once, PVOID param, PVOID *context)
{
	g_Kernel32 = LoadLibraryA("kernel32.dll");

	if (!g_Kernel32)
		return TRUE;

	pfnInitializeSynchronizationBarrier = (fnInitializeSynchronizationBarrier)
			GetProcAddress(g_Kernel32, "InitializeSynchronizationBarrier");

	pfnEnterSynchronizationBarrier = (fnEnterSynchronizationBarrier)
			GetProcAddress(g_Kernel32, "EnterSynchronizationBarrier");

	pfnDeleteSynchronizationBarrier = (fnDeleteSynchronizationBarrier)
			GetProcAddress(g_Kernel32, "DeleteSynchronizationBarrier");

	if (pfnInitializeSynchronizationBarrier && pfnEnterSynchronizationBarrier
			&& pfnDeleteSynchronizationBarrier)
	{
		g_NativeBarrier = TRUE;
	}

	return TRUE;
}

#endif

BOOL WINAPI winpr_InitializeSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier, LONG lTotalThreads, LONG lSpinCount)
{
	SYSTEM_INFO sysinfo;
	HANDLE hEvent0;
	HANDLE hEvent1;

#ifdef _WIN32
	InitOnceExecuteOnce(&g_InitOnce, InitOnce_Barrier, NULL, NULL);

	if (g_NativeBarrier)
		return pfnInitializeSynchronizationBarrier(lpBarrier, lTotalThreads, lSpinCount);
#endif

	if (!lpBarrier || lTotalThreads < 1 || lSpinCount < -1)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	ZeroMemory(lpBarrier, sizeof(SYNCHRONIZATION_BARRIER));

	if (lSpinCount == -1)
		lSpinCount = 2000;

	if (!(hEvent0 = CreateEvent(NULL, TRUE, FALSE, NULL)))
		return FALSE;

	if (!(hEvent1 = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		CloseHandle(hEvent0);
		return FALSE;
	}

	GetNativeSystemInfo(&sysinfo);

	lpBarrier->Reserved1 = lTotalThreads;
	lpBarrier->Reserved2 = lTotalThreads;
	lpBarrier->Reserved3[0] = (ULONG_PTR)hEvent0;
	lpBarrier->Reserved3[1] = (ULONG_PTR)hEvent1;
	lpBarrier->Reserved4 = sysinfo.dwNumberOfProcessors;
	lpBarrier->Reserved5 = lSpinCount;

	return TRUE;
}

BOOL WINAPI winpr_EnterSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier, DWORD dwFlags)
{
	LONG remainingThreads;
	HANDLE hCurrentEvent;
	HANDLE hDormantEvent;

#ifdef _WIN32
	if (g_NativeBarrier)
		return pfnEnterSynchronizationBarrier(lpBarrier, dwFlags);
#endif

	if (!lpBarrier)
		return FALSE;

	/**
	 * dwFlags according to https://msdn.microsoft.com/en-us/library/windows/desktop/hh706889(v=vs.85).aspx
	 *
	 * SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY (0x01)
	 * Specifies that the thread entering the barrier should block
	 * immediately until the last thread enters the barrier.
	 *
	 * SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY (0x02)
	 * Specifies that the thread entering the barrier should spin until the
	 * last thread enters the barrier, even if the spinning thread exceeds
	 * the barrier's maximum spin count.
	 *
	 * SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE (0x04)
	 * Specifies that the function can skip the work required to ensure
	 * that it is safe to delete the barrier, which can improve
	 * performance. All threads that enter this barrier must specify the
	 * flag; otherwise, the flag is ignored. This flag should be used only
	 * if the barrier will never be deleted.
	 */

	hCurrentEvent = (HANDLE)lpBarrier->Reserved3[0];
	hDormantEvent = (HANDLE)lpBarrier->Reserved3[1];

	remainingThreads = InterlockedDecrement((LONG*)&lpBarrier->Reserved1);

	assert(remainingThreads >= 0);

	if (remainingThreads > 0)
	{
		DWORD dwProcessors = lpBarrier->Reserved4;
		BOOL spinOnly = dwFlags & SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY;
		BOOL blockOnly = dwFlags & SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY;
		BOOL block = TRUE;

		/**
		 * If SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY is set we will
		 * always spin and trust that the user knows what he/she/it
		 * is doing. Otherwise we'll only spin if the flag
		 * SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY is not set and
		 * the number of remaining threads is less than the number
		 * of processors.
		 */

		if (spinOnly || (remainingThreads < dwProcessors && !blockOnly))
		{
			DWORD dwSpinCount = lpBarrier->Reserved5;
			DWORD sp = 0;
			/**
			 * nb: we must let the compiler know that our comparand
			 * can change between the iterations in the loop below
			 */
			volatile ULONG_PTR* cmp = &lpBarrier->Reserved3[0];
			/* we spin until the last thread _completed_ the event switch */
			while ((block = (*cmp == (ULONG_PTR)hCurrentEvent)))
				if (!spinOnly && ++sp > dwSpinCount)
					break;
		}

		if (block)
			WaitForSingleObject(hCurrentEvent, INFINITE);

		return FALSE;
	}

	/* reset the dormant event first */
	ResetEvent(hDormantEvent);

	/* reset the remaining counter */
	lpBarrier->Reserved1 = lpBarrier->Reserved2;

	/* switch events - this will also unblock the spinning threads */
	lpBarrier->Reserved3[1] = (ULONG_PTR)hCurrentEvent;
	lpBarrier->Reserved3[0] = (ULONG_PTR)hDormantEvent;

	/* signal the blocked threads */
	SetEvent(hCurrentEvent);

	return TRUE;
}

BOOL WINAPI winpr_DeleteSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier)
{
#ifdef _WIN32
	if (g_NativeBarrier)
		return pfnDeleteSynchronizationBarrier(lpBarrier);
#endif

	/**
	 * According to https://msdn.microsoft.com/en-us/library/windows/desktop/hh706887(v=vs.85).aspx
	 * Return value:
	 * The DeleteSynchronizationBarrier function always returns TRUE.
	 */

	if (!lpBarrier)
		return TRUE;

	while (lpBarrier->Reserved1 != lpBarrier->Reserved2)
		SwitchToThread();

	if (lpBarrier->Reserved3[0])
		CloseHandle((HANDLE)lpBarrier->Reserved3[0]);

	if (lpBarrier->Reserved3[1])
		CloseHandle((HANDLE)lpBarrier->Reserved3[1]);

	ZeroMemory(lpBarrier, sizeof(SYNCHRONIZATION_BARRIER));

	return TRUE;
}

#endif
