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
	PTP_POOL pool;
	PTP_WORK work;
	PTP_CALLBACK_INSTANCE callbackInstance;

	pool = (PTP_POOL) arg;

	while (WaitForSingleObject(Queue_Event(pool->PendingQueue), INFINITE) == WAIT_OBJECT_0)
	{
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

PTP_POOL GetDefaultThreadpool()
{
	int index;
	PTP_POOL pool = NULL;

	pool = &DEFAULT_POOL;

	if (!pool->Threads)
	{
		pool->ThreadCount = 4;
		pool->Threads = (HANDLE*) malloc(pool->ThreadCount * sizeof(HANDLE));

		pool->PendingQueue = Queue_New(TRUE, -1, -1);
		pool->WorkComplete = CountdownEvent_New(0);

		for (index = 0; index < pool->ThreadCount; index++)
		{
			pool->Threads[index] = CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE) thread_pool_work_func,
					(void*) pool, 0, NULL);
		}
	}

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
	pool = (PTP_POOL) malloc(sizeof(TP_POOL));

	if (pool)
	{
		pool->Minimum = 0;
		pool->Maximum = 500;
	}
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
	ptpp->Minimum = cthrdMic;
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
