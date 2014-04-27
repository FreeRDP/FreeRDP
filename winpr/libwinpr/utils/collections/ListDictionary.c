/**
 * WinPR: Windows Portable Runtime
 * System.Collections.Specialized.ListDictionary
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

#include <winpr/collections.h>

/**
 * C equivalent of the C# ListDictionary Class:
 * http://msdn.microsoft.com/en-us/library/system.collections.specialized.listdictionary.aspx
 *
 * Internal implementation uses a singly-linked list
 */

/**
 * Properties
 */

/**
 * Gets the number of key/value pairs contained in the ListDictionary.
 */

int ListDictionary_Count(wListDictionary* listDictionary)
{
	int count = 0;
	wListDictionaryItem* item;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	if (listDictionary->head)
	{
		item = listDictionary->head;

		while (item)
		{
			count++;
			item = item->next;
		}
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return count;
}

/**
 * Gets a value indicating whether the ListDictionary has a fixed size.
 */

BOOL ListDictionary_IsFixedSized(wListDictionary* listDictionary)
{
	return FALSE;
}

/**
 * Gets a value indicating whether the ListDictionary is read-only.
 */

BOOL ListDictionary_IsReadOnly(wListDictionary* listDictionary)
{
	return FALSE;
}

/**
 * Gets a value indicating whether the ListDictionary is synchronized (thread safe).
 */

BOOL ListDictionary_IsSynchronized(wListDictionary* listDictionary)
{
	return listDictionary->synchronized;
}

/**
 * Lock access to the ListDictionary
 */

void ListDictionary_Lock(wListDictionary* listDictionary)
{
	EnterCriticalSection(&listDictionary->lock);
}

/**
 * Unlock access to the ListDictionary
 */

void ListDictionary_Unlock(wListDictionary* listDictionary)
{
	LeaveCriticalSection(&listDictionary->lock);
}

/**
 * Methods
 */

/**
 * Gets the list of keys as an array
 */

int ListDictionary_GetKeys(wListDictionary* listDictionary, ULONG_PTR** ppKeys)
{
	int index;
	int count;
	ULONG_PTR* pKeys;
	wListDictionaryItem* item;

	if (!ppKeys)
		return -1;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	count = 0;

	if (listDictionary->head)
	{
		item = listDictionary->head;

		while (item)
		{
			count++;
			item = item->next;
		}
	}

	pKeys = (ULONG_PTR*) malloc(sizeof(ULONG_PTR) * count);
	ZeroMemory(pKeys, sizeof(ULONG_PTR) * count);

	index = 0;

	if (listDictionary->head)
	{
		item = listDictionary->head;

		while (item)
		{
			pKeys[index++] = (ULONG_PTR) item->key;
			item = item->next;
		}
	}

	*ppKeys = pKeys;

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return count;
}

/**
 * Adds an entry with the specified key and value into the ListDictionary.
 */

BOOL ListDictionary_Add(wListDictionary* listDictionary, void* key, void* value)
{
	wListDictionaryItem* item;
	wListDictionaryItem* lastItem;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	item = (wListDictionaryItem*) malloc(sizeof(wListDictionaryItem));
	if (!item)
		return FALSE;

	item->key = key;
	item->value = value;

	item->next = NULL;

	if (!listDictionary->head)
	{
		listDictionary->head = item;
	}
	else
	{
		lastItem = listDictionary->head;

		while (lastItem->next)
			lastItem = lastItem->next;

		lastItem->next = item;
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);
	return TRUE;
}

/**
 * Removes all entries from the ListDictionary.
 */

void ListDictionary_Clear(wListDictionary* listDictionary)
{
	wListDictionaryItem* item;
	wListDictionaryItem* nextItem;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	if (listDictionary->head)
	{
		item = listDictionary->head;

		while (item)
		{
			nextItem = item->next;
			if (listDictionary->objectValue.fnObjectFree)
				listDictionary->objectValue.fnObjectFree(item->value);
			free(item);
			item = nextItem;
		}

		listDictionary->head = NULL;
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);
}

/**
 * Determines whether the ListDictionary contains a specific key.
 */

BOOL ListDictionary_Contains(wListDictionary* listDictionary, void* key)
{
	wListDictionaryItem* item;
	OBJECT_EQUALS_FN keyEquals;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	keyEquals = listDictionary->objectKey.fnObjectEquals;
	item = listDictionary->head;

	while (item)
	{
		if (keyEquals(item->key, key))
			break;

		item = item->next;
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return (item) ? TRUE : FALSE;
}

/**
 * Removes the entry with the specified key from the ListDictionary.
 */

void* ListDictionary_Remove(wListDictionary* listDictionary, void* key)
{
	void* value = NULL;
	wListDictionaryItem* item;
	wListDictionaryItem* prevItem;
	OBJECT_EQUALS_FN keyEquals;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	keyEquals = listDictionary->objectKey.fnObjectEquals;

	item = listDictionary->head;
	prevItem = NULL;

	while (item)
	{
		if (keyEquals(item->key, key))
		{
			if (!prevItem)
				listDictionary->head = item->next;
			else
				prevItem->next = item->next;
			value = item->value;
			free(item);
			break;
		}

		prevItem = item;
		item = item->next;
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return value;
}

/**
 * Removes the first (head) entry from the list
 */

void *ListDictionary_Remove_Head(wListDictionary* listDictionary)
{
	wListDictionaryItem* item;
	void *value = NULL;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	if (listDictionary->head)
	{
		item = listDictionary->head;
		listDictionary->head = listDictionary->head->next;
		value = item->value;
		free(item);
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);
	return value;
}

/**
 * Get an item value using key
 */

void* ListDictionary_GetItemValue(wListDictionary* listDictionary, void* key)
{
	void* value = NULL;
	wListDictionaryItem* item = NULL;
	OBJECT_EQUALS_FN keyEquals;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	keyEquals = listDictionary->objectKey.fnObjectEquals;

	if (listDictionary->head)
	{
		item = listDictionary->head;

		while (item)
		{
			if (keyEquals(item->key, key))
				break;

			item = item->next;
		}
	}

	value = (item) ? item->value : NULL;

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return value;
}

/**
 * Set an item value using key
 */

BOOL ListDictionary_SetItemValue(wListDictionary* listDictionary, void* key, void* value)
{
	BOOL status = FALSE;
	wListDictionaryItem* item;
	OBJECT_EQUALS_FN keyEquals;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	keyEquals = listDictionary->objectKey.fnObjectEquals;
	if (listDictionary->head)
	{
		item = listDictionary->head;

		while (item)
		{
			if (keyEquals(item->key, key))
				break;

			item = item->next;
		}

		if (item)
		{
			if (listDictionary->objectValue.fnObjectFree)
				listDictionary->objectValue.fnObjectFree(item->value);
			item->value = value;
		}

		status = (item) ? TRUE : FALSE;
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return status;
}

static BOOL default_equal_function(void *obj1, void *obj2)
{
	return (obj1 == obj2);
}
/**
 * Construction, Destruction
 */

wListDictionary* ListDictionary_New(BOOL synchronized)
{
	wListDictionary* listDictionary = NULL;

	listDictionary = (wListDictionary*) calloc(1, sizeof(wListDictionary));
	if (!listDictionary)
		return NULL;

	listDictionary->synchronized = synchronized;

	if (!InitializeCriticalSectionAndSpinCount(&listDictionary->lock, 4000))
	{
		free(listDictionary);
		return NULL;
	}

	listDictionary->objectKey.fnObjectEquals = default_equal_function;
	listDictionary->objectValue.fnObjectEquals = default_equal_function;
	return listDictionary;
}

void ListDictionary_Free(wListDictionary* listDictionary)
{
	if (listDictionary)
	{
		ListDictionary_Clear(listDictionary);
		DeleteCriticalSection(&listDictionary->lock);
		free(listDictionary);
	}
}

