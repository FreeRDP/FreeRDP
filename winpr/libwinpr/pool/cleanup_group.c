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

#ifdef _WIN32

static BOOL module_initialized = FALSE;
static BOOL module_available = FALSE;
static HMODULE kernel32_module = NULL;

static PTP_CLEANUP_GROUP (WINAPI * pCreateThreadpoolCleanupGroup)();
static VOID (WINAPI * pCloseThreadpoolCleanupGroupMembers)(PTP_CLEANUP_GROUP ptpcg, BOOL fCancelPendingCallbacks, PVOID pvCleanupContext);
static VOID (WINAPI * pCloseThreadpoolCleanupGroup)(PTP_CLEANUP_GROUP ptpcg);

static void module_init()
{
	if (module_initialized)
		return;

	kernel32_module = LoadLibraryA("kernel32.dll");
	module_initialized = TRUE;

	if (!kernel32_module)
		return;

	module_available = TRUE;

	pCreateThreadpoolCleanupGroup = (void*) GetProcAddress(kernel32_module, "CreateThreadpoolCleanupGroup");
	pCloseThreadpoolCleanupGroupMembers = (void*) GetProcAddress(kernel32_module, "CloseThreadpoolCleanupGroupMembers");
	pCloseThreadpoolCleanupGroup = (void*) GetProcAddress(kernel32_module, "CloseThreadpoolCleanupGroup");
}

#endif

#if WINPR_THREAD_POOL

PTP_CLEANUP_GROUP CreateThreadpoolCleanupGroup()
{
	PTP_CLEANUP_GROUP cleanupGroup = NULL;
#ifdef _WIN32
	module_init();

	if (pCreateThreadpoolCleanupGroup)
		return pCreateThreadpoolCleanupGroup();
#else
	cleanupGroup = (PTP_CLEANUP_GROUP) malloc(sizeof(TP_CLEANUP_GROUP));
#endif
	return cleanupGroup;
}

VOID CloseThreadpoolCleanupGroupMembers(PTP_CLEANUP_GROUP ptpcg, BOOL fCancelPendingCallbacks, PVOID pvCleanupContext)
{
#ifdef _WIN32
	module_init();

	if (pCloseThreadpoolCleanupGroupMembers)
		pCloseThreadpoolCleanupGroupMembers(ptpcg, fCancelPendingCallbacks, pvCleanupContext);
#else

#endif
}

VOID CloseThreadpoolCleanupGroup(PTP_CLEANUP_GROUP ptpcg)
{
#ifdef _WIN32
	module_init();

	if (pCloseThreadpoolCleanupGroup)
		pCloseThreadpoolCleanupGroup(ptpcg);
#else
	free(ptpcg);
#endif
}

#endif

