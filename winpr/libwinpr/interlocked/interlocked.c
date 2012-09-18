/**
 * WinPR: Windows Portable Runtime
 * Interlocked Singly-Linked Lists
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

#include <winpr/interlocked.h>

/**
 * api-ms-win-core-interlocked-l1-2-0.dll:
 *
 * InitializeSListHead
 * InterlockedPopEntrySList
 * InterlockedPushEntrySList
 * InterlockedPushListSListEx
 * InterlockedFlushSList
 * QueryDepthSList
 * InterlockedIncrement
 * InterlockedDecrement
 * InterlockedExchange
 * InterlockedExchangeAdd
 * InterlockedCompareExchange
 * InterlockedCompareExchange64
 */

#ifndef _WIN32

VOID InitializeSListHead(PSLIST_HEADER ListHead)
{

}

PSLIST_ENTRY InterlockedPopEntrySList(PSLIST_HEADER ListHead)
{
	return NULL;
}

PSLIST_ENTRY InterlockedPushEntrySList(PSLIST_HEADER ListHead, PSLIST_ENTRY ListEntry)
{
	return NULL;
}

PSLIST_ENTRY InterlockedPushListSListEx(PSLIST_HEADER ListHead, PSLIST_ENTRY List, PSLIST_ENTRY ListEnd, ULONG Count)
{
	return NULL;
}

PSLIST_ENTRY InterlockedFlushSList(PSLIST_HEADER ListHead)
{
	return NULL;
}

USHORT QueryDepthSList(PSLIST_HEADER ListHead)
{
	return 0;
}

LONG InterlockedIncrement(LONG volatile *Addend)
{
	return 0;
}

LONG InterlockedDecrement(LONG volatile *Addend)
{
	return 0;
}

LONG InterlockedExchange(LONG volatile *Target, LONG Value)
{
	return 0;
}

LONG InterlockedExchangeAdd(LONG volatile *Addend, LONG Value)
{
	return 0;
}

LONG InterlockedCompareExchange(LONG volatile *Destination,LONG ExChange, LONG Comperand)
{
	return 0;
}

LONG64 InterlockedCompareExchange64(LONG64 volatile *Destination, LONG64 ExChange, LONG64 Comperand)
{
	return 0;
}

#endif
