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

#include <winpr/config.h>
#include <winpr/assert.h>
#include <winpr/crt.h>

#include <winpr/collections.h>

typedef struct s_wListDictionaryItem wListDictionaryItem;

struct s_wListDictionaryItem
{
	void* key;
	void* value;

	wListDictionaryItem* next;
};

struct s_wListDictionary
{
	BOOL synchronized;
	CRITICAL_SECTION lock;

	wListDictionaryItem* head;
	wObject objectKey;
	wObject objectValue;
};

/**
 * C equivalent of the C# ListDictionary Class:
 * http://msdn.microsoft.com/en-us/library/system.collections.specialized.listdictionary.aspx
 *
 * Internal implementation uses a singly-linked list
 */

WINPR_API wObject* ListDictionary_KeyObject(wListDictionary* _dictionary)
{
	WINPR_ASSERT(_dictionary);
	return &_dictionary->objectKey;
}

WINPR_API wObject* ListDictionary_ValueObject(wListDictionary* _dictionary)
{
	WINPR_ASSERT(_dictionary);
	return &_dictionary->objectValue;
}

/**
 * Properties
 */

/**
 * Gets the number of key/value pairs contained in the ListDictionary.
 */

size_t ListDictionary_Count(wListDictionary* listDictionary)
{
	size_t count = 0;

	WINPR_ASSERT(listDictionary);

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	if (listDictionary->head)
	{
		wListDictionaryItem* item = listDictionary->head;

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
 * Lock access to the ListDictionary
 */

void ListDictionary_Lock(wListDictionary* listDictionary)
{
	WINPR_ASSERT(listDictionary);

	EnterCriticalSection(&listDictionary->lock);
}

/**
 * Unlock access to the ListDictionary
 */

void ListDictionary_Unlock(wListDictionary* listDictionary)
{
	WINPR_ASSERT(listDictionary);

	LeaveCriticalSection(&listDictionary->lock);
}

/**
 * Methods
 */

/**
 * Gets the list of keys as an array
 */

size_t ListDictionary_GetKeys(wListDictionary* listDictionary, ULONG_PTR** ppKeys)
{
	ULONG_PTR* pKeys = NULL;

	WINPR_ASSERT(listDictionary);
	if (!ppKeys)
		return 0;

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	size_t count = 0;

	if (listDictionary->head)
	{
		wListDictionaryItem* item = listDictionary->head;

		while (item)
		{
			count++;
			item = item->next;
		}
	}

	if (count > 0)
	{
		pKeys = (ULONG_PTR*)calloc(count, sizeof(ULONG_PTR));

		if (!pKeys)
		{
			if (listDictionary->synchronized)
				LeaveCriticalSection(&listDictionary->lock);

			return -1;
		}
	}

	size_t index = 0;

	if (listDictionary->head)
	{
		wListDictionaryItem* item = listDictionary->head;

		while (item)
		{
			pKeys[index++] = (ULONG_PTR)item->key;
			item = item->next;
		}
	}

	*ppKeys = pKeys;

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return count;
}

static void item_free(wListDictionary* listDictionary, wListDictionaryItem* item)
{
	WINPR_ASSERT(listDictionary);

	if (item)
	{
		if (listDictionary->objectKey.fnObjectFree)
			listDictionary->objectKey.fnObjectFree(item->key);
		if (listDictionary->objectValue.fnObjectFree)
			listDictionary->objectValue.fnObjectFree(item->value);
	}
	free(item);
}

static void item_set(wListDictionary* listDictionary, wListDictionaryItem* item, const void* value)
{
	WINPR_ASSERT(listDictionary);
	WINPR_ASSERT(item);

	if (listDictionary->objectValue.fnObjectFree)
		listDictionary->objectValue.fnObjectFree(item->value);

	if (listDictionary->objectValue.fnObjectNew)
		item->value = listDictionary->objectValue.fnObjectNew(value);
	else
		item->value = (void*)(uintptr_t)value;
}

static wListDictionaryItem* new_item(wListDictionary* listDictionary, const void* key,
                                     const void* value)
{
	wListDictionaryItem* item = (wListDictionaryItem*)calloc(1, sizeof(wListDictionaryItem));
	if (!item)
		return NULL;

	if (listDictionary->objectKey.fnObjectNew)
		item->key = listDictionary->objectKey.fnObjectNew(key);
	else
		item->key = (void*)(uintptr_t)key;
	if (!item->key)
		goto fail;

	item_set(listDictionary, item, value);
	if (value && !item->value)
		goto fail;

	return item;

fail:
	item_free(listDictionary, item);
	return NULL;
}

/**
 * Adds an entry with the specified key and value into the ListDictionary.
 */

BOOL ListDictionary_Add(wListDictionary* listDictionary, const void* key, const void* value)
{
	BOOL ret = FALSE;

	WINPR_ASSERT(listDictionary);

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	wListDictionaryItem* item = new_item(listDictionary, key, value);

	if (!item)
		goto out_error;

	if (!listDictionary->head)
	{
		listDictionary->head = item;
	}
	else
	{
		wListDictionaryItem* lastItem = listDictionary->head;

		while (lastItem->next)
			lastItem = lastItem->next;

		lastItem->next = item;
	}

	ret = TRUE;
out_error:

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return ret;
}

/**
 * Removes all entries from the ListDictionary.
 */

void ListDictionary_Clear(wListDictionary* listDictionary)
{
	wListDictionaryItem* item = NULL;
	wListDictionaryItem* nextItem = NULL;

	WINPR_ASSERT(listDictionary);

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	if (listDictionary->head)
	{
		item = listDictionary->head;

		while (item)
		{
			nextItem = item->next;

			item_free(listDictionary, item);
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

BOOL ListDictionary_Contains(wListDictionary* listDictionary, const void* key)
{
	wListDictionaryItem* item = NULL;
	OBJECT_EQUALS_FN keyEquals = NULL;

	WINPR_ASSERT(listDictionary);

	if (listDictionary->synchronized)
		EnterCriticalSection(&(listDictionary->lock));

	keyEquals = listDictionary->objectKey.fnObjectEquals;
	item = listDictionary->head;

	while (item)
	{
		if (keyEquals(item->key, key))
			break;

		item = item->next;
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&(listDictionary->lock));

	return (item) ? TRUE : FALSE;
}

/**
 * Removes the entry with the specified key from the ListDictionary.
 */

static void* ListDictionary_RemoveOrTake(wListDictionary* listDictionary, const void* key,
                                         BOOL take)
{
	void* value = NULL;
	wListDictionaryItem* item = NULL;
	wListDictionaryItem* prevItem = NULL;
	OBJECT_EQUALS_FN keyEquals = NULL;

	WINPR_ASSERT(listDictionary);

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

			if (take)
			{
				value = item->value;
				item->value = NULL;
			}
			item_free(listDictionary, item);
			break;
		}

		prevItem = item;
		item = item->next;
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return value;
}

void ListDictionary_Remove(wListDictionary* listDictionary, const void* key)
{
	ListDictionary_RemoveOrTake(listDictionary, key, FALSE);
}

void* ListDictionary_Take(wListDictionary* listDictionary, const void* key)
{
	return ListDictionary_RemoveOrTake(listDictionary, key, TRUE);
}

/**
 * Removes the first (head) entry from the list
 */

static void* ListDictionary_Remove_Or_Take_Head(wListDictionary* listDictionary, BOOL take)
{
	wListDictionaryItem* item = NULL;
	void* value = NULL;

	WINPR_ASSERT(listDictionary);

	if (listDictionary->synchronized)
		EnterCriticalSection(&listDictionary->lock);

	if (listDictionary->head)
	{
		item = listDictionary->head;
		listDictionary->head = listDictionary->head->next;
		if (take)
		{
			value = item->value;
			item->value = NULL;
		}
		item_free(listDictionary, item);
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return value;
}

void ListDictionary_Remove_Head(wListDictionary* listDictionary)
{
	ListDictionary_Remove_Or_Take_Head(listDictionary, FALSE);
}

void* ListDictionary_Take_Head(wListDictionary* listDictionary)
{
	return ListDictionary_Remove_Or_Take_Head(listDictionary, TRUE);
}

/**
 * Get an item value using key
 */

void* ListDictionary_GetItemValue(wListDictionary* listDictionary, const void* key)
{
	void* value = NULL;
	wListDictionaryItem* item = NULL;
	OBJECT_EQUALS_FN keyEquals = NULL;

	WINPR_ASSERT(listDictionary);

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

BOOL ListDictionary_SetItemValue(wListDictionary* listDictionary, const void* key,
                                 const void* value)
{
	BOOL status = FALSE;
	wListDictionaryItem* item = NULL;
	OBJECT_EQUALS_FN keyEquals = NULL;

	WINPR_ASSERT(listDictionary);

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
			item_set(listDictionary, item, value);

		status = (item) ? TRUE : FALSE;
	}

	if (listDictionary->synchronized)
		LeaveCriticalSection(&listDictionary->lock);

	return status;
}

static BOOL default_equal_function(const void* obj1, const void* obj2)
{
	return (obj1 == obj2);
}
/**
 * Construction, Destruction
 */

wListDictionary* ListDictionary_New(BOOL synchronized)
{
	wListDictionary* listDictionary = (wListDictionary*)calloc(1, sizeof(wListDictionary));

	if (!listDictionary)
		return NULL;

	listDictionary->synchronized = synchronized;

	if (!InitializeCriticalSectionAndSpinCount(&(listDictionary->lock), 4000))
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
