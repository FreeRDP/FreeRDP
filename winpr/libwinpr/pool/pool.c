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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/pool.h>
#include <winpr/library.h>

#include "pool.h"

#ifdef WINPR_THREAD_POOL

#ifdef _WIN32
static INIT_ONCE init_once_module = INIT_ONCE_STATIC_INIT;
static PTP_POOL (WINAPI * pCreateThreadpool)(PVOID reserved);
static VOID (WINAPI * pCloseThreadpool)(PTP_POOL ptpp);
static BOOL (WINAPI * pSetThreadpoolThreadMinimum)(PTP_POOL ptpp, DWORD cthrdMic);
static VOID (WINAPI * pSetThreadpoolThreadMaximum)(PTP_POOL ptpp, DWORD cthrdMost);

static BOOL CALLBACK init_module(PINIT_ONCE once, PVOID param, PVOID *context)
{
	HMODULE kernel32 = LoadLibraryA("kernel32.dll");
	if (kernel32)
	{
		pCreateThreadpool = (void*)GetProcAddress(kernel32, "CreateThreadpool");
		pCloseThreadpool = (void*)GetProcAddress(kernel32, "CloseThreadpool");
		pSetThreadpoolThreadMinimum = (void*)GetProcAddress(kernel32, "SetThreadpoolThreadMinimum");
		pSetThreadpoolThreadMaximum = (void*)GetProcAddress(kernel32, "SetThreadpoolThreadMaximum");
	}
	return TRUE;
}
#endif

static TP_POOL DEFAULT_POOL =
{
	0,    /* DWORD Minimum */
	500,  /* DWORD Maximum */
	NULL, /* wArrayList* Threads */
	NULL, /* wQueue* PendingQueue */
	NULL, /* HANDLE TerminateEvent */
	NULL, /* wCountdownEvent* WorkComplete */
};

static void* thread_pool_work_func(void* arg)
{
	DWORD status;
	PTP_POOL pool;
	PTP_WORK work;
	HANDLE events[2];
	PTP_CALLBACK_INSTANCE callbackInstance;

	pool = (PTP_POOL) arg;

	events[0] = pool->TerminateEvent;
	events[1] = Queue_Event(pool->PendingQueue);

	while (1)
	{
		status = WaitForMultipleObjects(2, events, FALSE, INFINITE);

		if (status == WAIT_OBJECT_0)
			break;

		if (status != (WAIT_OBJECT_0 + 1))
			break;

		callbackInstance = (PTP_CALLBACK_INSTANCE) Queue_Dequeue(pool->PendingQueue);

		if (callbackInstance)
		{
			work = callbackInstance->Work;
			work->WorkCallback(callbackInstance, work->CallbackParameter, work);
			CountdownEvent_Signal(pool->WorkComplete, 1);
			free(callbackInstance);
		}
	}

	ExitThread(0);
	return NULL;
}

static void threads_close(void *thread)
{
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
}

static BOOL InitializeThreadpool(PTP_POOL pool)
{
	int index;
	HANDLE thread;

	if (pool->Threads)
		return TRUE;

	pool->Minimum = 0;
	pool->Maximum = 500;

	if (!(pool->PendingQueue = Queue_New(TRUE, -1, -1)))
		goto fail_queue_new;

	if (!(pool->WorkComplete = CountdownEvent_New(0)))
		goto fail_countdown_event;

	if (!(pool->TerminateEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail_terminate_event;

	if (!(pool->Threads = ArrayList_New(TRUE)))
		goto fail_thread_array;

	pool->Threads->object.fnObjectFree = threads_close;

	for (index = 0; index < 4; index++)
	{
		if (!(thread = CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE) thread_pool_work_func,
					(void*) pool, 0, NULL)))
		{
			goto fail_create_threads;
		}

		if (ArrayList_Add(pool->Threads, thread) < 0)
			goto fail_create_threads;
	}

	return TRUE;

fail_create_threads:
	SetEvent(pool->TerminateEvent);
	ArrayList_Free(pool->Threads);
	pool->Threads = NULL;
fail_thread_array:
	CloseHandle(pool->TerminateEvent);
	pool->TerminateEvent = NULL;
fail_terminate_event:
	CountdownEvent_Free(pool->WorkComplete);
	pool->WorkComplete = NULL;
fail_countdown_event:
	Queue_Free(pool->PendingQueue);
	pool->WorkComplete = NULL;
fail_queue_new:

	return FALSE;
}

PTP_POOL GetDefaultThreadpool()
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
#endif
	if (!(pool = (PTP_POOL) calloc(1, sizeof(TP_POOL))))
		return NULL;

	if (!InitializeThreadpool(pool))
	{
		free(pool);
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
	SetEvent(ptpp->TerminateEvent);

	ArrayList_Free(ptpp->Threads);
	Queue_Free(ptpp->PendingQueue);
	CountdownEvent_Free(ptpp->WorkComplete);
	CloseHandle(ptpp->TerminateEvent);

	if (ptpp == &DEFAULT_POOL)
	{
		ptpp->Threads = NULL;
		ptpp->PendingQueue = NULL;
		ptpp->WorkComplete = NULL;
		ptpp->TerminateEvent = NULL;
	}
	else
	{
		free(ptpp);
	}
}

BOOL winpr_SetThreadpoolThreadMinimum(PTP_POOL ptpp, DWORD cthrdMic)
{
	HANDLE thread;
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pSetThreadpoolThreadMinimum)
		return pSetThreadpoolThreadMinimum(ptpp, cthrdMic);
#endif
	ptpp->Minimum = cthrdMic;

	while (ArrayList_Count(ptpp->Threads) < ptpp->Minimum)
	{
		if (!(thread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) thread_pool_work_func,
				(void*) ptpp, 0, NULL)))
		{
			return FALSE;
		}

		if (ArrayList_Add(ptpp->Threads, thread) < 0)
			return FALSE;
	}

	return TRUE;
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
}

#endif /* WINPR_THREAD_POOL defined */
