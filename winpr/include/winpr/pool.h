/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API
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

#ifndef WINPR_POOL_H
#define WINPR_POOL_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/synch.h>
#include <winpr/thread.h>

#ifndef _WIN32

typedef DWORD TP_VERSION, *PTP_VERSION;

typedef struct _TP_CALLBACK_INSTANCE TP_CALLBACK_INSTANCE, *PTP_CALLBACK_INSTANCE;

typedef VOID (*PTP_SIMPLE_CALLBACK)(PTP_CALLBACK_INSTANCE Instance, PVOID Context);

typedef struct _TP_POOL TP_POOL, *PTP_POOL;

typedef struct _TP_POOL_STACK_INFORMATION
{
	SIZE_T StackReserve;
	SIZE_T StackCommit;
} TP_POOL_STACK_INFORMATION, *PTP_POOL_STACK_INFORMATION;

typedef struct _TP_CLEANUP_GROUP TP_CLEANUP_GROUP, *PTP_CLEANUP_GROUP;

typedef VOID (*PTP_CLEANUP_GROUP_CANCEL_CALLBACK)(PVOID ObjectContext, PVOID CleanupContext);

typedef struct _TP_CALLBACK_ENVIRON_V1
{
	TP_VERSION Version;
	PTP_POOL Pool;
	PTP_CLEANUP_GROUP CleanupGroup;
	PTP_CLEANUP_GROUP_CANCEL_CALLBACK CleanupGroupCancelCallback;
	PVOID RaceDll;
	struct _ACTIVATION_CONTEXT* ActivationContext;
	PTP_SIMPLE_CALLBACK FinalizationCallback;

	union
	{
		DWORD Flags;
		struct
		{
			DWORD LongFunction:1;
			DWORD Persistent:1;
			DWORD Private:30;
		} s;
	} u;
} TP_CALLBACK_ENVIRON_V1;

typedef TP_CALLBACK_ENVIRON_V1 TP_CALLBACK_ENVIRON, *PTP_CALLBACK_ENVIRON;

#endif /* _WIN32 not defined */

typedef struct _TP_WORK TP_WORK, *PTP_WORK;
typedef struct _TP_TIMER TP_TIMER, *PTP_TIMER;

typedef DWORD TP_WAIT_RESULT;
typedef struct _TP_WAIT TP_WAIT, *PTP_WAIT;

typedef struct _TP_IO TP_IO, *PTP_IO;


#ifndef _WIN32

typedef VOID (*PTP_WORK_CALLBACK)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work);
typedef VOID (*PTP_TIMER_CALLBACK)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer);
typedef VOID (*PTP_WAIT_CALLBACK)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult);

#endif

/* 
There is a bug in the Win8 header that defines the IO 
callback unconditionally. Versions of Windows greater
than XP will conditionally define it. The following 
logic tries to fix that.
*/
#ifdef _THREADPOOLAPISET_H_
#define PTP_WIN32_IO_CALLBACK_DEFINED 1
#else
#if (_WIN32_WINNT >= 0x0600)
#define PTP_WIN32_IO_CALLBACK_DEFINED 1
#endif
#endif

#ifndef PTP_WIN32_IO_CALLBACK_DEFINED

typedef VOID (*PTP_WIN32_IO_CALLBACK)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped,
	ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io);

#endif

#if (!defined(_WIN32) || ((defined(_WIN32) && (_WIN32_WINNT < 0x0600))))
#define WINPR_THREAD_POOL	1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Synch */

#ifdef WINPR_THREAD_POOL

WINPR_API PTP_WAIT winpr_CreateThreadpoolWait(PTP_WAIT_CALLBACK pfnwa, PVOID pv, PTP_CALLBACK_ENVIRON pcbe);
WINPR_API VOID winpr_CloseThreadpoolWait(PTP_WAIT pwa);
WINPR_API VOID winpr_SetThreadpoolWait(PTP_WAIT pwa, HANDLE h, PFILETIME pftTimeout);
WINPR_API VOID winpr_WaitForThreadpoolWaitCallbacks(PTP_WAIT pwa, BOOL fCancelPendingCallbacks);

#define CreateThreadpoolWait winpr_CreateThreadpoolWait
#define CloseThreadpoolWait winpr_CloseThreadpoolWait
#define SetThreadpoolWait winpr_SetThreadpoolWait
#define WaitForThreadpoolWaitCallbacks winpr_WaitForThreadpoolWaitCallbacks

/* Work */

WINPR_API PTP_WORK winpr_CreateThreadpoolWork(PTP_WORK_CALLBACK pfnwk, PVOID pv, PTP_CALLBACK_ENVIRON pcbe);
WINPR_API VOID winpr_CloseThreadpoolWork(PTP_WORK pwk);
WINPR_API VOID winpr_SubmitThreadpoolWork(PTP_WORK pwk);
WINPR_API BOOL winpr_TrySubmitThreadpoolCallback(PTP_SIMPLE_CALLBACK pfns, PVOID pv, PTP_CALLBACK_ENVIRON pcbe);
WINPR_API VOID winpr_WaitForThreadpoolWorkCallbacks(PTP_WORK pwk, BOOL fCancelPendingCallbacks);

#define CreateThreadpoolWork winpr_CreateThreadpoolWork
#define CloseThreadpoolWork winpr_CloseThreadpoolWork
#define SubmitThreadpoolWork winpr_SubmitThreadpoolWork
#define TrySubmitThreadpoolCallback winpr_TrySubmitThreadpoolCallback
#define WaitForThreadpoolWorkCallbacks winpr_WaitForThreadpoolWorkCallbacks

/* Timer */

WINPR_API PTP_TIMER winpr_CreateThreadpoolTimer(PTP_TIMER_CALLBACK pfnti, PVOID pv, PTP_CALLBACK_ENVIRON pcbe);
WINPR_API VOID winpr_CloseThreadpoolTimer(PTP_TIMER pti);
WINPR_API BOOL winpr_IsThreadpoolTimerSet(PTP_TIMER pti);
WINPR_API VOID winpr_SetThreadpoolTimer(PTP_TIMER pti, PFILETIME pftDueTime, DWORD msPeriod, DWORD msWindowLength);
WINPR_API VOID winpr_WaitForThreadpoolTimerCallbacks(PTP_TIMER pti, BOOL fCancelPendingCallbacks);

#define CreateThreadpoolTimer winpr_CreateThreadpoolTimer
#define CloseThreadpoolTimer winpr_CloseThreadpoolTimer
#define IsThreadpoolTimerSet winpr_IsThreadpoolTimerSet
#define SetThreadpoolTimer winpr_SetThreadpoolTimer
#define WaitForThreadpoolTimerCallbacks winpr_WaitForThreadpoolTimerCallbacks

/* I/O */

WINPR_API PTP_IO winpr_CreateThreadpoolIo(HANDLE fl, PTP_WIN32_IO_CALLBACK pfnio, PVOID pv, PTP_CALLBACK_ENVIRON pcbe);
WINPR_API VOID winpr_CloseThreadpoolIo(PTP_IO pio);
WINPR_API VOID winpr_StartThreadpoolIo(PTP_IO pio);
WINPR_API VOID winpr_CancelThreadpoolIo(PTP_IO pio);
WINPR_API VOID winpr_WaitForThreadpoolIoCallbacks(PTP_IO pio, BOOL fCancelPendingCallbacks);

#define CreateThreadpoolIo winpr_CreateThreadpoolIo
#define CloseThreadpoolIo winpr_CloseThreadpoolIo
#define StartThreadpoolIo winpr_StartThreadpoolIo
#define CancelThreadpoolIo winpr_CancelThreadpoolIo
#define WaitForThreadpoolIoCallbacks winpr_WaitForThreadpoolIoCallbacks

/* Clean-up Group */

WINPR_API PTP_CLEANUP_GROUP winpr_CreateThreadpoolCleanupGroup(void);
WINPR_API VOID winpr_CloseThreadpoolCleanupGroupMembers(PTP_CLEANUP_GROUP ptpcg, BOOL fCancelPendingCallbacks, PVOID pvCleanupContext);
WINPR_API VOID winpr_CloseThreadpoolCleanupGroup(PTP_CLEANUP_GROUP ptpcg);

#define CreateThreadpoolCleanupGroup winpr_CreateThreadpoolCleanupGroup
#define CloseThreadpoolCleanupGroupMembers winpr_CloseThreadpoolCleanupGroupMembers
#define CloseThreadpoolCleanupGroup winpr_CloseThreadpoolCleanupGroup

/* Pool */

WINPR_API PTP_POOL winpr_CreateThreadpool(PVOID reserved);
WINPR_API VOID winpr_CloseThreadpool(PTP_POOL ptpp);
WINPR_API BOOL winpr_SetThreadpoolThreadMinimum(PTP_POOL ptpp, DWORD cthrdMic);
WINPR_API VOID winpr_SetThreadpoolThreadMaximum(PTP_POOL ptpp, DWORD cthrdMost);

#define CreateThreadpool winpr_CreateThreadpool
#define CloseThreadpool winpr_CloseThreadpool
#define SetThreadpoolThreadMinimum winpr_SetThreadpoolThreadMinimum
#define SetThreadpoolThreadMaximum winpr_SetThreadpoolThreadMaximum

/* Callback Environment */

static INLINE VOID InitializeThreadpoolEnvironment(PTP_CALLBACK_ENVIRON pcbe)
{
	pcbe->Version = 1;
	pcbe->Pool = NULL;
	pcbe->CleanupGroup = NULL;
	pcbe->CleanupGroupCancelCallback = NULL;
	pcbe->RaceDll = NULL;
	pcbe->ActivationContext = NULL;
	pcbe->FinalizationCallback = NULL;
	pcbe->u.Flags = 0;
}

static INLINE VOID DestroyThreadpoolEnvironment(PTP_CALLBACK_ENVIRON pcbe)
{
	/* no actions, this may change in a future release. */
}

static INLINE VOID SetThreadpoolCallbackPool(PTP_CALLBACK_ENVIRON pcbe, PTP_POOL ptpp)
{
	pcbe->Pool = ptpp;
}

static INLINE VOID SetThreadpoolCallbackCleanupGroup(PTP_CALLBACK_ENVIRON pcbe, PTP_CLEANUP_GROUP ptpcg, PTP_CLEANUP_GROUP_CANCEL_CALLBACK pfng)
{
	pcbe->CleanupGroup = ptpcg;
	pcbe->CleanupGroupCancelCallback = pfng;
}

static INLINE VOID SetThreadpoolCallbackRunsLong(PTP_CALLBACK_ENVIRON pcbe)
{
	pcbe->u.s.LongFunction = 1;
}

static INLINE VOID SetThreadpoolCallbackLibrary(PTP_CALLBACK_ENVIRON pcbe, PVOID mod)
{
	pcbe->RaceDll = mod;
}


/* Callback */

WINPR_API BOOL winpr_CallbackMayRunLong(PTP_CALLBACK_INSTANCE pci);

/* Callback Clean-up */

WINPR_API VOID winpr_SetEventWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE evt);
WINPR_API VOID winpr_ReleaseSemaphoreWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE sem, DWORD crel);
WINPR_API VOID winpr_ReleaseMutexWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE mut);
WINPR_API VOID winpr_LeaveCriticalSectionWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, PCRITICAL_SECTION pcs);
WINPR_API VOID winpr_FreeLibraryWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HMODULE mod);
WINPR_API VOID winpr_DisassociateCurrentThreadFromCallback(PTP_CALLBACK_INSTANCE pci);

#define SetEventWhenCallbackReturns winpr_SetEventWhenCallbackReturns
#define ReleaseSemaphoreWhenCallbackReturns winpr_ReleaseSemaphoreWhenCallbackReturns
#define ReleaseMutexWhenCallbackReturns winpr_ReleaseMutexWhenCallbackReturns
#define LeaveCriticalSectionWhenCallbackReturns winpr_LeaveCriticalSectionWhenCallbackReturns
#define FreeLibraryWhenCallbackReturns winpr_FreeLibraryWhenCallbackReturns
#define DisassociateCurrentThreadFromCallback winpr_DisassociateCurrentThreadFromCallback

#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_POOL_H */
