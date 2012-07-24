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

#ifndef WINPR_SYNCH_H
#define WINPR_SYNCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/handle.h>

#ifndef _WIN32

WINPR_API HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
WINPR_API HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName);

#ifdef UNICODE
#define CreateSemaphore CreateSemaphoreW
#else
#define CreateSemaphore CreateSemaphoreA
#endif

WINPR_API HANDLE OpenSemaphoreA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
WINPR_API HANDLE OpenSemaphoreW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);

#ifdef UNICODE
#define OpenSemaphore OpenSemaphoreW
#else
#define OpenSemaphore OpenSemaphoreA
#endif

WINPR_API BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount);

#define WAIT_OBJECT_0		0x00000000L
#define WAIT_ABANDONED		0x00000080L
#define WAIT_TIMEOUT		0x00000102L
#define WAIT_FAILED		((DWORD) 0xFFFFFFFF)

WINPR_API DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
WINPR_API DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds);

#endif

#endif /* WINPR_SYNCH_H */

