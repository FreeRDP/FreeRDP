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

/* Mutex */

WINPR_API HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
WINPR_API HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName);

WINPR_API HANDLE CreateMutexExA(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCTSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);
WINPR_API HANDLE CreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);

WINPR_API HANDLE OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle,LPCSTR lpName);
WINPR_API HANDLE OpenMutexW(DWORD dwDesiredAccess, BOOL bInheritHandle,LPCWSTR lpName);

WINPR_API BOOL ReleaseMutex(HANDLE hMutex);

#ifdef UNICODE
#define CreateMutex	CreateMutexW
#define CreateMutexEx	CreateMutexExW
#define OpenMutex	OpenMutexW
#else
#define CreateMutex	CreateMutexA
#define CreateMutexEx	CreateMutexExA
#define OpenMutex	OpenMutexA
#endif

/* Semaphore */

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

/* Event */

WINPR_API HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
WINPR_API HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName);

WINPR_API HANDLE CreateEventExA(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);
WINPR_API HANDLE CreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess);

WINPR_API HANDLE OpenEventA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
WINPR_API HANDLE OpenEventW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);

WINPR_API BOOL SetEvent(HANDLE hEvent);
WINPR_API BOOL ResetEvent(HANDLE hEvent);

/* Slim Reader/Writer (SRW) Lock */

typedef PVOID RTL_SRWLOCK;
typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;

WINPR_API VOID InitializeSRWLock(PSRWLOCK SRWLock);

WINPR_API VOID AcquireSRWLockExclusive(PSRWLOCK SRWLock);
WINPR_API VOID AcquireSRWLockShared(PSRWLOCK SRWLock);

WINPR_API BOOL TryAcquireSRWLockExclusive(PSRWLOCK SRWLock);
WINPR_API BOOL TryAcquireSRWLockShared(PSRWLOCK SRWLock);

WINPR_API VOID ReleaseSRWLockExclusive(PSRWLOCK SRWLock);
WINPR_API VOID ReleaseSRWLockShared(PSRWLOCK SRWLock);

/* Condition Variable */

typedef PVOID RTL_CONDITION_VARIABLE;
typedef RTL_CONDITION_VARIABLE CONDITION_VARIABLE, *PCONDITION_VARIABLE;

/* Critical Section */

typedef struct _RTL_CRITICAL_SECTION
{
	void* DebugInfo;
	LONG LockCount;
	LONG RecursionCount;
	PVOID OwningThread;
	PVOID LockSemaphore;
	ULONG SpinCount;
} RTL_CRITICAL_SECTION, *PRTL_CRITICAL_SECTION;

typedef PRTL_CRITICAL_SECTION PCRITICAL_SECTION;
typedef PRTL_CRITICAL_SECTION LPCRITICAL_SECTION;

WINPR_API VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
WINPR_API BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount);

WINPR_API DWORD SetCriticalSectionSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount);

WINPR_API VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
WINPR_API BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

WINPR_API VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

WINPR_API VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

/* Synchronization Barrier */

typedef PVOID RTL_SYNCHRONIZATION_BARRIER;
typedef RTL_SYNCHRONIZATION_BARRIER SYNCHRONIZATION_BARRIER, *PSYNCHRONIZATION_BARRIER, *LPSYNCHRONIZATION_BARRIER;

WINPR_API BOOL InitializeSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier, LONG lTotalThreads, LONG lSpinCount);
WINPR_API BOOL EnterSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier, DWORD dwFlags);
WINPR_API BOOL DeleteSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier);

/* Sleep */

WINPR_API VOID Sleep(DWORD dwMilliseconds);
WINPR_API DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable);

/* Wait */

WINPR_API DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
WINPR_API DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds);
WINPR_API DWORD WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll, DWORD dwMilliseconds, BOOL bAlertable);

#endif

#endif /* WINPR_SYNCH_H */

