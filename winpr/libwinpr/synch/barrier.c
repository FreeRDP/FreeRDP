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

#include <winpr/crt.h>

#ifdef WINPR_SYNCHRONIZATION_BARRIER

#include <winpr/library.h>
#include <winpr/interlocked.h>

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

BOOL WINAPI InitializeSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier, LONG lTotalThreads, LONG lSpinCount)
{
	WINPR_BARRIER* pBarrier;

#ifdef _WIN32
	InitOnceExecuteOnce(&g_InitOnce, InitOnce_Barrier, NULL, NULL);

	if (g_NativeBarrier)
		return pfnInitializeSynchronizationBarrier(lpBarrier, lTotalThreads, lSpinCount);
#endif

	if (!lpBarrier)
		return FALSE;

	ZeroMemory(lpBarrier, sizeof(SYNCHRONIZATION_BARRIER));

	pBarrier = (WINPR_BARRIER*) calloc(1, sizeof(WINPR_BARRIER));

	if (!pBarrier)
		return FALSE;

	if (lSpinCount < 0)
		lSpinCount = 2000;

	pBarrier->lTotalThreads = lTotalThreads;
	pBarrier->lSpinCount = lSpinCount;
	pBarrier->count = 0;

	pBarrier->event = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!pBarrier->event)
	{
		free(pBarrier);
		return FALSE;
	}

	lpBarrier->Reserved3[0] = (ULONG_PTR) pBarrier;

	return TRUE;
}

BOOL WINAPI EnterSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier, DWORD dwFlags)
{
	LONG count;
	BOOL status = FALSE;
	WINPR_BARRIER* pBarrier;

#ifdef _WIN32
	if (g_NativeBarrier)
		return pfnEnterSynchronizationBarrier(lpBarrier, dwFlags);
#endif

	if (!lpBarrier)
		return FALSE;

	pBarrier = (WINPR_BARRIER*) lpBarrier->Reserved3[0];

	if (!pBarrier)
		return FALSE;

	count = InterlockedIncrement(&(pBarrier->count));

	if (count < pBarrier->lTotalThreads)
	{
		WaitForSingleObject(pBarrier->event, INFINITE);
	}
	else
	{
		SetEvent(pBarrier->event);
		status = TRUE;
	}

	InterlockedDecrement(&(pBarrier->count));

	return status;
}

BOOL WINAPI DeleteSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier)
{
	WINPR_BARRIER* pBarrier;

#ifdef _WIN32
	if (g_NativeBarrier)
		return pfnDeleteSynchronizationBarrier(lpBarrier);
#endif

	if (!lpBarrier)
		return TRUE;

	pBarrier = (WINPR_BARRIER*) lpBarrier->Reserved3[0];

	if (!pBarrier)
		return TRUE;

	while (InterlockedCompareExchange(&pBarrier->count, 0, 0) != 0)
		Sleep(100);

	CloseHandle(pBarrier->event);

	free(pBarrier);

	ZeroMemory(lpBarrier, sizeof(SYNCHRONIZATION_BARRIER));

	return TRUE;
}

#endif
