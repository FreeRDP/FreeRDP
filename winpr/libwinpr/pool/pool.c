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

#include "pool.h"

#ifdef _WIN32

static BOOL module_initialized = FALSE;
static BOOL module_available = FALSE;
static HMODULE kernel32_module = NULL;

static PTP_POOL (WINAPI * pCreateThreadpool)(PVOID reserved);
static VOID (WINAPI * pCloseThreadpool)(PTP_POOL ptpp);
static BOOL (WINAPI * pSetThreadpoolThreadMinimum)(PTP_POOL ptpp, DWORD cthrdMic);
static VOID (WINAPI * pSetThreadpoolThreadMaximum)(PTP_POOL ptpp, DWORD cthrdMost);

static void module_init()
{
	if (module_initialized)
		return;

	kernel32_module = LoadLibraryA("kernel32.dll");
	module_initialized = TRUE;

	if (!kernel32_module)
		return;

	module_available = TRUE;

	pCreateThreadpool = (void*) GetProcAddress(kernel32_module, "CreateThreadpool");
	pCloseThreadpool = (void*) GetProcAddress(kernel32_module, "CloseThreadpool");
	pSetThreadpoolThreadMinimum = (void*) GetProcAddress(kernel32_module, "SetThreadpoolThreadMinimum");
	pSetThreadpoolThreadMaximum = (void*) GetProcAddress(kernel32_module, "SetThreadpoolThreadMaximum");
}

#else

static TP_POOL DEFAULT_POOL =
{
	0, /* Minimum */
	500, /* Maximum */
	NULL, /* Threads */
	0, /* ThreadCount */
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

	return NULL;
}

static void threads_close(void *thread)
{
	CloseHandle(thread);
}

void InitializeThreadpool(PTP_POOL pool)
{
	int index;
	HANDLE thread;

	if (!pool->Threads)
	{
		pool->Minimum = 0;
		pool->Maximum = 500;

		pool->Threads = ArrayList_New(TRUE);
		pool->Threads->object.fnObjectFree = threads_close;

		pool->PendingQueue = Queue_New(TRUE, -1, -1);
		pool->WorkComplete = CountdownEvent_New(0);

		pool->TerminateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		for (index = 0; index < 4; index++)
		{
			thread = CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE) thread_pool_work_func,
					(void*) pool, 0, NULL);

			ArrayList_Add(pool->Threads, thread);
		}
	}
}

PTP_POOL GetDefaultThreadpool()
{
	PTP_POOL pool = NULL;

	pool = &DEFAULT_POOL;

	InitializeThreadpool(pool);

	return pool;
}

#endif

PTP_POOL CreateThreadpool(PVOID reserved)
{
	PTP_POOL pool = NULL;

#ifdef _WIN32
	module_init();

	if (pCreateThreadpool)
		return pCreateThreadpool(reserved);
#else
	pool = (PTP_POOL) calloc(1, sizeof(TP_POOL));

	if (pool)
		InitializeThreadpool(pool);
#endif

	return pool;
}

VOID CloseThreadpool(PTP_POOL ptpp)
{
#ifdef _WIN32
	module_init();

	if (pCloseThreadpool)
		pCloseThreadpool(ptpp);
#else
	int index;
	HANDLE thread;

	SetEvent(ptpp->TerminateEvent);

	index = ArrayList_Count(ptpp->Threads) - 1;

	while (index >= 0)
	{
		thread = (HANDLE) ArrayList_GetItem(ptpp->Threads, index);
		WaitForSingleObject(thread, INFINITE);
		index--;
	}

	ArrayList_Free(ptpp->Threads);
	Queue_Free(ptpp->PendingQueue);
	CountdownEvent_Free(ptpp->WorkComplete);
	CloseHandle(ptpp->TerminateEvent);

	free(ptpp);
#endif
}

BOOL SetThreadpoolThreadMinimum(PTP_POOL ptpp, DWORD cthrdMic)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolThreadMinimum)
		return pSetThreadpoolThreadMinimum(ptpp, cthrdMic);
#else
	HANDLE thread;

	ptpp->Minimum = cthrdMic;

	while (ArrayList_Count(ptpp->Threads) < ptpp->Minimum)
	{
		thread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) thread_pool_work_func,
				(void*) ptpp, 0, NULL);

		ArrayList_Add(ptpp->Threads, thread);
	}
#endif
	return TRUE;
}

VOID SetThreadpoolThreadMaximum(PTP_POOL ptpp, DWORD cthrdMost)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolThreadMaximum)
		pSetThreadpoolThreadMaximum(ptpp, cthrdMost);
#else
	ptpp->Maximum = cthrdMost;
#endif
}

/* dummy */

void winpr_pool_dummy()
{

}
