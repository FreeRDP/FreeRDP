/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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
#include <winpr/error.h>
#include <winpr/handle.h>

#include <winpr/nt.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _WIN32

/* Mutex */
#define CREATE_MUTEX_INITIAL_OWNER 0x00000001

	WINPR_API HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner,
	                              LPCSTR lpName);
	WINPR_API HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner,
	                              LPCWSTR lpName);

	WINPR_API HANDLE CreateMutexExA(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCSTR lpName,
	                                DWORD dwFlags, DWORD dwDesiredAccess);
	WINPR_API HANDLE CreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName,
	                                DWORD dwFlags, DWORD dwDesiredAccess);

	WINPR_API HANDLE OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
	WINPR_API HANDLE OpenMutexW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);

	WINPR_API BOOL ReleaseMutex(HANDLE hMutex);

#ifdef UNICODE
#define CreateMutex CreateMutexW
#define CreateMutexEx CreateMutexExW
#define OpenMutex OpenMutexW
#else
#define CreateMutex CreateMutexA
#define CreateMutexEx CreateMutexExA
#define OpenMutex OpenMutexA
#endif

	/* Semaphore */

	WINPR_API HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
	                                  LONG lInitialCount, LONG lMaximumCount, LPCSTR lpName);
	WINPR_API HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
	                                  LONG lInitialCount, LONG lMaximumCount, LPCWSTR lpName);

	WINPR_API HANDLE OpenSemaphoreA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
	WINPR_API HANDLE OpenSemaphoreW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);

#ifdef UNICODE
#define CreateSemaphore CreateSemaphoreW
#define OpenSemaphore OpenSemaphoreW
#else
#define CreateSemaphore CreateSemaphoreA
#define OpenSemaphore OpenSemaphoreA
#endif

	WINPR_API BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount);

/* Event */
#define CREATE_EVENT_MANUAL_RESET 0x00000001
#define CREATE_EVENT_INITIAL_SET 0x00000002

	WINPR_API HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
	                              BOOL bInitialState, LPCSTR lpName);
	WINPR_API HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
	                              BOOL bInitialState, LPCWSTR lpName);

	WINPR_API HANDLE CreateEventExA(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCSTR lpName,
	                                DWORD dwFlags, DWORD dwDesiredAccess);
	WINPR_API HANDLE CreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName,
	                                DWORD dwFlags, DWORD dwDesiredAccess);

	WINPR_API HANDLE OpenEventA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
	WINPR_API HANDLE OpenEventW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);

	WINPR_API BOOL SetEvent(HANDLE hEvent);
	WINPR_API BOOL ResetEvent(HANDLE hEvent);

#if defined(WITH_DEBUG_EVENTS)
#define DumpEventHandles() DumpEventHandles_(__FUNCTION__, __FILE__, __LINE__)
	WINPR_API void DumpEventHandles_(const char* fkt, const char* file, size_t line);
#endif
#ifdef UNICODE
#define CreateEvent CreateEventW
#define CreateEventEx CreateEventExW
#define OpenEvent OpenEventW
#else
#define CreateEvent CreateEventA
#define CreateEventEx CreateEventExA
#define OpenEvent OpenEventA
#endif

	/* Condition Variable */

	typedef PVOID RTL_CONDITION_VARIABLE;
	typedef RTL_CONDITION_VARIABLE CONDITION_VARIABLE, *PCONDITION_VARIABLE;

	/* Critical Section */

	typedef struct
	{
		PVOID DebugInfo;
		LONG LockCount;
		LONG RecursionCount;
		HANDLE OwningThread;
		HANDLE LockSemaphore;
		ULONG_PTR SpinCount;
	} RTL_CRITICAL_SECTION, *PRTL_CRITICAL_SECTION;

	typedef RTL_CRITICAL_SECTION CRITICAL_SECTION;
	typedef PRTL_CRITICAL_SECTION PCRITICAL_SECTION;
	typedef PRTL_CRITICAL_SECTION LPCRITICAL_SECTION;

	WINPR_API VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
	WINPR_API BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection,
	                                           DWORD dwSpinCount, DWORD Flags);
	WINPR_API BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection,
	                                                     DWORD dwSpinCount);

	WINPR_API DWORD SetCriticalSectionSpinCount(LPCRITICAL_SECTION lpCriticalSection,
	                                            DWORD dwSpinCount);

	WINPR_API VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);
	WINPR_API BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

	WINPR_API VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

	WINPR_API VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection);

	/* Sleep */

	WINPR_API VOID Sleep(DWORD dwMilliseconds);
	WINPR_API DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable);

	/* Address */

	WINPR_API VOID WakeByAddressAll(PVOID Address);
	WINPR_API VOID WakeByAddressSingle(PVOID Address);

	WINPR_API BOOL WaitOnAddress(VOID volatile* Address, PVOID CompareAddress, SIZE_T AddressSize,
	                             DWORD dwMilliseconds);

	/* Wait */

#define INFINITE 0xFFFFFFFF

#define WAIT_OBJECT_0 0x00000000L
#define WAIT_ABANDONED 0x00000080L
#define WAIT_IO_COMPLETION 0x000000C0L

#ifndef WAIT_TIMEOUT
#define WAIT_TIMEOUT 0x00000102L
#endif

#define WAIT_FAILED ((DWORD)0xFFFFFFFF)

#define MAXIMUM_WAIT_OBJECTS 64

	WINPR_API DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
	WINPR_API DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable);
	WINPR_API DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll,
	                                       DWORD dwMilliseconds);
	WINPR_API DWORD WaitForMultipleObjectsEx(DWORD nCount, const HANDLE* lpHandles, BOOL bWaitAll,
	                                         DWORD dwMilliseconds, BOOL bAlertable);

	WINPR_API DWORD SignalObjectAndWait(HANDLE hObjectToSignal, HANDLE hObjectToWaitOn,
	                                    DWORD dwMilliseconds, BOOL bAlertable);

	/* Waitable Timer */

#define CREATE_WAITABLE_TIMER_MANUAL_RESET 0x00000001

	typedef struct
	{
		ULONG Version;
		DWORD Flags;

		union
		{
			struct
			{
				HMODULE LocalizedReasonModule;
				ULONG LocalizedReasonId;
				ULONG ReasonStringCount;
				LPWSTR* ReasonStrings;
			} Detailed;

			LPWSTR SimpleReasonString;
		} Reason;
	} REASON_CONTEXT, *PREASON_CONTEXT;

	typedef VOID (*PTIMERAPCROUTINE)(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue,
	                                 DWORD dwTimerHighValue);

	WINPR_API HANDLE CreateWaitableTimerA(LPSECURITY_ATTRIBUTES lpTimerAttributes,
	                                      BOOL bManualReset, LPCSTR lpTimerName);
	WINPR_API HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpTimerAttributes,
	                                      BOOL bManualReset, LPCWSTR lpTimerName);

	WINPR_API HANDLE CreateWaitableTimerExA(LPSECURITY_ATTRIBUTES lpTimerAttributes,
	                                        LPCSTR lpTimerName, DWORD dwFlags,
	                                        DWORD dwDesiredAccess);
	WINPR_API HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes,
	                                        LPCWSTR lpTimerName, DWORD dwFlags,
	                                        DWORD dwDesiredAccess);

	WINPR_API BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
	                                PTIMERAPCROUTINE pfnCompletionRoutine,
	                                LPVOID lpArgToCompletionRoutine, BOOL fResume);

	WINPR_API BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER* lpDueTime, LONG lPeriod,
	                                  PTIMERAPCROUTINE pfnCompletionRoutine,
	                                  LPVOID lpArgToCompletionRoutine, PREASON_CONTEXT WakeContext,
	                                  ULONG TolerableDelay);

	WINPR_API HANDLE OpenWaitableTimerA(DWORD dwDesiredAccess, BOOL bInheritHandle,
	                                    LPCSTR lpTimerName);
	WINPR_API HANDLE OpenWaitableTimerW(DWORD dwDesiredAccess, BOOL bInheritHandle,
	                                    LPCWSTR lpTimerName);

	WINPR_API BOOL CancelWaitableTimer(HANDLE hTimer);

#ifdef UNICODE
#define CreateWaitableTimer CreateWaitableTimerW
#define CreateWaitableTimerEx CreateWaitableTimerExW
#define OpenWaitableTimer OpenWaitableTimerW
#else
#define CreateWaitableTimer CreateWaitableTimerA
#define CreateWaitableTimerEx CreateWaitableTimerExA
#define OpenWaitableTimer OpenWaitableTimerA
#endif

	WINPR_API int GetTimerFileDescriptor(HANDLE hEvent);

	/**
	 * Timer-Queue Timer
	 */

#define WT_EXECUTEDEFAULT 0x00000000
#define WT_EXECUTEINIOTHREAD 0x00000001
#define WT_EXECUTEINUITHREAD 0x00000002
#define WT_EXECUTEINWAITTHREAD 0x00000004
#define WT_EXECUTEONLYONCE 0x00000008
#define WT_EXECUTELONGFUNCTION 0x00000010
#define WT_EXECUTEINTIMERTHREAD 0x00000020
#define WT_EXECUTEINPERSISTENTIOTHREAD 0x00000040
#define WT_EXECUTEINPERSISTENTTHREAD 0x00000080
#define WT_TRANSFER_IMPERSONATION 0x00000100

	typedef VOID (*WAITORTIMERCALLBACK)(PVOID lpParameter, BOOLEAN TimerOrWaitFired);

	WINPR_API HANDLE CreateTimerQueue(void);
	WINPR_API BOOL DeleteTimerQueue(HANDLE TimerQueue);
	WINPR_API BOOL DeleteTimerQueueEx(HANDLE TimerQueue, HANDLE CompletionEvent);

	WINPR_API BOOL CreateTimerQueueTimer(PHANDLE phNewTimer, HANDLE TimerQueue,
	                                     WAITORTIMERCALLBACK Callback, PVOID Parameter,
	                                     DWORD DueTime, DWORD Period, ULONG Flags);
	WINPR_API BOOL ChangeTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, ULONG DueTime,
	                                     ULONG Period);
	WINPR_API BOOL DeleteTimerQueueTimer(HANDLE TimerQueue, HANDLE Timer, HANDLE CompletionEvent);

#endif

#if (defined(_WIN32) && (_WIN32_WINNT < 0x0600))
#define InitializeCriticalSectionEx(lpCriticalSection, dwSpinCount, Flags) \
	InitializeCriticalSectionAndSpinCount(lpCriticalSection, dwSpinCount)
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif

#ifndef _RTL_RUN_ONCE_DEF
#define _RTL_RUN_ONCE_DEF

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#define RTL_RUN_ONCE_INIT \
	{                     \
		0                 \
	}

#define RTL_RUN_ONCE_CHECK_ONLY 0x00000001
#define RTL_RUN_ONCE_ASYNC 0x00000002
#define RTL_RUN_ONCE_INIT_FAILED 0x00000004

#define RTL_RUN_ONCE_CTX_RESERVED_BITS 2

	typedef struct
	{
		PVOID Ptr;
	} RTL_RUN_ONCE, *PRTL_RUN_ONCE;

	typedef ULONG CALLBACK RTL_RUN_ONCE_INIT_FN(PRTL_RUN_ONCE RunOnce, PVOID Parameter,
	                                            PVOID* Context);
	typedef RTL_RUN_ONCE_INIT_FN* PRTL_RUN_ONCE_INIT_FN;

#endif

#if (!defined(_WIN32)) || (defined(_WIN32) && (_WIN32_WINNT < 0x0600))

	/* One-Time Initialization */

#define INIT_ONCE_STATIC_INIT RTL_RUN_ONCE_INIT

	typedef RTL_RUN_ONCE INIT_ONCE;
	typedef PRTL_RUN_ONCE PINIT_ONCE;
	typedef PRTL_RUN_ONCE LPINIT_ONCE;
	typedef BOOL(CALLBACK* PINIT_ONCE_FN)(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* Context);

	WINPR_API BOOL winpr_InitOnceBeginInitialize(LPINIT_ONCE lpInitOnce, DWORD dwFlags,
	                                             PBOOL fPending, LPVOID* lpContext);
	WINPR_API BOOL winpr_InitOnceComplete(LPINIT_ONCE lpInitOnce, DWORD dwFlags, LPVOID lpContext);
	WINPR_API BOOL winpr_InitOnceExecuteOnce(PINIT_ONCE InitOnce, PINIT_ONCE_FN InitFn,
	                                         PVOID Parameter, LPVOID* Context);
	WINPR_API VOID winpr_InitOnceInitialize(PINIT_ONCE InitOnce);

#define InitOnceBeginInitialize winpr_InitOnceBeginInitialize
#define InitOnceComplete winpr_InitOnceComplete
#define InitOnceExecuteOnce winpr_InitOnceExecuteOnce
#define InitOnceInitialize winpr_InitOnceInitialize
#endif

	/* Synchronization Barrier */

#if (!defined(_WIN32)) || (defined(_WIN32) && (_WIN32_WINNT < 0x0602) && !defined(_SYNCHAPI_H_))
#define WINPR_SYNCHRONIZATION_BARRIER 1
#endif

#ifdef WINPR_SYNCHRONIZATION_BARRIER

	typedef struct
	{
		DWORD Reserved1;
		DWORD Reserved2;
		ULONG_PTR Reserved3[2];
		DWORD Reserved4;
		DWORD Reserved5;
	} RTL_BARRIER, *PRTL_BARRIER;

	typedef RTL_BARRIER SYNCHRONIZATION_BARRIER;
	typedef PRTL_BARRIER PSYNCHRONIZATION_BARRIER;
	typedef PRTL_BARRIER LPSYNCHRONIZATION_BARRIER;

#define SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY 0x01
#define SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY 0x02
#define SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE 0x04

	WINPR_API BOOL WINAPI winpr_InitializeSynchronizationBarrier(
	    LPSYNCHRONIZATION_BARRIER lpBarrier, LONG lTotalThreads, LONG lSpinCount);
	WINPR_API BOOL WINAPI winpr_EnterSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier,
	                                                        DWORD dwFlags);
	WINPR_API BOOL WINAPI winpr_DeleteSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier);

#define InitializeSynchronizationBarrier winpr_InitializeSynchronizationBarrier
#define EnterSynchronizationBarrier winpr_EnterSynchronizationBarrier
#define DeleteSynchronizationBarrier winpr_DeleteSynchronizationBarrier

#endif

	/* Extended API */

	WINPR_API VOID USleep(DWORD dwMicroseconds);

	WINPR_API HANDLE CreateFileDescriptorEventW(LPSECURITY_ATTRIBUTES lpEventAttributes,
	                                            BOOL bManualReset, BOOL bInitialState,
	                                            int FileDescriptor, ULONG mode);
	WINPR_API HANDLE CreateFileDescriptorEventA(LPSECURITY_ATTRIBUTES lpEventAttributes,
	                                            BOOL bManualReset, BOOL bInitialState,
	                                            int FileDescriptor, ULONG mode);

	WINPR_API HANDLE CreateWaitObjectEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,
	                                       BOOL bManualReset, BOOL bInitialState, void* pObject);

#ifdef UNICODE
#define CreateFileDescriptorEvent CreateFileDescriptorEventW
#else
#define CreateFileDescriptorEvent CreateFileDescriptorEventA
#endif

	WINPR_API int GetEventFileDescriptor(HANDLE hEvent);
	WINPR_API int SetEventFileDescriptor(HANDLE hEvent, int FileDescriptor, ULONG mode);

	WINPR_API void* GetEventWaitObject(HANDLE hEvent);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_SYNCH_H */
