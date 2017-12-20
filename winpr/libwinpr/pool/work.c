/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Work)
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
#include "../log.h"
#define TAG WINPR_TAG("pool")

#ifdef WINPR_THREAD_POOL

#ifdef _WIN32
static INIT_ONCE init_once_module = INIT_ONCE_STATIC_INIT;
static PTP_WORK(WINAPI* pCreateThreadpoolWork)(PTP_WORK_CALLBACK pfnwk, PVOID pv,
        PTP_CALLBACK_ENVIRON pcbe);
static VOID (WINAPI* pCloseThreadpoolWork)(PTP_WORK pwk);
static VOID (WINAPI* pSubmitThreadpoolWork)(PTP_WORK pwk);
static BOOL (WINAPI* pTrySubmitThreadpoolCallback)(PTP_SIMPLE_CALLBACK pfns, PVOID pv,
        PTP_CALLBACK_ENVIRON pcbe);
static VOID (WINAPI* pWaitForThreadpoolWorkCallbacks)(PTP_WORK pwk, BOOL fCancelPendingCallbacks);

static BOOL CALLBACK init_module(PINIT_ONCE once, PVOID param, PVOID* context)
{
	HMODULE kernel32 = LoadLibraryA("kernel32.dll");

	if (kernel32)
	{
		pCreateThreadpoolWork = (void*)GetProcAddress(kernel32, "CreateThreadpoolWork");
		pCloseThreadpoolWork = (void*)GetProcAddress(kernel32, "CloseThreadpoolWork");
		pSubmitThreadpoolWork = (void*)GetProcAddress(kernel32, "SubmitThreadpoolWork");
		pTrySubmitThreadpoolCallback = (void*)GetProcAddress(kernel32, "TrySubmitThreadpoolCallback");
		pWaitForThreadpoolWorkCallbacks = (void*)GetProcAddress(kernel32, "WaitForThreadpoolWorkCallbacks");
	}

	return TRUE;
}
#endif

static TP_CALLBACK_ENVIRON DEFAULT_CALLBACK_ENVIRONMENT =
{
	1, /* Version */
	NULL, /* Pool */
	NULL, /* CleanupGroup */
	NULL, /* CleanupGroupCancelCallback */
	NULL, /* RaceDll */
	NULL, /* ActivationContext */
	NULL, /* FinalizationCallback */
	{ 0 } /* Flags */
};

PTP_WORK winpr_CreateThreadpoolWork(PTP_WORK_CALLBACK pfnwk, PVOID pv, PTP_CALLBACK_ENVIRON pcbe)
{
	PTP_WORK work = NULL;
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);

	if (pCreateThreadpoolWork)
		return pCreateThreadpoolWork(pfnwk, pv, pcbe);

#endif
	work = (PTP_WORK) calloc(1, sizeof(TP_WORK));

	if (work)
	{
		if (!pcbe)
		{
			pcbe = &DEFAULT_CALLBACK_ENVIRONMENT;
			pcbe->Pool = GetDefaultThreadpool();
		}

		work->CallbackEnvironment = pcbe;
		work->WorkCallback = pfnwk;
		work->CallbackParameter = pv;
#ifndef _WIN32

		if (pcbe->CleanupGroup)
			ArrayList_Add(pcbe->CleanupGroup->groups, work);

#endif
	}

	return work;
}

VOID winpr_CloseThreadpoolWork(PTP_WORK pwk)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);

	if (pCloseThreadpoolWork)
	{
		pCloseThreadpoolWork(pwk);
		return;
	}

#else

	if (pwk->CallbackEnvironment->CleanupGroup)
		ArrayList_Remove(pwk->CallbackEnvironment->CleanupGroup->groups, pwk);

#endif
	free(pwk);
}

VOID winpr_SubmitThreadpoolWork(PTP_WORK pwk)
{
	PTP_POOL pool;
	PTP_CALLBACK_INSTANCE callbackInstance;
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);

	if (pSubmitThreadpoolWork)
	{
		pSubmitThreadpoolWork(pwk);
		return;
	}

#endif
	pool = pwk->CallbackEnvironment->Pool;
	callbackInstance = (PTP_CALLBACK_INSTANCE) calloc(1, sizeof(TP_CALLBACK_INSTANCE));

	if (callbackInstance)
	{
		callbackInstance->Work = pwk;
		CountdownEvent_AddCount(pool->WorkComplete, 1);
		Queue_Enqueue(pool->PendingQueue, callbackInstance);
	}
}

BOOL winpr_TrySubmitThreadpoolCallback(PTP_SIMPLE_CALLBACK pfns, PVOID pv,
                                       PTP_CALLBACK_ENVIRON pcbe)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);

	if (pTrySubmitThreadpoolCallback)
		return pTrySubmitThreadpoolCallback(pfns, pv, pcbe);

#endif
	WLog_ERR(TAG, "TrySubmitThreadpoolCallback is not implemented");
	return FALSE;
}

VOID winpr_WaitForThreadpoolWorkCallbacks(PTP_WORK pwk, BOOL fCancelPendingCallbacks)
{
	HANDLE event;
	PTP_POOL pool;
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);

	if (pWaitForThreadpoolWorkCallbacks)
	{
		pWaitForThreadpoolWorkCallbacks(pwk, fCancelPendingCallbacks);
		return;
	}

#endif
	pool = pwk->CallbackEnvironment->Pool;
	event = CountdownEvent_WaitHandle(pool->WorkComplete);

	if (WaitForSingleObject(event, INFINITE) != WAIT_OBJECT_0)
		WLog_ERR(TAG, "error waiting on work completion");
}

#endif /* WINPR_THREAD_POOL defined */
