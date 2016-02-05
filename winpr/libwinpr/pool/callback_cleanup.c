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

#ifdef _WIN32

static BOOL module_initialized = FALSE;
static BOOL module_available = FALSE;
static HMODULE kernel32_module = NULL;

static VOID (WINAPI * pSetEventWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE evt);
static VOID (WINAPI * pReleaseSemaphoreWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE sem, DWORD crel);
static VOID (WINAPI * pReleaseMutexWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HANDLE mut);
static VOID (WINAPI * pLeaveCriticalSectionWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, PCRITICAL_SECTION pcs);
static VOID (WINAPI * pFreeLibraryWhenCallbackReturns)(PTP_CALLBACK_INSTANCE pci, HMODULE mod);
static VOID (WINAPI * pDisassociateCurrentThreadFromCallback)(PTP_CALLBACK_INSTANCE pci);

static void module_init()
{
	if (module_initialized)
		return;

	kernel32_module = LoadLibraryA("kernel32.dll");
	module_initialized = TRUE;

	if (!kernel32_module)
		return;

	module_available = TRUE;

	pSetEventWhenCallbackReturns = (void*) GetProcAddress(kernel32_module, "SetEventWhenCallbackReturns");
	pReleaseSemaphoreWhenCallbackReturns = (void*) GetProcAddress(kernel32_module, "ReleaseSemaphoreWhenCallbackReturns");
	pReleaseMutexWhenCallbackReturns = (void*) GetProcAddress(kernel32_module, "ReleaseMutexWhenCallbackReturns");
	pLeaveCriticalSectionWhenCallbackReturns = (void*) GetProcAddress(kernel32_module, "LeaveCriticalSectionWhenCallbackReturns");
	pFreeLibraryWhenCallbackReturns = (void*) GetProcAddress(kernel32_module, "FreeLibraryWhenCallbackReturns");
	pDisassociateCurrentThreadFromCallback = (void*) GetProcAddress(kernel32_module, "DisassociateCurrentThreadFromCallback");
}

#endif

#ifdef WINPR_THREAD_POOL

VOID SetEventWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE evt)
{
#ifdef _WIN32
	module_init();

	if (pSetEventWhenCallbackReturns)
		pSetEventWhenCallbackReturns(pci, evt);
#else
#endif
}

VOID ReleaseSemaphoreWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE sem, DWORD crel)
{
#ifdef _WIN32
	module_init();

	if (pReleaseSemaphoreWhenCallbackReturns)
		pReleaseSemaphoreWhenCallbackReturns(pci, sem, crel);
#else
#endif
}

VOID ReleaseMutexWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HANDLE mut)
{
#ifdef _WIN32
	module_init();

	if (pReleaseMutexWhenCallbackReturns)
		pReleaseMutexWhenCallbackReturns(pci, mut);
#else
#endif
}

VOID LeaveCriticalSectionWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, PCRITICAL_SECTION pcs)
{
#ifdef _WIN32
	module_init();

	if (pLeaveCriticalSectionWhenCallbackReturns)
		pLeaveCriticalSectionWhenCallbackReturns(pci, pcs);
#else
#endif
}

VOID FreeLibraryWhenCallbackReturns(PTP_CALLBACK_INSTANCE pci, HMODULE mod)
{
#ifdef _WIN32
	module_init();

	if (pFreeLibraryWhenCallbackReturns)
		pFreeLibraryWhenCallbackReturns(pci, mod);
#else
#endif
}

VOID DisassociateCurrentThreadFromCallback(PTP_CALLBACK_INSTANCE pci)
{
#ifdef _WIN32
	module_init();

	if (pDisassociateCurrentThreadFromCallback)
		pDisassociateCurrentThreadFromCallback(pci);
#else
#endif
}

#endif

