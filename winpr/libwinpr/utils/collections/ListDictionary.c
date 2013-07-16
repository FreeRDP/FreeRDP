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

	if (!listDictionary->head)
		return 0;

	item = listDictionary->head;

	while (item)
	{
		count++;
		item = item->next;
	}

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
 * Methods
 */

/**
 * Adds an entry with the specified key and value into the ListDictionary.
 */

void ListDictionary_Add(wListDictionary* listDictionary, void* key, void* value)
{
	wListDictionaryItem* item;
	wListDictionaryItem* lastItem;

	item = (wListDictionaryItem*) malloc(sizeof(wListDictionaryItem));

	item->key = key;
	item->value = value;

	item->next = NULL;

	if (!listDictionary->head)
	{
		listDictionary->head = item;
		return;
	}

	lastItem = listDictionary->head;

	while (lastItem->next)
		lastItem = lastItem->next;

	lastItem->next = item;
}

/**
 * Removes all entries from the ListDictionary.
 */

void ListDictionary_Clear(wListDictionary* listDictionary)
{
	wListDictionaryItem* item;
	wListDictionaryItem* nextItem;

	if (!listDictionary->head)
		return;

	item = listDictionary->head;

	while (item)
	{
		nextItem = item->next;
		free(item);
		item = nextItem;
	}

	listDictionary->head = NULL;
}

/**
 * Determines whether the ListDictionary contains a specific key.
 */

BOOL ListDictionary_Contains(wListDictionary* listDictionary, void* key)
{
	wListDictionaryItem* item;

	if (!listDictionary->head)
		return FALSE;

	item = listDictionary->head;

	while (item)
	{
		if (item->key == key)
			break;

		item = item->next;
	}

	return (item) ? TRUE : FALSE;
}

/**
 * Removes the entry with the specified key from the ListDictionary.
 */

void ListDictionary_Remove(wListDictionary* listDictionary, void* key)
{
	wListDictionaryItem* item;
	wListDictionaryItem* prevItem;

	if (!listDictionary->head)
		return;

	item = listDictionary->head;

	if (listDictionary->head->key == key)
	{
		listDictionary->head = listDictionary->head->next;
		free(item);
		return;
	}

	if (!item->next)
		return;

	prevItem = item;
	item = item->next;

	while (item)
	{
		if (item->key == key)
		{
			prevItem->next = item->next;
			free(item);
			break;
		}

		item = item->next;
	}
}

/**
 * Get an item value using key
 */

void* ListDictionary_GetItemValue(wListDictionary* listDictionary, void* key)
{
	wListDictionaryItem* item;

	if (!listDictionary->head)
		return NULL;

	item = listDictionary->head;

	while (item)
	{
		if (item->key == key)
			break;

		item = item->next;
	}

	return (item) ? item->value : NULL;
}

/**
 * Set an item value using key
 */

BOOL ListDictionary_SetItemValue(wListDictionary* listDictionary, void* key, void* value)
{
	wListDictionaryItem* item;

	if (!listDictionary->head)
		return FALSE;

	item = listDictionary->head;

	while (item)
	{
		if (item->key == key)
			break;

		item = item->next;
	}

	if (item)
		item->value = value;

	return (item) ? TRUE : FALSE;
}

/**
 * Construction, Destruction
 */

wListDictionary* ListDictionary_New(BOOL synchronized)
{
	wListDictionary* listDictionary = NULL;

	listDictionary = (wListDictionary*) malloc(sizeof(wListDictionary));

	if (listDictionary)
	{
		listDictionary->synchronized = synchronized;

		listDictionary->head = NULL;
	}

	return listDictionary;
}

void ListDictionary_Free(wListDictionary* listDictionary)
{
	if (listDictionary)
	{
		ListDictionary_Clear(listDictionary);
		free(listDictionary);
	}
}

