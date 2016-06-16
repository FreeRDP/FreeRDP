/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/synch.h>
#include <winpr/interlocked.h>

#include "../log.h"
#define TAG WINPR_TAG("sync")

#if (!defined(_WIN32)) || (defined(_WIN32) && (_WIN32_WINNT < 0x0600))

BOOL winpr_InitOnceBeginInitialize(LPINIT_ONCE lpInitOnce, DWORD dwFlags, PBOOL fPending, LPVOID* lpContext)
{
	WLog_ERR(TAG, "not implemented");
	return FALSE;
}

BOOL winpr_InitOnceComplete(LPINIT_ONCE lpInitOnce, DWORD dwFlags, LPVOID lpContext)
{
	WLog_ERR(TAG, "not implemented");
	return FALSE;
}

VOID winpr_InitOnceInitialize(PINIT_ONCE InitOnce)
{
	WLog_ERR(TAG, "not implemented");
}

BOOL winpr_InitOnceExecuteOnce(PINIT_ONCE InitOnce, PINIT_ONCE_FN InitFn, PVOID Parameter, LPVOID* Context)
{
	for (;;)
	{
		switch ((ULONG_PTR)InitOnce->Ptr & 3)
		{
			case 2:
				/* already completed successfully */
				return TRUE;

			case 0:

				/* first time */
				if (InterlockedCompareExchangePointer(&InitOnce->Ptr, (PVOID)1, (PVOID)0) != (PVOID)0)
				{
					/* some other thread was faster */
					break;
				}

				/* it's our job to call the init function */
				if (InitFn(InitOnce, Parameter, Context))
				{
					/* success */
					InitOnce->Ptr = (PVOID)2;
					return TRUE;
				}

				/* the init function returned an error,  reset the status */
				InitOnce->Ptr = (PVOID)0;
				return FALSE;

			case 1:
				/* in progress */
				break;

			default:
				WLog_ERR(TAG, "internal error");
				return FALSE;
		}

		Sleep(5);
	}
}

#endif
