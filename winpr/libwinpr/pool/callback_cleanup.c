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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/pool.h>
#include <winpr/library.h>

#include "pool.h"

#ifdef WINPR_THREAD_POOL

#ifdef _WIN32
static INIT_ONCE init_once_module = INIT_ONCE_STATIC_INIT;
static VOID(WINAPI* pSetEventWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE evt);
static VOID(WINAPI* pReleaseSemaphoreWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE sem,
                                                          DWORD crel);
static VOID(WINAPI* pReleaseMutexWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE mut);
static VOID(WINAPI* pLeaveCriticalSectionWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci,
                                                              PCRITICAL_SECTION pcs);
static VOID(WINAPI* pFreeLibraryWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HMODULE mod);
static VOID(WINAPI* pDisassociateCurrentThreadFromCallback)(PTP_CALLBACK_INSTANCE pci);

static BOOL CALLBACK init_module(PINIT_ONCE once, PVOID param, PVOID* context)
{
	HMODULE kernel32 = LoadLibraryA("kernel32.dll");
	if (kernel32)
	{
		pSetEventWhenCallbackReturns =
		    GetProcAddressAs(kernel32, "SetEventWhenCallbackReturns"), void*);
		pReleaseSemaphoreWhenCallbackReturns =
		    GetProcAddressAs(kernel32, "ReleaseSemaphoreWhenCallbackReturns", void*);
		pReleaseMutexWhenCallbackReturns =
		    GetProcAddressAs(kernel32, "ReleaseMutexWhenCallbackReturns", void*);
		pLeaveCriticalSectionWhenCallbackReturns =
		    GetProcAddressAs(kernel32, "LeaveCriticalSectionWhenCallbackReturns", void*);
		pFreeLibraryWhenCallbackReturns =
		    GetProcAddressAs(kernel32, "FreeLibraryWhenCallbackReturns", void*);
		pDisassociateCurrentThreadFromCallback =
		    GetProcAddressAs(kernel32, "DisassociateCurrentThreadFromCallback", void*);
	}
	return TRUE;
}
#endif

VOID SetEventWhenCallbackReturns(WINPR_ATTR_UNUSED PTP_CALLBACK_INSTANCE pci,
                                 WINPR_ATTR_UNUSED HANDLE evt)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pSetEventWhenCallbackReturns)
	{
		pSetEventWhenCallbackReturns(pci, evt);
		return;
	}
#endif
	WLog_ERR("TODO", "TODO: implement");
	/* No default implementation */
}

VOID ReleaseSemaphoreWhenCallbackReturns(WINPR_ATTR_UNUSED PTP_CALLBACK_INSTANCE pci,
                                         WINPR_ATTR_UNUSED HANDLE sem, WINPR_ATTR_UNUSED DWORD crel)
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
	WLog_ERR("TODO", "TODO: implement");
}

VOID ReleaseMutexWhenCallbackReturns(WINPR_ATTR_UNUSED PTP_CALLBACK_INSTANCE pci,
                                     WINPR_ATTR_UNUSED HANDLE mut)
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
	WLog_ERR("TODO", "TODO: implement");
}

VOID LeaveCriticalSectionWhenCallbackReturns(WINPR_ATTR_UNUSED PTP_CALLBACK_INSTANCE pci,
                                             WINPR_ATTR_UNUSED PCRITICAL_SECTION pcs)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pLeaveCriticalSectionWhenCallbackReturns)
	{
		pLeaveCriticalSectionWhenCallbackReturns(pci, pcs);
	}
#endif
	/* No default implementation */
	WLog_ERR("TODO", "TODO: implement");
}

VOID FreeLibraryWhenCallbackReturns(WINPR_ATTR_UNUSED PTP_CALLBACK_INSTANCE pci,
                                    WINPR_ATTR_UNUSED HMODULE mod)
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
	WLog_ERR("TODO", "TODO: implement");
}

VOID DisassociateCurrentThreadFromCallback(WINPR_ATTR_UNUSED PTP_CALLBACK_INSTANCE pci)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pDisassociateCurrentThreadFromCallback)
	{
		pDisassociateCurrentThreadFromCallback(pci);
		return;
	}
#endif
	WLog_ERR("TODO", "TODO: implement");
	/* No default implementation */
}

#endif /* WINPR_THREAD_POOL defined */
