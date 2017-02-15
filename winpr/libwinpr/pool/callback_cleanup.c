/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Callback Clean-up)
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
static VOID (WINAPI * pSetEventWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE evt);
static VOID (WINAPI * pReleaseSemaphoreWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE sem, DWORD crel);
static VOID (WINAPI * pReleaseMutexWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE mut);
static VOID (WINAPI * pLeaveCriticalSectionWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, PCRITICAL_SECTION pcs);
static VOID (WINAPI * pFreeLibraryWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HMODULE mod);
static VOID (WINAPI * pDisassociateCurrentThreadFromCallback)(PTP_CALLBACK_INSTANCE pci);

static BOOL CALLBACK init_module(PINIT_ONCE once, PVOID param, PVOID *context)
{
	HMODULE kernel32 = LoadLibraryA("kernel32.dll");
	if (kernel32)
	{
		pSetEventWhenCallbackReturns = (void*)GetProcAddress(kernel32, "SetEventWhenCallbackReturns");
		pReleaseSemaphoreWhenCallbackReturns = (void*)GetProcAddress(kernel32, "ReleaseSemaphoreWhenCallbackReturns");
		pReleaseMutexWhenCallbackReturns = (void*)GetProcAddress(kernel32, "ReleaseMutexWhenCallbackReturns");
		pLeaveCriticalSectionWhenCallbackReturns = (void*)GetProcAddress(kernel32, "LeaveCriticalSectionWhenCallbackReturns");
		pFreeLibraryWhenCallbackReturns = (void*)GetProcAddress(kernel32, "FreeLibraryWhenCallbackReturns");
		pDisassociateCurrentThreadFromCallback = (void*)GetProcAddress(kernel32, "DisassociateCurrentThreadFromCallback");
	}
	return TRUE;
}
#endif

VOID SetEventWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE evt)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pSetEventWhenCallbackReturns)
	{
		pSetEventWhenCallbackReturns(pci, evt);
		return;
	}
#endif
	/* No default implementation */
}

VOID ReleaseSemaphoreWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE sem, DWORD crel)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pReleaseSemaphoreWhenCallbackReturns)
	{
		pReleaseSemaphoreWhenCallbackReturns(pci, sem, crel);
		return;
	}
#endif
	/* No default implementation */
}

VOID ReleaseMutexWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE mut)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pReleaseMutexWhenCallbackReturns)
	{
		pReleaseMutexWhenCallbackReturns(pci, mut);
		return;
	}
#endif
	/* No default implementation */
}

VOID LeaveCriticalSectionWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, PCRITICAL_SECTION pcs)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pLeaveCriticalSectionWhenCallbackReturns)
	{
		pLeaveCriticalSectionWhenCallbackReturns(pci, pcs);
	}
#endif
	/* No default implementation */
}

VOID FreeLibraryWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HMODULE mod)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pFreeLibraryWhenCallbackReturns)
	{
		pFreeLibraryWhenCallbackReturns(pci, mod);
		return;
	}
#endif
	/* No default implementation */
}

VOID DisassociateCurrentThreadFromCallback(PTP_CALLBACK_INSTANCE pci)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pDisassociateCurrentThreadFromCallback)
	{
		pDisassociateCurrentThreadFromCallback(pci);
		return;
	}
#endif
	/* No default implementation */
}

#endif /* WINPR_THREAD_POOL defined */

