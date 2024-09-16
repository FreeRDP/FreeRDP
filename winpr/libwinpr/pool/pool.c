/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Pool)
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/sysinfo.h>
#include <winpr/pool.h>
#include <winpr/library.h>

#include "pool.h"

#ifdef WINPR_THREAD_POOL

#ifdef _WIN32
static INIT_ONCE init_once_module = INIT_ONCE_STATIC_INIT;
static PTP_POOL(WINAPI* pCreateThreadpool)(PVOID reserved);
static VOID(WINAPI* pCloseThreadpool)(PTP_POOL ptpp);
static BOOL(WINAPI* pSetThreadpoolThreadMinimum)(PTP_POOL ptpp, DWORD cthrdMic);
static VOID(WINAPI* pSetThreadpoolThreadMaximum)(PTP_POOL ptpp, DWORD cthrdMost);

static BOOL CALLBACK init_module(PINIT_ONCE once, PVOID param, PVOID* context)
{
	HMODULE kernel32 = LoadLibraryA("kernel32.dll");
	if (kernel32)
	{
		pCreateThreadpool = GetProcAddressAs(kernel32, "CreateThreadpool", void*);
		pCloseThreadpool = GetProcAddressAs(kernel32, "CloseThreadpool", void*);
		pSetThreadpoolThreadMinimum =
		    GetProcAddressAs(kernel32, "SetThreadpoolThreadMinimum", void*);
		pSetThreadpoolThreadMaximum =
		    GetProcAddressAs(kernel32, "SetThreadpoolThreadMaximum", void*);
	}
	return TRUE;
}
#endif

static TP_POOL DEFAULT_POOL = {
	0,    /* DWORD Minimum */
	500,  /* DWORD Maximum */
	NULL, /* wArrayList* Threads */
	NULL, /* wQueue* PendingQueue */
	NULL, /* HANDLE TerminateEvent */
	NULL, /* wCountdownEvent* WorkComplete */
};

static DWORD WINAPI thread_pool_work_func(LPVOID arg)
{
	DWORD status = 0;
	PTP_POOL pool = NULL;
	PTP_WORK work = NULL;
	HANDLE events[2];
	PTP_CALLBACK_INSTANCE callbackInstance = NULL;

	pool = (PTP_POOL)arg;

	events[0] = pool->TerminateEvent;
	events[1] = Queue_Event(pool->PendingQueue);

	while (1)
	{
		status = WaitForMultipleObjects(2, events, FALSE, INFINITE);

		if (status == WAIT_OBJECT_0)
			break;

		if (status != (WAIT_OBJECT_0 + 1))
			break;

		callbackInstance = (PTP_CALLBACK_INSTANCE)Queue_Dequeue(pool->PendingQueue);

		if (callbackInstance)
		{
			work = callbackInstance->Work;
			work->WorkCallback(callbackInstance, work->CallbackParameter, work);
			CountdownEvent_Signal(pool->WorkComplete, 1);
			free(callbackInstance);
		}
	}

	ExitThread(0);
	return 0;
}

static void threads_close(void* thread)
{
	(void)WaitForSingleObject(thread, INFINITE);
	(void)CloseHandle(thread);
}

static BOOL InitializeThreadpool(PTP_POOL pool)
{
	BOOL rc = FALSE;
	wObject* obj = NULL;

	if (pool->Threads)
		return TRUE;

	if (!(pool->PendingQueue = Queue_New(TRUE, -1, -1)))
		goto fail;

	if (!(pool->WorkComplete = CountdownEvent_New(0)))
		goto fail;

	if (!(pool->TerminateEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail;

	if (!(pool->Threads = ArrayList_New(TRUE)))
		goto fail;

	obj = ArrayList_Object(pool->Threads);
	obj->fnObjectFree = threads_close;

	SYSTEM_INFO info = { 0 };
	GetSystemInfo(&info);
	if (info.dwNumberOfProcessors < 1)
		info.dwNumberOfProcessors = 1;
	if (!SetThreadpoolThreadMinimum(pool, info.dwNumberOfProcessors))
		goto fail;
	SetThreadpoolThreadMaximum(pool, info.dwNumberOfProcessors);

	rc = TRUE;

fail:
	return rc;
}

PTP_POOL GetDefaultThreadpool(void)
{
	PTP_POOL pool = NULL;

	pool = &DEFAULT_POOL;

	if (!InitializeThreadpool(pool))
		return NULL;

	return pool;
}

PTP_POOL winpr_CreateThreadpool(PVOID reserved)
{
	PTP_POOL pool = NULL;
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pCreateThreadpool)
		return pCreateThreadpool(reserved);
#else
	WINPR_UNUSED(reserved);
#endif
	if (!(pool = (PTP_POOL)calloc(1, sizeof(TP_POOL))))
		return NULL;

	if (!InitializeThreadpool(pool))
	{
		winpr_CloseThreadpool(pool);
		return NULL;
	}

	return pool;
}

VOID winpr_CloseThreadpool(PTP_POOL ptpp)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pCloseThreadpool)
	{
		pCloseThreadpool(ptpp);
		return;
	}
#endif
	(void)SetEvent(ptpp->TerminateEvent);

	ArrayList_Free(ptpp->Threads);
	Queue_Free(ptpp->PendingQueue);
	CountdownEvent_Free(ptpp->WorkComplete);
	(void)CloseHandle(ptpp->TerminateEvent);

	{
		TP_POOL empty = { 0 };
		*ptpp = empty;
	}

	if (ptpp != &DEFAULT_POOL)
		free(ptpp);
}

BOOL winpr_SetThreadpoolThreadMinimum(PTP_POOL ptpp, DWORD cthrdMic)
{
	BOOL rc = FALSE;
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pSetThreadpoolThreadMinimum)
		return pSetThreadpoolThreadMinimum(ptpp, cthrdMic);
#endif
	ptpp->Minimum = cthrdMic;

	ArrayList_Lock(ptpp->Threads);
	while (ArrayList_Count(ptpp->Threads) < ptpp->Minimum)
	{
		HANDLE thread = CreateThread(NULL, 0, thread_pool_work_func, (void*)ptpp, 0, NULL);
		if (!thread)
			goto fail;

		if (!ArrayList_Append(ptpp->Threads, thread))
		{
			(void)CloseHandle(thread);
			goto fail;
		}
	}

	rc = TRUE;
fail:
	ArrayList_Unlock(ptpp->Threads);

	return rc;
}

VOID winpr_SetThreadpoolThreadMaximum(PTP_POOL ptpp, DWORD cthrdMost)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pSetThreadpoolThreadMaximum)
	{
		pSetThreadpoolThreadMaximum(ptpp, cthrdMost);
		return;
	}
#endif
	ptpp->Maximum = cthrdMost;

	ArrayList_Lock(ptpp->Threads);
	if (ArrayList_Count(ptpp->Threads) > ptpp->Maximum)
	{
		(void)SetEvent(ptpp->TerminateEvent);
		ArrayList_Clear(ptpp->Threads);
		(void)ResetEvent(ptpp->TerminateEvent);
	}
	ArrayList_Unlock(ptpp->Threads);
	winpr_SetThreadpoolThreadMinimum(ptpp, ptpp->Minimum);
}

#endif /* WINPR_THREAD_POOL defined */
