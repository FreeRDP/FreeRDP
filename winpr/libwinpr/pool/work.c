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

#ifdef _WIN32

static BOOL module_initialized = FALSE;
static BOOL module_available = FALSE;
static HMODULE kernel32_module = NULL;

static PTP_WORK (WINAPI * pCreateThreadpoolWork)(PTP_WORK_CALLBACK pfnwk, PVOID pv, PTP_CALLBACK_ENVIRON pcbe);
static VOID (WINAPI * pCloseThreadpoolWork)(PTP_WORK pwk);
static VOID (WINAPI * pSubmitThreadpoolWork)(PTP_WORK pwk);
static BOOL (WINAPI * pTrySubmitThreadpoolCallback)(PTP_SIMPLE_CALLBACK pfns, PVOID pv, PTP_CALLBACK_ENVIRON pcbe);
static VOID (WINAPI * pWaitForThreadpoolWorkCallbacks)(PTP_WORK pwk, BOOL fCancelPendingCallbacks);

static void module_init()
{
	if (module_initialized)
		return;

	kernel32_module = LoadLibraryA("kernel32.dll");

	if (!kernel32_module)
		return;

	module_available = TRUE;

	pCreateThreadpoolWork = (void*) GetProcAddress(kernel32_module, "CreateThreadpoolWork");
	pCloseThreadpoolWork = (void*) GetProcAddress(kernel32_module, "CloseThreadpoolWork");
	pSubmitThreadpoolWork = (void*) GetProcAddress(kernel32_module, "SubmitThreadpoolWork");
	pTrySubmitThreadpoolCallback = (void*) GetProcAddress(kernel32_module, "TrySubmitThreadpoolCallback");
	pWaitForThreadpoolWorkCallbacks = (void*) GetProcAddress(kernel32_module, "WaitForThreadpoolWorkCallbacks");
}

#endif

PTP_WORK CreateThreadpoolWork(PTP_WORK_CALLBACK pfnwk, PVOID pv, PTP_CALLBACK_ENVIRON pcbe)
{
#ifdef _WIN32
	module_init();

	if (pCreateThreadpoolWork)
		return pCreateThreadpoolWork(pfnwk, pv, pcbe);
#endif

	return NULL;
}

VOID CloseThreadpoolWork(PTP_WORK pwk)
{
#ifdef _WIN32
	module_init();

	if (pCloseThreadpoolWork)
		pCloseThreadpoolWork(pwk);
#endif
}

VOID SubmitThreadpoolWork(PTP_WORK pwk)
{
#ifdef _WIN32
	module_init();

	if (pSubmitThreadpoolWork)
		pSubmitThreadpoolWork(pwk);
#endif
}

BOOL TrySubmitThreadpoolCallback(PTP_SIMPLE_CALLBACK pfns, PVOID pv, PTP_CALLBACK_ENVIRON pcbe)
{
#ifdef _WIN32
	module_init();

	if (pTrySubmitThreadpoolCallback)
		return pTrySubmitThreadpoolCallback(pfns, pv, pcbe);
#endif

	return FALSE;
}

VOID WaitForThreadpoolWorkCallbacks(PTP_WORK pwk, BOOL fCancelPendingCallbacks)
{
#ifdef _WIN32
	module_init();

	if (pWaitForThreadpoolWorkCallbacks)
		pWaitForThreadpoolWorkCallbacks(pwk, fCancelPendingCallbacks);
#endif
}


