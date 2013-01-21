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

#include "pool.h"

#ifdef _WIN32

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

VOID InitializeThreadpoolEnvironment(PTP_CALLBACK_ENVIRON pcbe)
{
#ifdef _WIN32
#else
	pcbe->Version = 1;
	pcbe->Pool = NULL;
	pcbe->CleanupGroup = NULL;
	pcbe->CleanupGroupCancelCallback = NULL;
	pcbe->RaceDll = NULL;
	pcbe->ActivationContext = NULL;
	pcbe->FinalizationCallback = NULL;
	pcbe->u.s.LongFunction = FALSE;
	pcbe->u.s.Persistent = FALSE;
	pcbe->u.s.Private = 0;
#endif
}

VOID DestroyThreadpoolEnvironment(PTP_CALLBACK_ENVIRON pcbe)
{
#ifdef _WIN32
#else
#endif
}

VOID SetThreadpoolCallbackPool(PTP_CALLBACK_ENVIRON pcbe, PTP_POOL ptpp)
{
#ifdef _WIN32
#else
	pcbe->Pool = ptpp;
#endif
}

VOID SetThreadpoolCallbackCleanupGroup(PTP_CALLBACK_ENVIRON pcbe, PTP_CLEANUP_GROUP ptpcg, PTP_CLEANUP_GROUP_CANCEL_CALLBACK pfng)
{
#ifdef _WIN32
#else
	pcbe->CleanupGroup = ptpcg;
	pcbe->CleanupGroupCancelCallback = pfng;
#endif
}

VOID SetThreadpoolCallbackRunsLong(PTP_CALLBACK_ENVIRON pcbe)
{
#ifdef _WIN32
#else
	pcbe->u.s.LongFunction = TRUE;
#endif
}

VOID SetThreadpoolCallbackLibrary(PTP_CALLBACK_ENVIRON pcbe, PVOID mod)
{
#ifdef _WIN32
#else
#endif
}

VOID SetThreadpoolCallbackPriority(PTP_CALLBACK_ENVIRON pcbe, TP_CALLBACK_PRIORITY Priority)
{
#ifdef _WIN32
#else
#endif
}
