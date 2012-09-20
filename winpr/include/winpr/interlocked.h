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

#ifndef WINPR_INTERLOCKED_H
#define WINPR_INTERLOCKED_H

#include <winpr/spec.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

typedef struct _LIST_ENTRY LIST_ENTRY;
typedef struct _LIST_ENTRY* PLIST_ENTRY;

struct _LIST_ENTRY
{
	PLIST_ENTRY Flink;
	PLIST_ENTRY Blink;
};

typedef struct _SINGLE_LIST_ENTRY SINGLE_LIST_ENTRY;
typedef struct _SINGLE_LIST_ENTRY* PSINGLE_LIST_ENTRY;

struct _SINGLE_LIST_ENTRY
{
	PSINGLE_LIST_ENTRY Next;
};

typedef struct LIST_ENTRY32
{
	DWORD Flink;
	DWORD Blink;
} LIST_ENTRY32;
typedef LIST_ENTRY32 *PLIST_ENTRY32;

typedef struct LIST_ENTRY64
{
	ULONGLONG Flink;
	ULONGLONG Blink;
} LIST_ENTRY64;
typedef LIST_ENTRY64 *PLIST_ENTRY64;

#ifdef _AMD64_

typedef struct _SLIST_ENTRY *PSLIST_ENTRY;
typedef struct DECLSPEC_ALIGN(16) _SLIST_ENTRY
{
	PSLIST_ENTRY Next;
} SLIST_ENTRY;

#else  /* _AMD64_ */

#define SLIST_ENTRY SINGLE_LIST_ENTRY
#define _SLIST_ENTRY _SINGLE_LIST_ENTRY
#define PSLIST_ENTRY PSINGLE_LIST_ENTRY

#endif /* _AMD64_ */

#if defined(_AMD64_)

typedef struct DECLSPEC_ALIGN(16) _SLIST_HEADER
{
	ULONGLONG Alignment;
	ULONGLONG Region;
} SLIST_HEADER;
typedef struct _SLIST_HEADER *PSLIST_HEADER;

#else  /* _AMD64_ */

typedef union _SLIST_HEADER
{
	ULONGLONG Alignment;

	struct
	{
		SLIST_ENTRY Next;
		WORD Depth;
		WORD Sequence;
	} DUMMYSTRUCTNAME;
} SLIST_HEADER, *PSLIST_HEADER;

#endif /* _AMD64_ */

WINPR_API VOID InitializeSListHead(PSLIST_HEADER ListHead);

WINPR_API PSLIST_ENTRY InterlockedPopEntrySList(PSLIST_HEADER ListHead);
WINPR_API PSLIST_ENTRY InterlockedPushEntrySList(PSLIST_HEADER ListHead, PSLIST_ENTRY ListEntry);
WINPR_API PSLIST_ENTRY InterlockedPushListSListEx(PSLIST_HEADER ListHead, PSLIST_ENTRY List, PSLIST_ENTRY ListEnd, ULONG Count);
WINPR_API PSLIST_ENTRY InterlockedFlushSList(PSLIST_HEADER ListHead);

WINPR_API USHORT QueryDepthSList(PSLIST_HEADER ListHead);

WINPR_API LONG InterlockedIncrement(LONG volatile *Addend);
WINPR_API LONG InterlockedDecrement(LONG volatile *Addend);

WINPR_API LONG InterlockedExchange(LONG volatile *Target, LONG Value);
WINPR_API LONG InterlockedExchangeAdd(LONG volatile *Addend, LONG Value);

WINPR_API LONG InterlockedCompareExchange(LONG volatile *Destination,LONG ExChange, LONG Comperand);
WINPR_API LONG64 InterlockedCompareExchange64(LONG64 volatile *Destination, LONG64 ExChange, LONG64 Comperand);

#endif /* _WIN32 */

#endif /* WINPR_INTERLOCKED_H */

