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
#include <winpr/platform.h>

#ifdef WINPR_MONO_CONFLICT
#define InterlockedCompareExchange64 winpr_InterlockedCompareExchange64
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32

#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(address, type, field) \
	    ((type *)(((ULONG_PTR) address) - (ULONG_PTR)(&(((type *) 0)->field))))
#endif

typedef struct _WINPR_LIST_ENTRY WINPR_LIST_ENTRY;
typedef struct _WINPR_LIST_ENTRY* WINPR_PLIST_ENTRY;

struct _WINPR_LIST_ENTRY
{
		WINPR_PLIST_ENTRY Flink;
		WINPR_PLIST_ENTRY Blink;
};

typedef struct _WINPR_SINGLE_LIST_ENTRY WINPR_SINGLE_LIST_ENTRY;
typedef struct _WINPR_SINGLE_LIST_ENTRY* WINPR_PSINGLE_LIST_ENTRY;

struct _WINPR_SINGLE_LIST_ENTRY
{
		WINPR_PSINGLE_LIST_ENTRY Next;
};

typedef struct WINPR_LIST_ENTRY32
{
	DWORD Flink;
	DWORD Blink;
} WINPR_LIST_ENTRY32;
typedef WINPR_LIST_ENTRY32 *WINPR_PLIST_ENTRY32;

typedef struct WINPR_LIST_ENTRY64
{
	ULONGLONG Flink;
	ULONGLONG Blink;
} WINPR_LIST_ENTRY64;
typedef WINPR_LIST_ENTRY64 *WINPR_PLIST_ENTRY64;

#ifdef _WIN64

typedef struct _WINPR_SLIST_ENTRY *WINPR_PSLIST_ENTRY;
typedef struct DECLSPEC_ALIGN(16) _WINPR_SLIST_ENTRY
{
	    WINPR_PSLIST_ENTRY Next;
} WINPR_SLIST_ENTRY;

#else  /* _WIN64 */

#define WINPR_SLIST_ENTRY WINPR_SINGLE_LIST_ENTRY
#define _WINPR_SLIST_ENTRY _WINPR_SINGLE_LIST_ENTRY
#define WINPR_PSLIST_ENTRY WINPR_PSINGLE_LIST_ENTRY

#endif /* _WIN64 */

#ifdef _WIN64

typedef union DECLSPEC_ALIGN(16) _WINPR_SLIST_HEADER
{
	struct
	{
		ULONGLONG Alignment;
		ULONGLONG Region;
	} DUMMYSTRUCTNAME;

	struct
	{
		ULONGLONG Depth:16;
		ULONGLONG Sequence:9;
		ULONGLONG NextEntry:39;
		ULONGLONG HeaderType:1;
		ULONGLONG Init:1;
		ULONGLONG Reserved:59;
		ULONGLONG Region:3;
	} Header8;

	struct
	{
		ULONGLONG Depth:16;
		ULONGLONG Sequence:48;
		ULONGLONG HeaderType:1;
		ULONGLONG Reserved:3;
		ULONGLONG NextEntry:60;
	} HeaderX64;
} WINPR_SLIST_HEADER, *WINPR_PSLIST_HEADER;

#else  /* _WIN64 */

typedef union _WINPR_SLIST_HEADER
{
	ULONGLONG Alignment;

	struct
	{
		WINPR_SLIST_ENTRY Next;
		WORD Depth;
		WORD Sequence;
	} DUMMYSTRUCTNAME;
} WINPR_SLIST_HEADER, *WINPR_PSLIST_HEADER;

#endif /* _WIN64 */

/* Singly-Linked List */

WINPR_API VOID InitializeSListHead(WINPR_PSLIST_HEADER ListHead);

WINPR_API WINPR_PSLIST_ENTRY InterlockedPushEntrySList(WINPR_PSLIST_HEADER ListHead, WINPR_PSLIST_ENTRY ListEntry);
WINPR_API WINPR_PSLIST_ENTRY InterlockedPushListSListEx(WINPR_PSLIST_HEADER ListHead, WINPR_PSLIST_ENTRY List, WINPR_PSLIST_ENTRY ListEnd, ULONG Count);
WINPR_API WINPR_PSLIST_ENTRY InterlockedPopEntrySList(WINPR_PSLIST_HEADER ListHead);
WINPR_API WINPR_PSLIST_ENTRY InterlockedFlushSList(WINPR_PSLIST_HEADER ListHead);

WINPR_API USHORT QueryDepthSList(WINPR_PSLIST_HEADER ListHead);

WINPR_API LONG InterlockedIncrement(LONG volatile *Addend);
WINPR_API LONG InterlockedDecrement(LONG volatile *Addend);

WINPR_API LONG InterlockedExchange(LONG volatile *Target, LONG Value);
WINPR_API LONG InterlockedExchangeAdd(LONG volatile *Addend, LONG Value);

WINPR_API LONG InterlockedCompareExchange(LONG volatile *Destination, LONG Exchange, LONG Comperand);

WINPR_API PVOID InterlockedCompareExchangePointer(PVOID volatile *Destination, PVOID Exchange, PVOID Comperand);

#else /* _WIN32 */
#define WINPR_LIST_ENTRY LIST_ENTRY
#define _WINPR_LIST_ENTRY _LIST_ENTRY
#define WINPR_PLIST_ENTRY PLIST_ENTRY

#define WINPR_SINGLE_LIST_ENTRY SINGLE_LIST_ENTRY
#define _WINPR_SINGLE_LIST_ENTRY _SINGLE_LIST_ENTRY
#define WINPR_PSINGLE_LIST_ENTRY PSINGLE_LIST_ENTRY

#define WINPR_SLIST_ENTRY SLIST_ENTRY
#define _WINPR_SLIST_ENTRY _SLIST_ENTRY
#define WINPR_PSLIST_ENTRY PSLIST_ENTRY

#define WINPR_SLIST_HEADER SLIST_HEADER
#define _WINPR_SLIST_HEADER _SLIST_HEADER
#define WINPR_PSLIST_HEADER PSLIST_HEADER

#endif /* _WIN32 */

#if (!defined(_WIN32) || (defined(_WIN32) && (_WIN32_WINNT < 0x0502) && !defined(InterlockedCompareExchange64)))
#define WINPR_INTERLOCKED_COMPARE_EXCHANGE64	1
#endif

#ifdef WINPR_INTERLOCKED_COMPARE_EXCHANGE64

WINPR_API LONGLONG InterlockedCompareExchange64(LONGLONG volatile *Destination, LONGLONG Exchange, LONGLONG Comperand);

#endif

/* Doubly-Linked List */

WINPR_API VOID InitializeListHead(WINPR_PLIST_ENTRY ListHead);

WINPR_API BOOL IsListEmpty(const WINPR_LIST_ENTRY* ListHead);

WINPR_API BOOL RemoveEntryList(WINPR_PLIST_ENTRY Entry);

WINPR_API VOID InsertHeadList(WINPR_PLIST_ENTRY ListHead, WINPR_PLIST_ENTRY Entry);
WINPR_API WINPR_PLIST_ENTRY RemoveHeadList(WINPR_PLIST_ENTRY ListHead);

WINPR_API VOID InsertTailList(WINPR_PLIST_ENTRY ListHead, WINPR_PLIST_ENTRY Entry);
WINPR_API WINPR_PLIST_ENTRY RemoveTailList(WINPR_PLIST_ENTRY ListHead);
WINPR_API VOID AppendTailList(WINPR_PLIST_ENTRY ListHead, WINPR_PLIST_ENTRY ListToAppend);

WINPR_API VOID PushEntryList(WINPR_PSINGLE_LIST_ENTRY ListHead, WINPR_PSINGLE_LIST_ENTRY Entry);
WINPR_API WINPR_PSINGLE_LIST_ENTRY PopEntryList(WINPR_PSINGLE_LIST_ENTRY ListHead);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_INTERLOCKED_H */

