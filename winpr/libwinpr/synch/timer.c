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

/**
 * CreateWaitableTimerExW
 * OpenWaitableTimerW
 * SetWaitableTimer
 * SetWaitableTimerEx
 * CancelWaitableTimer
 */

#ifndef _WIN32

HANDLE CreateWaitableTimerExA(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	return NULL;
}

HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	return NULL;
}

BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, BOOL fResume)
{
	return TRUE;
}

BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, PREASON_CONTEXT WakeContext, ULONG TolerableDelay)
{
	return TRUE;
}

HANDLE OpenWaitableTimerA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpTimerName)
{
	return NULL;
}

HANDLE OpenWaitableTimerW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpTimerName)
{
	return NULL;
}

BOOL CancelWaitableTimer(HANDLE hTimer)
{
	return TRUE;
}

#endif
