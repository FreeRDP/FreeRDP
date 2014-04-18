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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*OBJECT_NEW_FN)(void);
typedef void (*OBJECT_INIT_FN)(void* obj);
typedef void (*OBJECT_UNINIT_FN)(void* obj);
typedef void (*OBJECT_FREE_FN)(void* obj);
typedef BOOL (*OBJECT_EQUALS_FN)(void* objA, void* objB);

struct _wObject
{
	OBJECT_NEW_FN fnObjectNew;
	OBJECT_INIT_FN fnObjectInit;
	OBJECT_UNINIT_FN fnObjectUninit;
	OBJECT_FREE_FN fnObjectFree;
	OBJECT_EQUALS_FN fnObjectEquals;
};
typedef struct _wObject wObject;

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
	CRITICAL_SECTION lock;
	HANDLE event;

	wObject object;
};
typedef struct _wQueue wQueue;

WINPR_API int Queue_Count(wQueue* queue);

WINPR_API void Queue_Lock(wQueue* queue);
WINPR_API void Queue_Unlock(wQueue* queue);

WINPR_API HANDLE Queue_Event(wQueue* queue);

#define Queue_Object(_queue)	(&_queue->object)

WINPR_API void Queue_Clear(wQueue* queue);

WINPR_API BOOL Queue_Contains(wQueue* queue, void* obj);

WINPR_API BOOL Queue_Enqueue(wQueue* queue, void* obj);
WINPR_API void* Queue_Dequeue(wQueue* queue);

WINPR_API void* Queue_Peek(wQueue* queue);

WINPR_API wQueue* Queue_New(BOOL synchronized, int capacity, int growthFactor);
WINPR_API void Queue_Free(wQueue* queue);

/* System.Collections.Stack */

struct _wStack
{
	int size;
	int capacity;
	void** array;
	CRITICAL_SECTION lock;
	BOOL synchronized;
	wObject object;
};
typedef struct _wStack wStack;

WINPR_API int Stack_Count(wStack* stack);
WINPR_API BOOL Stack_IsSynchronized(wStack* stack);

#define Stack_Object(_stack)	(&_stack->object)

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
	int capacity;
	int growthFactor;
	BOOL synchronized;

	int size;
	void** array;
	CRITICAL_SECTION lock;

	wObject object;
};
typedef struct _wArrayList wArrayList;

WINPR_API int ArrayList_Capacity(wArrayList* arrayList);
WINPR_API int ArrayList_Count(wArrayList* arrayList);
WINPR_API BOOL ArrayList_IsFixedSized(wArrayList* arrayList);
WINPR_API BOOL ArrayList_IsReadOnly(wArrayList* arrayList);
WINPR_API BOOL ArrayList_IsSynchronized(wArrayList* arrayList);

WINPR_API void ArrayList_Lock(wArrayList* arrayList);
WINPR_API void ArrayList_Unlock(wArrayList* arrayList);

WINPR_API void* ArrayList_GetItem(wArrayList* arrayList, int index);
WINPR_API void ArrayList_SetItem(wArrayList* arrayList, int index, void* obj);

#define ArrayList_Object(_arrayList)	(&_arrayList->object)

WINPR_API void ArrayList_Clear(wArrayList* arrayList);
WINPR_API BOOL ArrayList_Contains(wArrayList* arrayList, void* obj);

WINPR_API int ArrayList_Add(wArrayList* arrayList, void* obj);
WINPR_API BOOL ArrayList_Insert(wArrayList* arrayList, int index, void* obj);

WINPR_API BOOL ArrayList_Remove(wArrayList* arrayList, void* obj);
WINPR_API BOOL ArrayList_RemoveAt(wArrayList* arrayList, int index);

WINPR_API int ArrayList_IndexOf(wArrayList* arrayList, void* obj, int startIndex, int count);
WINPR_API int ArrayList_LastIndexOf(wArrayList* arrayList, void* obj, int startIndex, int count);

WINPR_API wArrayList* ArrayList_New(BOOL synchronized);
WINPR_API void ArrayList_Free(wArrayList* arrayList);

/* System.Collections.DictionaryBase */

struct _wDictionary
{
	BOOL synchronized;
	CRITICAL_SECTION lock;
};
typedef struct _wDictionary wDictionary;

/* System.Collections.Specialized.ListDictionary */

typedef struct _wListDictionaryItem wListDictionaryItem;

struct _wListDictionaryItem
{
	void* key;
	void* value;

	wListDictionaryItem* next;
};

struct _wListDictionary
{
	BOOL synchronized;
	CRITICAL_SECTION lock;

	wListDictionaryItem* head;
	wObject objectKey;
	wObject objectValue;
};
typedef struct _wListDictionary wListDictionary;

#define ListDictionary_KeyObject(_dictionary)	(&_dictionary->objectKey)
#define ListDictionary_ValueObject(_dictionary)	(&_dictionary->objectValue)

WINPR_API int ListDictionary_Count(wListDictionary* listDictionary);

WINPR_API void ListDictionary_Lock(wListDictionary* listDictionary);
WINPR_API void ListDictionary_Unlock(wListDictionary* listDictionary);

WINPR_API BOOL ListDictionary_Add(wListDictionary* listDictionary, void* key, void* value);
WINPR_API void* ListDictionary_Remove(wListDictionary* listDictionary, void* key);
WINPR_API void* ListDictionary_Remove_Head(wListDictionary* listDictionary);
WINPR_API void ListDictionary_Clear(wListDictionary* listDictionary);

WINPR_API BOOL ListDictionary_Contains(wListDictionary* listDictionary, void* key);
WINPR_API int ListDictionary_GetKeys(wListDictionary* listDictionary, ULONG_PTR** ppKeys);

WINPR_API void* ListDictionary_GetItemValue(wListDictionary* listDictionary, void* key);
WINPR_API BOOL ListDictionary_SetItemValue(wListDictionary* listDictionary, void* key, void* value);

WINPR_API wListDictionary* ListDictionary_New(BOOL synchronized);
WINPR_API void ListDictionary_Free(wListDictionary* listDictionary);

/* System.Collections.Generic.LinkedList<T> */

typedef struct _wLinkedListItem wLinkedListNode;

struct _wLinkedListItem
{
	void* value;
	wLinkedListNode* prev;
	wLinkedListNode* next;
};

struct _wLinkedList
{
	int count;
	int initial;
	wLinkedListNode* head;
	wLinkedListNode* tail;
	wLinkedListNode* current;
};
typedef struct _wLinkedList wLinkedList;

WINPR_API int LinkedList_Count(wLinkedList* list);
WINPR_API void* LinkedList_First(wLinkedList* list);
WINPR_API void* LinkedList_Last(wLinkedList* list);

WINPR_API BOOL LinkedList_Contains(wLinkedList* list, void* value);
WINPR_API void LinkedList_Clear(wLinkedList* list);

WINPR_API void LinkedList_AddFirst(wLinkedList* list, void* value);
WINPR_API void LinkedList_AddLast(wLinkedList* list, void* value);

WINPR_API void LinkedList_Remove(wLinkedList* list, void* value);
WINPR_API void LinkedList_RemoveFirst(wLinkedList* list);
WINPR_API void LinkedList_RemoveLast(wLinkedList* list);

WINPR_API void LinkedList_Enumerator_Reset(wLinkedList* list);
WINPR_API void* LinkedList_Enumerator_Current(wLinkedList* list);
WINPR_API BOOL LinkedList_Enumerator_MoveNext(wLinkedList* list);

WINPR_API wLinkedList* LinkedList_New(void);
WINPR_API void LinkedList_Free(wLinkedList* list);

/* System.Collections.Generic.KeyValuePair<TKey,TValue> */

typedef struct _wKeyValuePair wKeyValuePair;

struct _wKeyValuePair
{
	void* key;
	void* value;

	wKeyValuePair* next;
};

/* Reference Table */

struct _wReference
{
	UINT32 Count;
	void* Pointer;
};
typedef struct _wReference wReference;

typedef int (*REFERENCE_FREE)(void* context, void* ptr);

struct _wReferenceTable
{
	UINT32 size;
	CRITICAL_SECTION lock;
	void* context;
	BOOL synchronized;
	wReference* array;
	REFERENCE_FREE ReferenceFree;
};
typedef struct _wReferenceTable wReferenceTable;

WINPR_API UINT32 ReferenceTable_Add(wReferenceTable* referenceTable, void* ptr);
WINPR_API UINT32 ReferenceTable_Release(wReferenceTable* referenceTable, void* ptr);

WINPR_API wReferenceTable* ReferenceTable_New(BOOL synchronized, void* context, REFERENCE_FREE ReferenceFree);
WINPR_API void ReferenceTable_Free(wReferenceTable* referenceTable);

/* Countdown Event */

struct _wCountdownEvent
{
	DWORD count;
	CRITICAL_SECTION lock;
	HANDLE event;
	DWORD initialCount;
};
typedef struct _wCountdownEvent wCountdownEvent;

WINPR_API DWORD CountdownEvent_CurrentCount(wCountdownEvent* countdown);
WINPR_API DWORD CountdownEvent_InitialCount(wCountdownEvent* countdown);
WINPR_API BOOL CountdownEvent_IsSet(wCountdownEvent* countdown);
WINPR_API HANDLE CountdownEvent_WaitHandle(wCountdownEvent* countdown);

WINPR_API void CountdownEvent_AddCount(wCountdownEvent* countdown, DWORD signalCount);
WINPR_API BOOL CountdownEvent_Signal(wCountdownEvent* countdown, DWORD signalCount);
WINPR_API void CountdownEvent_Reset(wCountdownEvent* countdown, DWORD count);

WINPR_API wCountdownEvent* CountdownEvent_New(DWORD initialCount);
WINPR_API void CountdownEvent_Free(wCountdownEvent* countdown);

/* Hash Table */

struct _wHashTable
{
	BOOL synchronized;
	CRITICAL_SECTION lock;

	long numOfBuckets;
	long numOfElements;
	float idealRatio;
	float lowerRehashThreshold;
	float upperRehashThreshold;
	wKeyValuePair** bucketArray;
	int (*keycmp)(void* key1, void* key2);
	int (*valuecmp)(void* value1, void* value2);
	unsigned long (*hashFunction)(void* key);
	void (*keyDeallocator)(void* key);
	void (*valueDeallocator)(void* value);
};
typedef struct _wHashTable wHashTable;

WINPR_API int HashTable_Count(wHashTable* table);
WINPR_API int HashTable_Add(wHashTable* table, void* key, void* value);
WINPR_API BOOL HashTable_Remove(wHashTable* table, void* key);
WINPR_API void HashTable_Clear(wHashTable* table);
WINPR_API BOOL HashTable_Contains(wHashTable* table, void* key);
WINPR_API BOOL HashTable_ContainsKey(wHashTable* table, void* key);
WINPR_API BOOL HashTable_ContainsValue(wHashTable* table, void* value);
WINPR_API void* HashTable_GetItemValue(wHashTable* table, void* key);
WINPR_API BOOL HashTable_SetItemValue(wHashTable* table, void* key, void* value);

WINPR_API wHashTable* HashTable_New(BOOL synchronized);
WINPR_API void HashTable_Free(wHashTable* table);

/* BufferPool */

struct _wBufferPoolItem
{
	int size;
	void* buffer;
};
typedef struct _wBufferPoolItem wBufferPoolItem;

struct _wBufferPool
{
	int fixedSize;
	DWORD alignment;
	BOOL synchronized;
	CRITICAL_SECTION lock;

	int size;
	int capacity;
	void** array;

	int aSize;
	int aCapacity;
	wBufferPoolItem* aArray;

	int uSize;
	int uCapacity;
	wBufferPoolItem* uArray;
};
typedef struct _wBufferPool wBufferPool;

WINPR_API int BufferPool_GetPoolSize(wBufferPool* pool);
WINPR_API int BufferPool_GetBufferSize(wBufferPool* pool, void* buffer);

WINPR_API void* BufferPool_Take(wBufferPool* pool, int bufferSize);
WINPR_API BOOL BufferPool_Return(wBufferPool* pool, void* buffer);
WINPR_API void BufferPool_Clear(wBufferPool* pool);

WINPR_API wBufferPool* BufferPool_New(BOOL synchronized, int fixedSize, DWORD alignment);
WINPR_API void BufferPool_Free(wBufferPool* pool);

/* ObjectPool */

struct _wObjectPool
{
	int size;
	int capacity;
	void** array;
	CRITICAL_SECTION lock;
	wObject object;
	BOOL synchronized;
};
typedef struct _wObjectPool wObjectPool;

WINPR_API void* ObjectPool_Take(wObjectPool* pool);
WINPR_API void ObjectPool_Return(wObjectPool* pool, void* obj);
WINPR_API void ObjectPool_Clear(wObjectPool* pool);

#define ObjectPool_Object(_pool)	(&_pool->object)

WINPR_API wObjectPool* ObjectPool_New(BOOL synchronized);
WINPR_API void ObjectPool_Free(wObjectPool* pool);

/* Message Queue */

typedef struct _wMessage wMessage;

typedef void (*MESSAGE_FREE_FN)(wMessage* message);

struct _wMessage
{
	UINT32 id;
	void* context;
	void* wParam;
	void* lParam;
	UINT64 time;
	MESSAGE_FREE_FN Free;
};

struct _wMessageQueue
{
	int head;
	int tail;
	int size;
	int capacity;
	wMessage* array;
	CRITICAL_SECTION lock;
	HANDLE event;

	wObject object;
};
typedef struct _wMessageQueue wMessageQueue;

#define WMQ_QUIT	0xFFFFFFFF

WINPR_API HANDLE MessageQueue_Event(wMessageQueue* queue);
WINPR_API BOOL MessageQueue_Wait(wMessageQueue* queue);
WINPR_API int MessageQueue_Size(wMessageQueue* queue);

WINPR_API void MessageQueue_Dispatch(wMessageQueue* queue, wMessage* message);
WINPR_API void MessageQueue_Post(wMessageQueue* queue, void* context, UINT32 type, void* wParam, void* lParam);
WINPR_API void MessageQueue_PostQuit(wMessageQueue* queue, int nExitCode);

WINPR_API int MessageQueue_Get(wMessageQueue* queue, wMessage* message);
WINPR_API int MessageQueue_Peek(wMessageQueue* queue, wMessage* message, BOOL remove);

/*! \brief Clears all elements in a message queue.
 *
 *  \note If dynamically allocated data is part of the messages,
 *        a custom cleanup handler must be passed in the 'callback'
 *        argument for MessageQueue_New.
 *
 *  \param queue The queue to clear.
 *
 *  \return 0 in case of success or a error code otherwise.
 */
WINPR_API int MessageQueue_Clear(wMessageQueue *queue);

/*! \brief Creates a new message queue.
 * 				 If 'callback' is null, no custom cleanup will be done
 * 				 on message queue deallocation.
 * 				 If the 'callback' argument contains valid uninit or
 * 				 free functions those will be called by
 * 				 'MessageQueue_Clear'.
 *
 * \param callback a pointer to custom initialization / cleanup functions.
 * 								 Can be NULL if not used.
 *
 * \return A pointer to a newly allocated MessageQueue or NULL.
 */
WINPR_API wMessageQueue* MessageQueue_New(const wObject *callback);

/*! \brief Frees resources allocated by a message queue.
 * 				 This function will only free resources allocated
 *				 internally.
 *
 * \note Empty the queue before calling this function with
 * 			 'MessageQueue_Clear', 'MessageQueue_Get' or
 * 			 'MessageQueue_Peek' to free all resources allocated
 * 			 by the message contained.
 *
 * \param queue A pointer to the queue to be freed.
 */
WINPR_API void MessageQueue_Free(wMessageQueue* queue);

/* Message Pipe */

struct _wMessagePipe
{
	wMessageQueue* In;
	wMessageQueue* Out;
};
typedef struct _wMessagePipe wMessagePipe;

WINPR_API void MessagePipe_PostQuit(wMessagePipe* pipe, int nExitCode);

WINPR_API wMessagePipe* MessagePipe_New(void);
WINPR_API void MessagePipe_Free(wMessagePipe* pipe);

/* Publisher/Subscriber Pattern */

struct _wEventArgs
{
	DWORD Size;
	const char* Sender;
};
typedef struct _wEventArgs wEventArgs;

typedef void (*pEventHandler)(void* context, wEventArgs* e);

#define MAX_EVENT_HANDLERS	32

struct _wEventType
{
	const char* EventName;
	wEventArgs EventArgs;
	int EventHandlerCount;
	pEventHandler EventHandlers[MAX_EVENT_HANDLERS];
};
typedef struct _wEventType wEventType;

#define EventArgsInit(_event_args, _sender) \
	memset(_event_args, 0, sizeof(*_event_args)); \
	((wEventArgs*) _event_args)->Size = sizeof(*_event_args); \
	((wEventArgs*) _event_args)->Sender = _sender

#define DEFINE_EVENT_HANDLER(_name) \
	typedef void (*p ## _name ## EventHandler)(void* context, _name ## EventArgs* e)

#define DEFINE_EVENT_RAISE(_name) \
	static INLINE int PubSub_On ## _name (wPubSub* pubSub, void* context, _name ## EventArgs* e) { \
		return PubSub_OnEvent(pubSub, #_name, context, (wEventArgs*) e); }

#define DEFINE_EVENT_SUBSCRIBE(_name) \
	static INLINE int PubSub_Subscribe ## _name (wPubSub* pubSub, p ## _name ## EventHandler EventHandler) { \
		return PubSub_Subscribe(pubSub, #_name, (pEventHandler) EventHandler); }

#define DEFINE_EVENT_UNSUBSCRIBE(_name) \
	static INLINE int PubSub_Unsubscribe ## _name (wPubSub* pubSub, p ## _name ## EventHandler EventHandler) { \
		return PubSub_Unsubscribe(pubSub, #_name, (pEventHandler) EventHandler); }

#define DEFINE_EVENT_BEGIN(_name) \
	typedef struct _ ## _name ## EventArgs { \
	wEventArgs e;

#define DEFINE_EVENT_END(_name) \
	} _name ## EventArgs; \
	DEFINE_EVENT_HANDLER(_name); \
	DEFINE_EVENT_RAISE(_name) \
	DEFINE_EVENT_SUBSCRIBE(_name) \
	DEFINE_EVENT_UNSUBSCRIBE(_name)

#define DEFINE_EVENT_ENTRY(_name) \
	{ #_name, { sizeof( _name ## EventArgs) }, 0, { \
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, \
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, \
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, \
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL } },

struct _wPubSub
{
	CRITICAL_SECTION lock;
	BOOL synchronized;

	int size;
	int count;
	wEventType* events;
};
typedef struct _wPubSub wPubSub;

WINPR_API void PubSub_Lock(wPubSub* pubSub);
WINPR_API void PubSub_Unlock(wPubSub* pubSub);

WINPR_API wEventType* PubSub_GetEventTypes(wPubSub* pubSub, int* count);
WINPR_API void PubSub_AddEventTypes(wPubSub* pubSub, wEventType* events, int count);
WINPR_API wEventType* PubSub_FindEventType(wPubSub* pubSub, const char* EventName);

WINPR_API int PubSub_Subscribe(wPubSub* pubSub, const char* EventName, pEventHandler EventHandler);
WINPR_API int PubSub_Unsubscribe(wPubSub* pubSub, const char* EventName, pEventHandler EventHandler);

WINPR_API int PubSub_OnEvent(wPubSub* pubSub, const char* EventName, void* context, wEventArgs* e);

WINPR_API wPubSub* PubSub_New(BOOL synchronized);
WINPR_API void PubSub_Free(wPubSub* pubSub);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_COLLECTIONS_H */
