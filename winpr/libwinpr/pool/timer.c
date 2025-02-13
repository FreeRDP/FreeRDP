/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (Timer)
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
#include <winpr/wlog.h>

#ifdef WINPR_THREAD_POOL

PTP_TIMER winpr_CreateThreadpoolTimer(WINPR_ATTR_UNUSED PTP_TIMER_CALLBACK pfnti,
                                      WINPR_ATTR_UNUSED PVOID pv,
                                      WINPR_ATTR_UNUSED PTP_CALLBACK_ENVIRON pcbe)
{
	WLog_ERR("TODO", "TODO: Implement");
	return NULL;
}

VOID winpr_CloseThreadpoolTimer(WINPR_ATTR_UNUSED PTP_TIMER pti)
{
	WLog_ERR("TODO", "TODO: Implement");
}

BOOL winpr_IsThreadpoolTimerSet(WINPR_ATTR_UNUSED PTP_TIMER pti)
{
	WLog_ERR("TODO", "TODO: Implement");
	return FALSE;
}

VOID winpr_SetThreadpoolTimer(WINPR_ATTR_UNUSED PTP_TIMER pti,
                              WINPR_ATTR_UNUSED PFILETIME pftDueTime,
                              WINPR_ATTR_UNUSED DWORD msPeriod,
                              WINPR_ATTR_UNUSED DWORD msWindowLength)
{
	WLog_ERR("TODO", "TODO: Implement");
}

VOID winpr_WaitForThreadpoolTimerCallbacks(WINPR_ATTR_UNUSED PTP_TIMER pti,
                                           WINPR_ATTR_UNUSED BOOL fCancelPendingCallbacks)
{
	WLog_ERR("TODO", "TODO: Implement");
}

#endif
