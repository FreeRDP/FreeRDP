/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Callback)
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

#ifdef _WIN32

static BOOL module_initialized = FALSE;
static BOOL module_available = FALSE;
static HMODULE kernel32_module = NULL;

static BOOL (WINAPI * pCallbackMayRunLong)(PTP_CALLBACK_INSTANCE pci);

static void module_init()
{
	if (module_initialized)
		return;

	kernel32_module = LoadLibraryA("kernel32.dll");
	module_initialized = TRUE;

	if (!kernel32_module)
		return;

	module_available = TRUE;

	pCallbackMayRunLong = (void*) GetProcAddress(kernel32_module, "CallbackMayRunLong");
}

#endif

#ifdef WINPR_THREAD_POOL

BOOL CallbackMayRunLong(PTP_CALLBACK_INSTANCE pci)
{
#ifdef _WIN32
	module_init();

	if (pCallbackMayRunLong)
		return pCallbackMayRunLong(pci);
#else
#endif
	return FALSE;
}

#endif
