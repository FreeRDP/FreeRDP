/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Clean-up Group)
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
static PTP_CLEANUP_GROUP(WINAPI* pCreateThreadpoolCleanupGroup)();
static VOID (WINAPI* pCloseThreadpoolCleanupGroupMembers)(PTP_CLEANUP_GROUP ptpcg,
        BOOL fCancelPendingCallbacks, PVOID pvCleanupContext);
static VOID (WINAPI* pCloseThreadpoolCleanupGroup)(PTP_CLEANUP_GROUP ptpcg);

static BOOL CALLBACK init_module(PINIT_ONCE once, PVOID param, PVOID* context)
{
	HMODULE kernel32 = LoadLibraryA("kernel32.dll");

	if (kernel32)
	{
		pCreateThreadpoolCleanupGroup = (void*)GetProcAddress(kernel32, "CreateThreadpoolCleanupGroup");
		pCloseThreadpoolCleanupGroupMembers = (void*)GetProcAddress(kernel32,
		                                      "CloseThreadpoolCleanupGroupMembers");
		pCloseThreadpoolCleanupGroup = (void*)GetProcAddress(kernel32, "CloseThreadpoolCleanupGroup");
	}

	return TRUE;
}
#endif

PTP_CLEANUP_GROUP winpr_CreateThreadpoolCleanupGroup(void)
{
	PTP_CLEANUP_GROUP cleanupGroup = NULL;
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);

	if (pCreateThreadpoolCleanupGroup)
		return pCreateThreadpoolCleanupGroup();

	return cleanupGroup;
#else
	cleanupGroup = (PTP_CLEANUP_GROUP) calloc(1, sizeof(TP_CLEANUP_GROUP));

	if (!cleanupGroup)
		return NULL;

	cleanupGroup->groups = ArrayList_New(FALSE);

	if (!cleanupGroup->groups)
	{
		free(cleanupGroup);
		return NULL;
	}

	return cleanupGroup;
#endif
}

VOID winpr_SetThreadpoolCallbackCleanupGroup(PTP_CALLBACK_ENVIRON pcbe, PTP_CLEANUP_GROUP ptpcg,
        PTP_CLEANUP_GROUP_CANCEL_CALLBACK pfng)
{
	pcbe->CleanupGroup = ptpcg;
	pcbe->CleanupGroupCancelCallback = pfng;
#ifndef _WIN32
	pcbe->CleanupGroup->env = pcbe;
#endif
}

VOID winpr_CloseThreadpoolCleanupGroupMembers(PTP_CLEANUP_GROUP ptpcg, BOOL fCancelPendingCallbacks,
        PVOID pvCleanupContext)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);

	if (pCloseThreadpoolCleanupGroupMembers)
	{
		pCloseThreadpoolCleanupGroupMembers(ptpcg, fCancelPendingCallbacks, pvCleanupContext);
		return;
	}

#else

	while (ArrayList_Count(ptpcg->groups) > 0)
	{
		PTP_WORK work = ArrayList_GetItem(ptpcg->groups, 0);
		winpr_CloseThreadpoolWork(work);
	}

#endif
}

VOID winpr_CloseThreadpoolCleanupGroup(PTP_CLEANUP_GROUP ptpcg)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);

	if (pCloseThreadpoolCleanupGroup)
	{
		pCloseThreadpoolCleanupGroup(ptpcg);
		return;
	}

#else

	if (ptpcg && ptpcg->groups)
		ArrayList_Free(ptpcg->groups);

	if (ptpcg && ptpcg->env)
		ptpcg->env->CleanupGroup = NULL;

	free(ptpcg);
#endif
}

#endif /* WINPR_THREAD_POOL defined */

