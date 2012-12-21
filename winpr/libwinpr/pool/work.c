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

#if (!(defined _WIN32 && (_WIN32_WINNT < 0x0600)))

PTP_WORK CreateThreadpoolWork(PTP_WORK_CALLBACK pfnwk, PVOID pv, PTP_CALLBACK_ENVIRON pcbe)
{
	return NULL;
}

VOID CloseThreadpoolWork(PTP_WORK pwk)
{

}

VOID SubmitThreadpoolWork(PTP_WORK pwk)
{

}

BOOL TrySubmitThreadpoolCallback(PTP_SIMPLE_CALLBACK pfns, PVOID pv, PTP_CALLBACK_ENVIRON pcbe)
{
	return FALSE;
}

VOID WaitForThreadpoolWorkCallbacks(PTP_WORK pwk, BOOL fCancelPendingCallbacks)
{

}

#endif

