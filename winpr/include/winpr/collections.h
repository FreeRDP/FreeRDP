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
#include <stdarg.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/assert.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef void* (*OBJECT_NEW_FN)(const void* val);
	typedef void (*OBJECT_INIT_FN)(void* obj);
	typedef void (*OBJECT_UNINIT_FN)(void* obj);
	typedef void (*OBJECT_FREE_FN)(void* obj);
	typedef BOOL (*OBJECT_EQUALS_FN)(const void* objA, const void* objB);

	/** @struct wObject
	 *  @brief This struct contains function pointer to initialize/free objects
	 *
	 *  @var fnObjectNew  A new function that creates a clone of the input
	 *  @var fnObjectInit A function initializing an object, but not allocating it
	 *  @var fnObjectUninit A function to deinitialize an object, but not free it
	 *  @var fnObjectFree A function freeing an object
	 *  @var fnObjectEquals A function to compare two objects
	 */
	typedef struct
	{
		OBJECT_NEW_FN fnObjectNew;
		OBJECT_INIT_FN fnObjectInit;
		OBJECT_UNINIT_FN fnObjectUninit;
		OBJECT_FREE_FN fnObjectFree;
		OBJECT_EQUALS_FN fnObjectEquals;
	} wObject;

	/* utility function with compatible arguments for string data */

	/** @brief helper function to clone a string
	 *  @param pvstr the source string to clone
	 *  @return A clone of the source or \b NULL
	 *  @since version 3.3.0
	 */
	WINPR_API void* winpr_ObjectStringClone(const void* pvstr);

	/** @brief helper function to clone a WCHAR string
	 *  @param pvstr the source string to clone
	 *  @return A clone of the source or \b NULL
	 *  @since version 3.3.0
	 */
	WINPR_API void* winpr_ObjectWStringClone(const void* pvstr);

	/** @brief helper function to free a (WCHAR) string
	 *  @param pvstr the string to free
	 *  @since version 3.3.0
	 */
	WINPR_API void winpr_ObjectStringFree(void* pvstr);

	/* System.Collections.Queue */

	typedef struct s_wQueue wQueue;

	/** @brief Return the number of elements in the queue
	 *
	 *  @param queue A pointer to a queue, must not be \b NULL
	 *
	 *  @return the number of objects queued
	 */
	WINPR_API size_t Queue_Count(wQueue* queue);

	/** @brief Mutex-Lock a queue
	 *
	 *  @param queue A pointer to a queue, must not be \b NULL
	 */
	WINPR_API void Queue_Lock(wQueue* queue);

	/** @brief Mutex-Unlock a queue
	 *
	 *  @param queue A pointer to a queue, must not be \b NULL
	 */
	WINPR_API void Queue_Unlock(wQueue* queue);

	/** @brief Get an event handle for the queue, usable by \b WaitForSingleObject or \b
	 * WaitForMultipleObjects
	 *
	 *  @param queue A pointer to a queue, must not be \b NULL
	 */
	WINPR_API HANDLE Queue_Event(wQueue* queue);

	/** @brief Mutex-Lock a queue
	 *
	 *  @param queue A pointer to a queue, must not be \b NULL
	 *
	 *  @return A pointer to a \b wObject that contains the allocation/cleanup handlers for queue
	 * elements
	 */
	WINPR_API wObject* Queue_Object(wQueue* queue);

	/** @brief Remove all elements from a queue, call \b wObject cleanup functions \b fnObjectFree
	 *
	 *  @param queue A pointer to a queue, must not be \b NULL
	 */
	WINPR_API void Queue_Clear(wQueue* queue);

	/** @brief Check if the queue contains an object
	 *
	 *  @param queue A pointer to a queue, must not be \b NULL
	 *  @param obj The object to look for. \b fnObjectEquals is called internally
	 *
	 *  @return \b TRUE if the object was found, \b FALSE otherwise.
	 */
	WINPR_API BOOL Queue_Contains(wQueue* queue, const void* obj);

	/** \brief Pushes a new element into the queue.
	 *  If a \b fnObjectNew is set, the element is copied and the queue takes
	 *  ownership of the memory, otherwise the ownership stays with the caller.
	 *
	 *  \param queue The queue to operate on
	 *  \param obj A pointer to the object to queue
	 *
	 *  \return TRUE for success, FALSE if failed.
	 */
	WINPR_API BOOL Queue_Enqueue(wQueue* queue, const void* obj);

	/** \brief returns the element at the top of the queue. The element is removed from the queue,
	 *  ownership of the element is passed on to the caller.
	 *
	 *  \param queue The queue to check
	 *
	 *  \return NULL if empty, a pointer to the memory on top of the queue otherwise.
	 */
	WINPR_API void* Queue_Dequeue(wQueue* queue);

	/** \brief returns the element at the top of the queue. The element is not removed from the
	 * queue, ownership of the element stays with the queue.
	 *
	 *  \param queue The queue to check
	 *
	 *  \return NULL if empty, a pointer to the memory on top of the queue otherwise.
	 */
	WINPR_API void* Queue_Peek(wQueue* queue);

	/** \brief Removes the element at the top of the queue. If fnObjectFree is set, the element is
	 * freed. This can be used in combination with Queue_Peek to handle an element and discard it
	 * with this function afterward. An alternative is Queue_Dequeue with calling the appropriate
	 * free function afterward.
	 *
	 *  \param queue The queue to operate on
	 */
	WINPR_API void Queue_Discard(wQueue* queue);

	/** @brief Clean up a queue, free all resources (e.g. calls \b Queue_Clear)
	 *
	 *  @param queue The queue to free, may be \b NULL
	 */
	WINPR_API void Queue_Free(wQueue* queue);

	/** @brief Creates a new queue
	 *
	 *  @return A newly allocated queue or \b NULL in case of failure
	 */
	WINPR_ATTR_MALLOC(Queue_Free, 1)
	WINPR_API wQueue* Queue_New(BOOL synchronized, SSIZE_T capacity, SSIZE_T growthFactor);

	/* System.Collections.Stack */

	typedef struct s_wStack wStack;

	WINPR_API size_t Stack_Count(wStack* stack);
	WINPR_API BOOL Stack_IsSynchronized(wStack* stack);

	WINPR_API wObject* Stack_Object(wStack* stack);

	WINPR_API void Stack_Clear(wStack* stack);
	WINPR_API BOOL Stack_Contains(wStack* stack, const void* obj);

	WINPR_API void Stack_Push(wStack* stack, void* obj);
	WINPR_API void* Stack_Pop(wStack* stack);

	WINPR_API void* Stack_Peek(wStack* stack);

	WINPR_API void Stack_Free(wStack* stack);

	WINPR_ATTR_MALLOC(Stack_Free, 1)
	WINPR_API wStack* Stack_New(BOOL synchronized);

	/* System.Collections.ArrayList */

	typedef struct s_wArrayList wArrayList;

	WINPR_API size_t ArrayList_Capacity(wArrayList* arrayList);
	WINPR_API size_t ArrayList_Count(wArrayList* arrayList);
	WINPR_API size_t ArrayList_Items(wArrayList* arrayList, ULONG_PTR** ppItems);
	WINPR_API BOOL ArrayList_IsFixedSized(wArrayList* arrayList);
	WINPR_API BOOL ArrayList_IsReadOnly(wArrayList* arrayList);
	WINPR_API BOOL ArrayList_IsSynchronized(wArrayList* arrayList);

	WINPR_API void ArrayList_Lock(wArrayList* arrayList);
	WINPR_API void ArrayList_Unlock(wArrayList* arrayList);

	WINPR_API void* ArrayList_GetItem(wArrayList* arrayList, size_t index);
	WINPR_API BOOL ArrayList_SetItem(wArrayList* arrayList, size_t index, const void* obj);

	WINPR_API wObject* ArrayList_Object(wArrayList* arrayList);

	typedef BOOL (*ArrayList_ForEachFkt)(void* data, size_t index, va_list ap);

	WINPR_API BOOL ArrayList_ForEach(wArrayList* arrayList, ArrayList_ForEachFkt fkt, ...);
	WINPR_API BOOL ArrayList_ForEachAP(wArrayList* arrayList, ArrayList_ForEachFkt fkt, va_list ap);

	WINPR_API void ArrayList_Clear(wArrayList* arrayList);
	WINPR_API BOOL ArrayList_Contains(wArrayList* arrayList, const void* obj);

#if defined(WITH_WINPR_DEPRECATED)
	WINPR_API WINPR_DEPRECATED(int ArrayList_Add(wArrayList* arrayList, const void* obj));
#endif

	WINPR_API BOOL ArrayList_Append(wArrayList* arrayList, const void* obj);
	WINPR_API BOOL ArrayList_Insert(wArrayList* arrayList, size_t index, const void* obj);

	WINPR_API BOOL ArrayList_Remove(wArrayList* arrayList, const void* obj);
	WINPR_API BOOL ArrayList_RemoveAt(wArrayList* arrayList, size_t index);

	WINPR_API SSIZE_T ArrayList_IndexOf(wArrayList* arrayList, const void* obj, SSIZE_T startIndex,
	                                    SSIZE_T count);
	WINPR_API SSIZE_T ArrayList_LastIndexOf(wArrayList* arrayList, const void* obj,
	                                        SSIZE_T startIndex, SSIZE_T count);

	WINPR_API void ArrayList_Free(wArrayList* arrayList);

	WINPR_ATTR_MALLOC(ArrayList_Free, 1)
	WINPR_API wArrayList* ArrayList_New(BOOL synchronized);

	/* System.Collections.DictionaryBase */

	/* System.Collections.Specialized.ListDictionary */
	typedef struct s_wListDictionary wListDictionary;

	/** @brief Get the \b wObject function pointer struct for the \b key of the dictionary.
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *
	 *  @return a \b wObject used to initialize the key object, \b NULL in case of failure
	 */
	WINPR_API wObject* ListDictionary_KeyObject(wListDictionary* listDictionary);

	/** @brief Get the \b wObject function pointer struct for the \b value of the dictionary.
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *
	 *  @return a \b wObject used to initialize the value object, \b NULL in case of failure
	 */
	WINPR_API wObject* ListDictionary_ValueObject(wListDictionary* listDictionary);

	/** @brief Return the number of entries in the dictionary
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *
	 *  @return the number of entries
	 */
	WINPR_API size_t ListDictionary_Count(wListDictionary* listDictionary);

	/** @brief mutex-lock a dictionary
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 */
	WINPR_API void ListDictionary_Lock(wListDictionary* listDictionary);
	/** @brief mutex-unlock a dictionary
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 */
	WINPR_API void ListDictionary_Unlock(wListDictionary* listDictionary);

	/** @brief mutex-lock a dictionary
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *  @param key The key identifying the entry, if set cloned with \b fnObjectNew
	 *  @param value The value to store for the \b key. May be \b NULL. if set cloned with \b
	 * fnObjectNew
	 *
	 *  @return \b TRUE for successfull addition, \b FALSE for failure
	 */
	WINPR_API BOOL ListDictionary_Add(wListDictionary* listDictionary, const void* key,
	                                  const void* value);

	/** @brief Remove an item from the dictionary and return the value. Cleanup is up to the caller.
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *  @param key The key identifying the entry
	 *
	 *  @return a pointer to the value stored or \b NULL in case of failure or not found
	 */
	WINPR_API void* ListDictionary_Take(wListDictionary* listDictionary, const void* key);

	/** @brief Remove an item from the dictionary and call \b fnObjectFree for key and value
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *  @param key The key identifying the entry
	 */
	WINPR_API void ListDictionary_Remove(wListDictionary* listDictionary, const void* key);

	/** @brief Remove the head item from the dictionary and return the value. Cleanup is up to the
	 * caller.
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *
	 *  @return a pointer to the value stored or \b NULL in case of failure or not found
	 */
	WINPR_API void* ListDictionary_Take_Head(wListDictionary* listDictionary);

	/** @brief Remove the head item from the dictionary and call \b fnObjectFree for key and value
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 */
	WINPR_API void ListDictionary_Remove_Head(wListDictionary* listDictionary);

	/** @brief Remove all items from the dictionary and call \b fnObjectFree for key and value
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 */
	WINPR_API void ListDictionary_Clear(wListDictionary* listDictionary);

	/** @brief Check if a dictionary contains \b key (\b fnObjectEquals of the key object is called)
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *  @param key A key to look for
	 *
	 *  @return \b TRUE if found, \b FALSE otherwise
	 */
	WINPR_API BOOL ListDictionary_Contains(wListDictionary* listDictionary, const void* key);

	/** @brief return all keys the dictionary contains
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *  @param ppKeys A pointer to a \b ULONG_PTR array that will hold the result keys. Call \b free
	 * if no longer required
	 *
	 *  @return the number of keys found in the dictionary or \b 0 if \b ppKeys is \b NULL
	 */
	WINPR_API size_t ListDictionary_GetKeys(wListDictionary* listDictionary, ULONG_PTR** ppKeys);

	/** @brief Get the value in the dictionary for a \b key. The ownership of the data stays with
	 * the dictionary.
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *  @param key A key to look for (\b fnObjectEquals of the key object is called)
	 *
	 *  @return A pointer to the data in the dictionary or \b NULL if not found
	 */
	WINPR_API void* ListDictionary_GetItemValue(wListDictionary* listDictionary, const void* key);

	/** @brief Set the value in the dictionary for a \b key. The entry must already exist, \b value
	 * is copied if \b fnObjectNew is set
	 *
	 *  @param listDictionary A dictionary to query, must not be \b NULL
	 *  @param key A key to look for (\b fnObjectEquals of the key object is called)
	 *  @param value A pointer to the value to set
	 *
	 *  @return \b TRUE for success, \b FALSE in case of failure
	 */
	WINPR_API BOOL ListDictionary_SetItemValue(wListDictionary* listDictionary, const void* key,
	                                           const void* value);

	/** @brief Free memory allocated by a dictionary. Calls \b ListDictionary_Clear
	 *
	 *  @param listDictionary A dictionary to query, may be \b NULL
	 */
	WINPR_API void ListDictionary_Free(wListDictionary* listDictionary);

	/** @brief allocate a new dictionary
	 *
	 *  @param synchronized Create the dictionary with automatic mutex lock
	 *
	 *  @return A newly allocated dictionary or \b NULL in case of failure
	 */
	WINPR_ATTR_MALLOC(ListDictionary_Free, 1)
	WINPR_API wListDictionary* ListDictionary_New(BOOL synchronized);

	/* System.Collections.Generic.LinkedList<T> */

	typedef struct s_wLinkedList wLinkedList;

	/** @brief Return the current number of elements in the linked list
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 * @return the number of elements in the list
	 */
	WINPR_API size_t LinkedList_Count(wLinkedList* list);

	/** @brief Return the first element of the list, ownership stays with the list
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 *  @return A pointer to the element or \b NULL if empty
	 */
	WINPR_API void* LinkedList_First(wLinkedList* list);

	/** @brief Return the last element of the list, ownership stays with the list
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 *  @return A pointer to the element or \b NULL if empty
	 */
	WINPR_API void* LinkedList_Last(wLinkedList* list);

	/** @brief Check if the linked list contains a value
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *  @param value A value to check for
	 *
	 *  @return \b TRUE if found, \b FALSE otherwise
	 */
	WINPR_API BOOL LinkedList_Contains(wLinkedList* list, const void* value);

	/** @brief Remove all elements of the linked list. \b fnObjectUninit and \b fnObjectFree are
	 * called for each entry
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 */
	WINPR_API void LinkedList_Clear(wLinkedList* list);

	/** @brief Add a new element at the start of the linked list. \b fnObjectNew and \b fnObjectInit
	 * is called for the new entry
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *  @param value The value to add
	 *
	 * @return \b TRUE if successful, \b FALSE otherwise.
	 */
	WINPR_API BOOL LinkedList_AddFirst(wLinkedList* list, const void* value);

	/** @brief Add a new element at the end of the linked list. \b fnObjectNew and \b fnObjectInit
	 * is called for the new entry
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *  @param value The value to add
	 *
	 * @return \b TRUE if successful, \b FALSE otherwise.
	 */
	WINPR_API BOOL LinkedList_AddLast(wLinkedList* list, const void* value);

	/** @brief Remove a element identified by \b value from the linked list. \b fnObjectUninit and
	 * \b fnObjectFree is called for the entry
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *  @param value The value to remove
	 *
	 * @return \b TRUE if successful, \b FALSE otherwise.
	 */
	WINPR_API BOOL LinkedList_Remove(wLinkedList* list, const void* value);

	/** @brief Remove the first element from the linked list. \b fnObjectUninit and \b fnObjectFree
	 * is called for the entry
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 */
	WINPR_API void LinkedList_RemoveFirst(wLinkedList* list);

	/** @brief Remove the last element from the linked list. \b fnObjectUninit and \b fnObjectFree
	 * is called for the entry
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 */
	WINPR_API void LinkedList_RemoveLast(wLinkedList* list);

	/** @brief Move enumerator to the first element
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 */
	WINPR_API void LinkedList_Enumerator_Reset(wLinkedList* list);

	/** @brief Return the value for the current position of the enumerator
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 * @return A pointer to the current entry or \b NULL
	 */
	WINPR_API void* LinkedList_Enumerator_Current(wLinkedList* list);

	/** @brief Move enumerator to the next element
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 *  @return \b TRUE if the move was successful, \b FALSE if not (e.g. no more entries)
	 */
	WINPR_API BOOL LinkedList_Enumerator_MoveNext(wLinkedList* list);

	/** @brief Free a linked list
	 *
	 *  @param list A pointer to the list, may be \b NULL
	 */
	WINPR_API void LinkedList_Free(wLinkedList* list);

	/** @brief Allocate a linked list
	 *
	 * @return A pointer to the newly allocated linked list or \b NULL in case of failure
	 */
	WINPR_ATTR_MALLOC(LinkedList_Free, 1)
	WINPR_API wLinkedList* LinkedList_New(void);

	/** @brief Return the \b wObject function pointers for list elements
	 *
	 *  @param list A pointer to the list, must not be \b NULL
	 *
	 *  @return A pointer to the wObject or \b NULL in case of failure
	 */
	WINPR_API wObject* LinkedList_Object(wLinkedList* list);

	/* System.Collections.Generic.KeyValuePair<TKey,TValue> */

	/* Countdown Event */

	typedef struct CountdownEvent wCountdownEvent;

	/** @brief return the current event count of the CountdownEvent
	 *
	 *  @param countdown A pointer to a CountdownEvent, must not be \b NULL
	 *
	 *  @return The current event count
	 */
	WINPR_API size_t CountdownEvent_CurrentCount(wCountdownEvent* countdown);

	/** @brief return the initial event count of the CountdownEvent
	 *
	 *  @param countdown A pointer to a CountdownEvent, must not be \b NULL
	 *
	 *  @return The initial event count
	 */
	WINPR_API size_t CountdownEvent_InitialCount(wCountdownEvent* countdown);

	/** @brief return the current event state of the CountdownEvent
	 *
	 *  @param countdown A pointer to a CountdownEvent, must not be \b NULL
	 *
	 *  @return \b TRUE if set, \b FALSE otherwise
	 */
	WINPR_API BOOL CountdownEvent_IsSet(wCountdownEvent* countdown);

	/** @brief return the event HANDLE of the CountdownEvent to be used by \b WaitForSingleObject or
	 * \b WaitForMultipleObjects
	 *
	 *  @param countdown A pointer to a CountdownEvent, must not be \b NULL
	 *
	 *  @return a \b HANDLE or \b NULL in case of failure
	 */
	WINPR_API HANDLE CountdownEvent_WaitHandle(wCountdownEvent* countdown);

	/** @brief add \b signalCount to the current event count of the CountdownEvent
	 *
	 *  @param countdown A pointer to a CountdownEvent, must not be \b NULL
	 *  @param signalCount The amount to add to CountdownEvent
	 *
	 */
	WINPR_API void CountdownEvent_AddCount(wCountdownEvent* countdown, size_t signalCount);

	/** @brief Increase the current event signal state of the CountdownEvent
	 *
	 *  @param countdown A pointer to a CountdownEvent, must not be \b NULL
	 *  @param signalCount The amount of signaled events to add
	 *
	 *  @return \b TRUE if event is set, \b FALSE otherwise
	 */
	WINPR_API BOOL CountdownEvent_Signal(wCountdownEvent* countdown, size_t signalCount);

	/** @brief reset the CountdownEvent
	 *
	 *  @param countdown A pointer to a CountdownEvent, must not be \b NULL
	 *
	 */
	WINPR_API void CountdownEvent_Reset(wCountdownEvent* countdown, size_t count);

	/** @brief Free a CountdownEvent
	 *
	 *  @param countdown A pointer to a CountdownEvent, may be \b NULL
	 */
	WINPR_API void CountdownEvent_Free(wCountdownEvent* countdown);

	/** @brief Allocte a CountdownEvent with \b initialCount
	 *
	 *  @param initialCount The initial value of the event
	 *
	 *  @return The newly allocated event or \b NULL in case of failure
	 */
	WINPR_ATTR_MALLOC(CountdownEvent_Free, 1)
	WINPR_API wCountdownEvent* CountdownEvent_New(size_t initialCount);

	/* Hash Table */

	typedef UINT32 (*HASH_TABLE_HASH_FN)(const void* key);

	typedef struct s_wHashTable wHashTable;

	typedef BOOL (*HASH_TABLE_FOREACH_FN)(const void* key, void* value, void* arg);

	WINPR_API size_t HashTable_Count(wHashTable* table);

#if defined(WITH_WINPR_DEPRECATED)
	WINPR_API WINPR_DEPRECATED(int HashTable_Add(wHashTable* table, const void* key,
	                                             const void* value));
#endif

	WINPR_API BOOL HashTable_Insert(wHashTable* table, const void* key, const void* value);
	WINPR_API BOOL HashTable_Remove(wHashTable* table, const void* key);
	WINPR_API void HashTable_Clear(wHashTable* table);
	WINPR_API BOOL HashTable_Contains(wHashTable* table, const void* key);
	WINPR_API BOOL HashTable_ContainsKey(wHashTable* table, const void* key);
	WINPR_API BOOL HashTable_ContainsValue(wHashTable* table, const void* value);
	WINPR_API void* HashTable_GetItemValue(wHashTable* table, const void* key);
	WINPR_API BOOL HashTable_SetItemValue(wHashTable* table, const void* key, const void* value);
	WINPR_API size_t HashTable_GetKeys(wHashTable* table, ULONG_PTR** ppKeys);
	WINPR_API BOOL HashTable_Foreach(wHashTable* table, HASH_TABLE_FOREACH_FN fn, VOID* arg);

	WINPR_API UINT32 HashTable_PointerHash(const void* pointer);
	WINPR_API BOOL HashTable_PointerCompare(const void* pointer1, const void* pointer2);

	WINPR_API UINT32 HashTable_StringHash(const void* key);
	WINPR_API BOOL HashTable_StringCompare(const void* string1, const void* string2);
	WINPR_API void* HashTable_StringClone(const void* str);
	WINPR_API void HashTable_StringFree(void* str);

	WINPR_API void HashTable_Free(wHashTable* table);

	WINPR_ATTR_MALLOC(HashTable_Free, 1)
	WINPR_API wHashTable* HashTable_New(BOOL synchronized);

	WINPR_API void HashTable_Lock(wHashTable* table);
	WINPR_API void HashTable_Unlock(wHashTable* table);

	WINPR_API wObject* HashTable_KeyObject(wHashTable* table);
	WINPR_API wObject* HashTable_ValueObject(wHashTable* table);

	WINPR_API BOOL HashTable_SetHashFunction(wHashTable* table, HASH_TABLE_HASH_FN fn);

	/* Utility function to setup hash table for strings */
	WINPR_API BOOL HashTable_SetupForStringData(wHashTable* table, BOOL stringValues);

	/* BufferPool */

	typedef struct s_wBufferPool wBufferPool;

	WINPR_API SSIZE_T BufferPool_GetPoolSize(wBufferPool* pool);
	WINPR_API SSIZE_T BufferPool_GetBufferSize(wBufferPool* pool, const void* buffer);

	WINPR_API void* BufferPool_Take(wBufferPool* pool, SSIZE_T bufferSize);
	WINPR_API BOOL BufferPool_Return(wBufferPool* pool, void* buffer);
	WINPR_API void BufferPool_Clear(wBufferPool* pool);

	WINPR_API void BufferPool_Free(wBufferPool* pool);

	WINPR_ATTR_MALLOC(BufferPool_Free, 1)
	WINPR_API wBufferPool* BufferPool_New(BOOL synchronized, SSIZE_T fixedSize, DWORD alignment);

	/* ObjectPool */

	typedef struct s_wObjectPool wObjectPool;

	WINPR_API void* ObjectPool_Take(wObjectPool* pool);
	WINPR_API void ObjectPool_Return(wObjectPool* pool, void* obj);
	WINPR_API void ObjectPool_Clear(wObjectPool* pool);

	WINPR_API wObject* ObjectPool_Object(wObjectPool* pool);

	WINPR_API void ObjectPool_Free(wObjectPool* pool);

	WINPR_ATTR_MALLOC(ObjectPool_Free, 1)
	WINPR_API wObjectPool* ObjectPool_New(BOOL synchronized);

	/* Message Queue */

	typedef struct s_wMessage wMessage;

	typedef void (*MESSAGE_FREE_FN)(wMessage* message);

	struct s_wMessage
	{
		UINT32 id;
		void* context;
		void* wParam;
		void* lParam;
		UINT64 time;
		MESSAGE_FREE_FN Free;
	};

	typedef struct s_wMessageQueue wMessageQueue;

#define WMQ_QUIT 0xFFFFFFFF

	WINPR_API wObject* MessageQueue_Object(wMessageQueue* queue);
	WINPR_API HANDLE MessageQueue_Event(wMessageQueue* queue);
	WINPR_API BOOL MessageQueue_Wait(wMessageQueue* queue);
	WINPR_API size_t MessageQueue_Size(wMessageQueue* queue);

	WINPR_API BOOL MessageQueue_Dispatch(wMessageQueue* queue, const wMessage* message);
	WINPR_API BOOL MessageQueue_Post(wMessageQueue* queue, void* context, UINT32 type, void* wParam,
	                                 void* lParam);
	WINPR_API BOOL MessageQueue_PostQuit(wMessageQueue* queue, int nExitCode);

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
	WINPR_API int MessageQueue_Clear(wMessageQueue* queue);

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
	WINPR_ATTR_MALLOC(MessageQueue_Free, 1)
	WINPR_API wMessageQueue* MessageQueue_New(const wObject* callback);

	/* Message Pipe */

	typedef struct
	{
		wMessageQueue* In;
		wMessageQueue* Out;
	} wMessagePipe;

	WINPR_API void MessagePipe_PostQuit(wMessagePipe* pipe, int nExitCode);

	WINPR_API void MessagePipe_Free(wMessagePipe* pipe);

	WINPR_ATTR_MALLOC(MessagePipe_Free, 1)
	WINPR_API wMessagePipe* MessagePipe_New(void);

	/* Publisher/Subscriber Pattern */

	typedef struct
	{
		DWORD Size;
		const char* Sender;
	} wEventArgs;

	typedef void (*pEventHandler)(void* context, const wEventArgs* e);

#ifdef __cplusplus
#define WINPR_EVENT_CAST(t, val) reinterpret_cast<t>(val)
#else
#define WINPR_EVENT_CAST(t, val) (t)(val)
#endif

#define MAX_EVENT_HANDLERS 32

	typedef struct
	{
		const char* EventName;
		wEventArgs EventArgs;
		size_t EventHandlerCount;
		pEventHandler EventHandlers[MAX_EVENT_HANDLERS];
	} wEventType;

#define EventArgsInit(_event_args, _sender)       \
	memset(_event_args, 0, sizeof(*_event_args)); \
	(_event_args)->e.Size = sizeof(*_event_args); \
	(_event_args)->e.Sender = _sender

#define DEFINE_EVENT_HANDLER(name) \
	typedef void (*p##name##EventHandler)(void* context, const name##EventArgs* e)

#define DEFINE_EVENT_RAISE(name)                                                                \
	static INLINE int PubSub_On##name(wPubSub* pubSub, void* context, const name##EventArgs* e) \
	{                                                                                           \
		WINPR_ASSERT(e);                                                                        \
		return PubSub_OnEvent(pubSub, #name, context, &e->e);                                   \
	}

#define DEFINE_EVENT_SUBSCRIBE(name)                                                              \
	static INLINE int PubSub_Subscribe##name(wPubSub* pubSub, p##name##EventHandler EventHandler) \
	{                                                                                             \
		return PubSub_Subscribe(pubSub, #name, EventHandler);                                     \
	}

#define DEFINE_EVENT_UNSUBSCRIBE(name)                                             \
	static INLINE int PubSub_Unsubscribe##name(wPubSub* pubSub,                    \
	                                           p##name##EventHandler EventHandler) \
	{                                                                              \
		return PubSub_Unsubscribe(pubSub, #name, EventHandler);                    \
	}

#define DEFINE_EVENT_BEGIN(name) \
	typedef struct               \
	{                            \
		wEventArgs e;

#define DEFINE_EVENT_END(name)   \
	}                            \
	name##EventArgs;             \
	DEFINE_EVENT_HANDLER(name);  \
	DEFINE_EVENT_RAISE(name)     \
	DEFINE_EVENT_SUBSCRIBE(name) \
	DEFINE_EVENT_UNSUBSCRIBE(name)

#define DEFINE_EVENT_ENTRY(name)                     \
	{                                                \
#name, { sizeof(name##EventArgs), NULL }, 0, \
		{                                            \
			NULL                                     \
		}                                            \
	}

	typedef struct s_wPubSub wPubSub;

	WINPR_API void PubSub_Lock(wPubSub* pubSub);
	WINPR_API void PubSub_Unlock(wPubSub* pubSub);

	WINPR_API wEventType* PubSub_GetEventTypes(wPubSub* pubSub, size_t* count);
	WINPR_API void PubSub_AddEventTypes(wPubSub* pubSub, wEventType* events, size_t count);
	WINPR_API wEventType* PubSub_FindEventType(wPubSub* pubSub, const char* EventName);

	WINPR_API int PubSub_Subscribe(wPubSub* pubSub, const char* EventName, ...);
	WINPR_API int PubSub_Unsubscribe(wPubSub* pubSub, const char* EventName, ...);

	WINPR_API int PubSub_OnEvent(wPubSub* pubSub, const char* EventName, void* context,
	                             const wEventArgs* e);

	WINPR_API void PubSub_Free(wPubSub* pubSub);

	WINPR_ATTR_MALLOC(PubSub_Free, 1)
	WINPR_API wPubSub* PubSub_New(BOOL synchronized);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_COLLECTIONS_H */
