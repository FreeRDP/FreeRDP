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

#ifdef WINPR_THREAD_POOL

#ifdef _WIN32
static INIT_ONCE init_once_module = INIT_ONCE_STATIC_INIT;
static BOOL (WINAPI * pCallbackMayRunLong)(PTP_CALLBACK_INSTANCE pci);

static BOOL CALLBACK init_module(PINIT_ONCE once, PVOID param, PVOID *context)
{
	HMODULE kernel32 = LoadLibraryA("kernel32.dll");
	if (kernel32)
	{
		pCallbackMayRunLong = (void*)GetProcAddress(kernel32, "CallbackMayRunLong");
	}
	return TRUE;
}
#endif

BOOL CallbackMayRunLong(PTP_CALLBACK_INSTANCE pci)
{
#ifdef _WIN32
	InitOnceExecuteOnce(&init_once_module, init_module, NULL, NULL);
	if (pCallbackMayRunLong)
		return pCallbackMayRunLong(pci);
#endif
	/* No default implementation */
	return FALSE;
}

#endif /* WINPR_THREAD_POOL defined */
