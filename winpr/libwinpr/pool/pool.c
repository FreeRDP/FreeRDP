/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Pool)
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

static PTP_POOL (WINAPI * pCreateThreadpool)(PVOID reserved);
static VOID (WINAPI * pCloseThreadpool)(PTP_POOL ptpp);
static VOID (WINAPI * pSetThreadpoolThreadMaximum)(PTP_POOL ptpp, DWORD cthrdMost);
static BOOL (WINAPI * pSetThreadpoolThreadMinimum)(PTP_POOL ptpp, DWORD cthrdMic);

static void module_init()
{
	if (module_initialized)
		return;

	kernel32_module = LoadLibraryA("kernel32.dll");

	if (!kernel32_module)
		return;

	module_available = TRUE;

	pCreateThreadpool = (void*) GetProcAddress(kernel32_module, "CreateThreadpool");
	pCloseThreadpool = (void*) GetProcAddress(kernel32_module, "CloseThreadpool");
	pSetThreadpoolThreadMaximum = (void*) GetProcAddress(kernel32_module, "SetThreadpoolThreadMaximum");
	pSetThreadpoolThreadMinimum = (void*) GetProcAddress(kernel32_module, "SetThreadpoolThreadMinimum");
}

#endif

PTP_POOL CreateThreadpool(PVOID reserved)
{
#ifdef _WIN32
	module_init();

	if (pCreateThreadpool)
		return pCreateThreadpool(reserved);
#endif

	return NULL;
}

VOID CloseThreadpool(PTP_POOL ptpp)
{
#ifdef _WIN32
	module_init();

	if (pCloseThreadpool)
		pCloseThreadpool(ptpp);
#endif
}

VOID SetThreadpoolThreadMaximum(PTP_POOL ptpp, DWORD cthrdMost)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolThreadMaximum)
		pSetThreadpoolThreadMaximum(ptpp, cthrdMost);
#endif
}

BOOL SetThreadpoolThreadMinimum(PTP_POOL ptpp, DWORD cthrdMic)
{
#ifdef _WIN32
	module_init();

	if (pSetThreadpoolThreadMinimum)
		pSetThreadpoolThreadMinimum(ptpp, cthrdMic);
#endif

	return FALSE;
}

/* dummy */

void winpr_pool_dummy()
{

}
