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

#include <winpr/crt.h>

#include <winpr/synch.h>

#include "synch.h"

#ifndef _WIN32

#include "../handle/handle.h"

HANDLE CreateWaitableTimerA(LPSECURITY_ATTRIBUTES lpTimerAttributes, BOOL bManualReset, LPCSTR lpTimerName)
{
	HANDLE handle = NULL;
	WINPR_TIMER* timer;

	timer = (WINPR_TIMER*) malloc(sizeof(WINPR_TIMER));

	if (timer)
	{
		int status = 0;

		WINPR_HANDLE_SET_TYPE(timer, HANDLE_TYPE_TIMER);
		handle = (HANDLE) timer;

		timer->fd = -1;
		timer->lPeriod = 0;
		timer->bManualReset = bManualReset;
		timer->pfnCompletionRoutine = NULL;
		timer->lpArgToCompletionRoutine = NULL;

#ifdef HAVE_TIMERFD_H
		timer->fd = timerfd_create(CLOCK_MONOTONIC, 0);

		if (timer->fd <= 0)
			return NULL;

		status = fcntl(timer->fd, F_SETFL, O_NONBLOCK);

		if (status)
			return NULL;
#endif
	}

	return handle;
}

HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpTimerAttributes, BOOL bManualReset, LPCWSTR lpTimerName)
{
	return NULL;
}

HANDLE CreateWaitableTimerExA(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	BOOL bManualReset;

	bManualReset = (dwFlags & CREATE_WAITABLE_TIMER_MANUAL_RESET) ? TRUE : FALSE;

	return CreateWaitableTimerA(lpTimerAttributes, bManualReset, lpTimerName);
}

HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	return NULL;
}

BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, BOOL fResume)
{
	ULONG Type;
	PVOID Object;
	int status = 0;
	WINPR_TIMER* timer;
	LONGLONG seconds = 0;
	LONGLONG nanoseconds = 0;

	if (!winpr_Handle_GetInfo(hTimer, &Type, &Object))
		return FALSE;

	if (Type != HANDLE_TYPE_TIMER)
		return FALSE;

	if (!lpDueTime)
		return FALSE;

	if (lPeriod < 0)
		return FALSE;

	timer = (WINPR_TIMER*) Object;

	timer->lPeriod = lPeriod; /* milliseconds */
	timer->pfnCompletionRoutine = pfnCompletionRoutine;
	timer->lpArgToCompletionRoutine = lpArgToCompletionRoutine;

#ifdef HAVE_TIMERFD_H
	ZeroMemory(&(timer->timeout), sizeof(struct itimerspec));

	if (lpDueTime->QuadPart < 0)
	{
		LONGLONG due = lpDueTime->QuadPart * (-1);

		/* due time is in 100 nanosecond intervals */

		seconds = (due / 10000000);
		nanoseconds = ((due % 10000000) * 100);
	}
	else if (lpDueTime->QuadPart == 0)
	{
		seconds = nanoseconds = 0;
	}
	else
	{
		printf("SetWaitableTimer: implement absolute time\n");
		return FALSE;
	}

	if (lPeriod > 0)
	{
		timer->timeout.it_interval.tv_sec = (lPeriod / 1000); /* seconds */
		timer->timeout.it_interval.tv_nsec = ((lPeriod % 1000) * 1000000); /* nanoseconds */
	}

	if (lpDueTime->QuadPart != 0)
	{
		timer->timeout.it_value.tv_sec = seconds; /* seconds */
		timer->timeout.it_value.tv_nsec = nanoseconds; /* nanoseconds */
	}
	else
	{
		timer->timeout.it_value.tv_sec = timer->timeout.it_interval.tv_sec; /* seconds */
		timer->timeout.it_value.tv_nsec = timer->timeout.it_interval.tv_nsec; /* nanoseconds */
	}

	status = timerfd_settime(timer->fd, 0, &(timer->timeout), NULL);

	if (status)
	{
		printf("SetWaitableTimer timerfd_settime failure: %d\n", status);
		return FALSE;
	}
#endif

	return TRUE;
}

BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine, PREASON_CONTEXT WakeContext, ULONG TolerableDelay)
{
	ULONG Type;
	PVOID Object;
	WINPR_TIMER* timer;

	if (!winpr_Handle_GetInfo(hTimer, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_TIMER)
	{
		timer = (WINPR_TIMER*) Object;
		return TRUE;
	}

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
