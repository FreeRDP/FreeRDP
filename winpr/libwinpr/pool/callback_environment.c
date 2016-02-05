/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Callback Environment)
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

VOID InitializeCallbackEnvironment_V1(TP_CALLBACK_ENVIRON_V1* pcbe)
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

VOID InitializeCallbackEnvironment_V3(TP_CALLBACK_ENVIRON_V3* pcbe)
{
	pcbe->Version = 3;

	pcbe->Pool = NULL;
	pcbe->CleanupGroup = NULL;
	pcbe->CleanupGroupCancelCallback = NULL;
	pcbe->RaceDll = NULL;
	pcbe->ActivationContext = NULL;
	pcbe->FinalizationCallback = NULL;
	pcbe->u.Flags = 0;

	pcbe->CallbackPriority = TP_CALLBACK_PRIORITY_NORMAL;
	pcbe->Size = sizeof(TP_CALLBACK_ENVIRON);
}

#endif

#ifdef _WIN32

static BOOL module_initialized = FALSE;
static BOOL module_available = FALSE;
static HMODULE kernel32_module = NULL;

static VOID (WINAPI * pDestroyThreadpoolEnvironment)(PTP_CALLBACK_ENVIRON pcbe);
static VOID (WINAPI * pSetThreadpoolCallbackPool)(PTP_CALLBACK_ENVIRON pcbe, PTP_POOL ptpp);
static VOID (WINAPI * pSetThreadpoolCallbackCleanupGroup)(PTP_CALLBACK_ENVIRON pcbe, PTP_CLEANUP_GROUP ptpcg, PTP_CLEANUP_GROUP_CANCEL_CALLBACK pfng);
static VOID (WINAPI * pSetThreadpoolCallbackRunsLong)(PTP_CALLBACK_ENVIRON pcbe);
static VOID (WINAPI * pSetThreadpoolCallbackLibrary)(PTP_CALLBACK_ENVIRON pcbe, PVOID mod);
static VOID (WINAPI * pSetThreadpoolCallbackPriority)(PTP_CALLBACK_ENVIRON pcbe, TP_CALLBACK_PRIORITY Priority);

static void module_init()
{
	if (module_initialized)
		return;

	kernel32_module = LoadLibraryA("kernel32.dll");
	module_initialized = TRUE;

	if (!kernel32_module)
		return;

	module_available = TRUE;

	/* InitializeThreadpoolEnvironment is an inline function */
	pDestroyThreadpoolEnvironment = (void*) GetProcAddress(kernel32_module, "DestroyThreadpoolEnvironment");
	pSetThreadpoolCallbackPool = (void*) GetProcAddress(kernel32_module, "SetThreadpoolCallbackPool");
	pSetThreadpoolCallbackCleanupGroup = (void*) GetProcAddress(kernel32_module, "SetThreadpoolCallbackCleanupGroup");
	pSetThreadpoolCallbackRunsLong = (void*) GetProcAddress(kernel32_module, "SetThreadpoolCallbackRunsLong");
	pSetThreadpoolCallbackRunsLong = (void*) GetProcAddress(kernel32_module, "SetThreadpoolCallbackRunsLong");
	pSetThreadpoolCallbackLibrary = (void*) GetProcAddress(kernel32_module, "SetThreadpoolCallbackLibrary");
	pSetThreadpoolCallbackPriority = (void*) GetProcAddress(kernel32_module, "SetThreadpoolCallbackPriority");
}

#else

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

PTP_CALLBACK_ENVIRON GetDefaultThreadpoolEnvironment()
{
	PTP_CALLBACK_ENVIRON environment = &DEFAULT_CALLBACK_ENVIRONMENT;

	environment->Pool = GetDefaultThreadpool();

	return environment;
}

#endif

#ifdef WINPR_THREAD_POOL

VOID InitializeThreadpoolEnvironment(PTP_CALLBACK_ENVIRON pcbe)
{
	if (pcbe->Version == 3)
		InitializeCallbackEnvironment_V3((TP_CALLBACK_ENVIRON_V3*) pcbe);
	else
		InitializeCallbackEnvironment_V1(pcbe);
}

VOID DestroyThreadpoolEnvironment(PTP_CALLBACK_ENVIRON pcbe)
{
#ifdef _WIN32
	module_init();

	if (pDestroyThreadpoolEnvironment)
		pDestroyThreadpoolEnvironment(pcbe);
#else
#endif
}

VOID SetThreadpoolCallbackPool(PTP_CALLBACK_ENVIRON pcbe, PTP_POOL ptpp)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolCallbackPool)
		pSetThreadpoolCallbackPool(pcbe, ptpp);
#else
	pcbe->Pool = ptpp;
#endif
}

VOID SetThreadpoolCallbackCleanupGroup(PTP_CALLBACK_ENVIRON pcbe, PTP_CLEANUP_GROUP ptpcg, PTP_CLEANUP_GROUP_CANCEL_CALLBACK pfng)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolCallbackCleanupGroup)
		pSetThreadpoolCallbackCleanupGroup(pcbe, ptpcg, pfng);
#else
	pcbe->CleanupGroup = ptpcg;
	pcbe->CleanupGroupCancelCallback = pfng;
#endif
}

VOID SetThreadpoolCallbackRunsLong(PTP_CALLBACK_ENVIRON pcbe)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolCallbackRunsLong)
		pSetThreadpoolCallbackRunsLong(pcbe);
#else
	pcbe->u.s.LongFunction = TRUE;
#endif
}

VOID SetThreadpoolCallbackLibrary(PTP_CALLBACK_ENVIRON pcbe, PVOID mod)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolCallbackLibrary)
		pSetThreadpoolCallbackLibrary(pcbe, mod);
#else
#endif
}

VOID SetThreadpoolCallbackPriority(PTP_CALLBACK_ENVIRON pcbe, TP_CALLBACK_PRIORITY Priority)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolCallbackPriority)
		pSetThreadpoolCallbackPriority(pcbe, Priority);
#else
#endif
}

#endif
