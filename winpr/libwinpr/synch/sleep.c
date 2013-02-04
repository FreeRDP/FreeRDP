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

#include <winpr/windows.h>

#include <winpr/synch.h>

#ifndef _WIN32

#include <time.h>

#ifdef HAVE_UNISTD_H
#define _XOPEN_SOURCE 500
#include <unistd.h>
#endif

VOID Sleep(DWORD dwMilliseconds)
{
	usleep(dwMilliseconds * 1000);
}

DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable)
{
	usleep(dwMilliseconds * 1000);
	return TRUE;
}

#endif

VOID USleep(DWORD dwMicroseconds)
{
#ifndef _WIN32
	usleep(dwMicroseconds);
#else
	UINT64 t1;
	UINT64 t2;
	UINT64 freq;

	QueryPerformanceCounter((LARGE_INTEGER*) &t1);
	QueryPerformanceCounter((LARGE_INTEGER*) &freq);

	do
	{
		QueryPerformanceCounter((LARGE_INTEGER*) &t2);
	}
	while ((t2 - t1) < dwMicroseconds);
#endif
}
