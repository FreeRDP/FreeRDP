/**
 * WinPR: Windows Portable Runtime
 * Thread Pool API (I/O)
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

#ifdef WINPR_THREAD_POOL

PTP_IO winpr_CreateThreadpoolIo(HANDLE fl, PTP_WIN32_IO_CALLBACK pfnio, PVOID pv, PTP_CALLBACK_ENVIRON pcbe)
{
	return NULL;
}

VOID winpr_CloseThreadpoolIo(PTP_IO pio)
{

}

VOID winpr_StartThreadpoolIo(PTP_IO pio)
{

}

VOID winpr_CancelThreadpoolIo(PTP_IO pio)
{

}

VOID winpr_WaitForThreadpoolIoCallbacks(PTP_IO pio, BOOL fCancelPendingCallbacks)
{

}

#endif
