/**
 * WinPR: Windows Portable Runtime
 * Collections
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

#ifndef WINPR_COLLECTIONS_H
#define WINPR_COLLECTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/synch.h>

/* System.Collections.Queue */

struct _wQueue
{
	int capacity;
	int growthFactor;
	BOOL synchronized;

	int head;
	int tail;
	int size;
	void** array;
	HANDLE mutex;
};
typedef struct _wQueue wQueue;

WINPR_API int Queue_Count(wQueue* queue);
WINPR_API BOOL Queue_IsSynchronized(wQueue* queue);

WINPR_API void Queue_Clear(wQueue* queue);
WINPR_API BOOL Queue_Contains(wQueue* queue, void* obj);

WINPR_API void Queue_Enqueue(wQueue* queue, void* obj);
WINPR_API void* Queue_Dequeue(wQueue* queue);

WINPR_API void* Queue_Peek(wQueue* queue);

WINPR_API wQueue* Queue_New(BOOL synchronized, int capacity, int growthFactor);
WINPR_API void Queue_Free(wQueue* queue);

/* System.Collections.Stack */

struct _wStack
{
	BOOL bSynchronized;
};
typedef struct _wStack wStack;

WINPR_API int Stack_Count(wStack* stack);
WINPR_API BOOL Stack_IsSynchronized(wStack* stack);

WINPR_API void Stack_Clear(wStack* stack);
WINPR_API BOOL Stack_Contains(wStack* stack, void* obj);

WINPR_API void Stack_Push(wStack* stack, void* obj);
WINPR_API void* Stack_Pop(wStack* stack);

WINPR_API void* Stack_Peek(wStack* stack);

WINPR_API wStack* Stack_New(BOOL synchronized);
WINPR_API void Stack_Free(wStack* stack);

/* System.Collections.ArrayList */

struct _wArrayList
{
	BOOL bSynchronized;
};
typedef struct _wArrayList wArrayList;

WINPR_API int ArrayList_Capacity(wArrayList* arrayList);
WINPR_API int ArrayList_Count(wArrayList* arrayList);
WINPR_API BOOL ArrayList_IsFixedSized(wArrayList* arrayList);
WINPR_API BOOL ArrayList_IsReadOnly(wArrayList* arrayList);
WINPR_API BOOL ArrayList_IsSynchronized(wArrayList* arrayList);
WINPR_API void* ArrayList_Item(wArrayList* arrayList, int index, void* obj);

WINPR_API void ArrayList_Clear(wArrayList* arrayList);
WINPR_API BOOL ArrayList_Contains(wArrayList* arrayList, void* obj);

WINPR_API int ArrayList_Add(wArrayList* arrayList, void* obj);
WINPR_API void ArrayList_Insert(wArrayList* arrayList, int index, void* obj);

WINPR_API void ArrayList_Remove(wArrayList* arrayList, void* obj);
WINPR_API void ArrayList_RemoveAt(wArrayList* arrayList, int index, void* obj);

WINPR_API int ArrayList_IndexOf(wArrayList* arrayList, void* obj, int startIndex, int count);
WINPR_API int ArrayList_LastIndexOf(wArrayList* arrayList, void* obj, int startIndex, int count);

WINPR_API wArrayList* ArrayList_New(BOOL synchronized);
WINPR_API void ArrayList_Free(wArrayList* arrayList);

#endif /* WINPR_COLLECTIONS_H */
