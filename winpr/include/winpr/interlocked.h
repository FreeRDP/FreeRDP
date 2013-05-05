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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32

/* workaround for SLIST_ENTRY conflict */

#include <sys/queue.h>
#undef SLIST_ENTRY

#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(address, type, field) \
		((type *)(((ULONG_PTR) address) - (ULONG_PTR)(&(((type *) 0)->field))))
#endif

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

#ifdef _WIN64

typedef struct _SLIST_ENTRY *PSLIST_ENTRY;
typedef struct DECLSPEC_ALIGN(16) _SLIST_ENTRY
{
	PSLIST_ENTRY Next;
} SLIST_ENTRY;

#else  /* _WIN64 */

#define SLIST_ENTRY SINGLE_LIST_ENTRY
#define _SLIST_ENTRY _SINGLE_LIST_ENTRY
#define PSLIST_ENTRY PSINGLE_LIST_ENTRY

#endif /* _WIN64 */

#ifdef _WIN64

typedef union DECLSPEC_ALIGN(16) _SLIST_HEADER
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
} SLIST_HEADER, *PSLIST_HEADER;

#else  /* _WIN64 */

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

#endif /* _WIN64 */

/* Singly-Linked List */

WINPR_API VOID InitializeSListHead(PSLIST_HEADER ListHead);

WINPR_API PSLIST_ENTRY InterlockedPushEntrySList(PSLIST_HEADER ListHead, PSLIST_ENTRY ListEntry);
WINPR_API PSLIST_ENTRY InterlockedPushListSListEx(PSLIST_HEADER ListHead, PSLIST_ENTRY List, PSLIST_ENTRY ListEnd, ULONG Count);
WINPR_API PSLIST_ENTRY InterlockedPopEntrySList(PSLIST_HEADER ListHead);
WINPR_API PSLIST_ENTRY InterlockedFlushSList(PSLIST_HEADER ListHead);

WINPR_API USHORT QueryDepthSList(PSLIST_HEADER ListHead);

WINPR_API LONG InterlockedIncrement(LONG volatile *Addend);
WINPR_API LONG InterlockedDecrement(LONG volatile *Addend);

WINPR_API LONG InterlockedExchange(LONG volatile *Target, LONG Value);
WINPR_API LONG InterlockedExchangeAdd(LONG volatile *Addend, LONG Value);

WINPR_API LONG InterlockedCompareExchange(LONG volatile *Destination, LONG Exchange, LONG Comperand);

#endif /* _WIN32 */

WINPR_API LONGLONG InterlockedCompareExchange64(LONGLONG volatile *Destination, LONGLONG Exchange, LONGLONG Comperand);

/* Doubly-Linked List */

WINPR_API VOID InitializeListHead(PLIST_ENTRY ListHead);

WINPR_API BOOL IsListEmpty(const LIST_ENTRY* ListHead);

WINPR_API BOOL RemoveEntryList(PLIST_ENTRY Entry);

WINPR_API VOID InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry);
WINPR_API PLIST_ENTRY RemoveHeadList(PLIST_ENTRY ListHead);

WINPR_API VOID InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry);
WINPR_API PLIST_ENTRY RemoveTailList(PLIST_ENTRY ListHead);
WINPR_API VOID AppendTailList(PLIST_ENTRY ListHead, PLIST_ENTRY ListToAppend);

WINPR_API VOID PushEntryList(PSINGLE_LIST_ENTRY ListHead, PSINGLE_LIST_ENTRY Entry);
WINPR_API PSINGLE_LIST_ENTRY PopEntryList(PSINGLE_LIST_ENTRY ListHead);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_INTERLOCKED_H */

