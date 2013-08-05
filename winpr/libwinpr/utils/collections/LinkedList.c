/**
 * WinPR: Windows Portable Runtime
 * System.Collections.Generic.LinkedList<T>
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
 * C equivalent of the C# LinkedList<T> Class:
 * http://msdn.microsoft.com/en-us/library/he2s3bh7.aspx
 *
 * Internal implementation uses a doubly-linked list
 */

/**
 * Properties
 */

/**
 * Gets the number of nodes actually contained in the LinkedList.
 */

int LinkedList_Count(wLinkedList* list)
{
	return list->count;
}

/**
 * Gets the first node of the LinkedList.
 */

void* LinkedList_First(wLinkedList* list)
{
	if (list->head)
		return list->head->value;
	else
		return NULL;
}

/**
 * Gets the last node of the LinkedList.
 */

void* LinkedList_Last(wLinkedList* list)
{
	if (list->tail)
		return list->tail->value;
	else
		return NULL;
}

/**
 * Methods
 */

/**
 * Determines whether the LinkedList contains a specific value.
 */

BOOL LinkedList_Contains(wLinkedList* list, void* value)
{
	wLinkedListNode* item;

	if (!list->head)
		return FALSE;

	item = list->head;

	while (item)
	{
		if (item->value == value)
			break;

		item = item->next;
	}

	return (item) ? TRUE : FALSE;
}

/**
 * Removes all entries from the LinkedList.
 */

void LinkedList_Clear(wLinkedList* list)
{
	wLinkedListNode* node;
	wLinkedListNode* nextNode;

	if (!list->head)
		return;

	node = list->head;

	while (node)
	{
		nextNode = node->next;
		free(node);
		node = nextNode;
	}

	list->head = list->tail = NULL;
	list->count = 0;
}

/**
 * Adds a new node containing the specified value at the start of the LinkedList.
 */

void LinkedList_AddFirst(wLinkedList* list, void* value)
{
	wLinkedListNode* node;

	node = (wLinkedListNode*) malloc(sizeof(wLinkedListNode));
	node->prev = node->next = NULL;
	node->value = value;

	if (!list->head)
	{
		list->tail = list->head = node;
	}
	else
	{
		list->head->prev = node;
		node->next = list->head;
		list->head = node;
	}

	list->count++;
}

/**
 * Adds a new node containing the specified value at the end of the LinkedList.
 */

void LinkedList_AddLast(wLinkedList* list, void* value)
{
	wLinkedListNode* node;

	node = (wLinkedListNode*) malloc(sizeof(wLinkedListNode));
	node->prev = node->next = NULL;
	node->value = value;

	if (!list->tail)
	{
		list->head = list->tail = node;
	}
	else
	{
		list->tail->next = node;
		node->prev = list->tail;
		list->tail = node;
	}

	list->count++;
}

/**
 * Removes the first occurrence of the specified value from the LinkedList.
 */

void LinkedList_Remove(wLinkedList* list, void* value)
{
	wLinkedListNode* node;

	node = list->head;

	while (node)
	{
		if (node->value == value)
		{
			if (node->prev)
				node->prev->next = node->next;

			if (node->next)
				node->next->prev = node->prev;

			if ((!node->prev) && (!node->next))
				list->head = list->tail = NULL;

			free(node);

			list->count--;

			break;
		}

		node = node->next;
	}
}

/**
 * Removes the node at the start of the LinkedList.
 */

void LinkedList_RemoveFirst(wLinkedList* list)
{
	wLinkedListNode* node;

	if (list->head)
	{
		node = list->head;

		list->head = list->head->next;

		if (!list->head)
		{
			list->tail = NULL;
		}
		else
		{
			list->head->prev = NULL;
		}

		free(node);

		list->count--;
	}
}

/**
 * Removes the node at the end of the LinkedList.
 */

void LinkedList_RemoveLast(wLinkedList* list)
{
	wLinkedListNode* node;

	if (list->tail)
	{
		node = list->tail;

		list->tail = list->tail->prev;

		if (!list->tail)
		{
			list->head = NULL;
		}
		else
		{
			list->tail->next = NULL;
		}

		free(node);

		list->count--;
	}
}

/**
 * Sets the enumerator to its initial position, which is before the first element in the collection.
 */

void LinkedList_Enumerator_Reset(wLinkedList* list)
{
	list->initial = 1;
	list->current = list->head;
}

/*
 * Gets the element at the current position of the enumerator.
 */

void* LinkedList_Enumerator_Current(wLinkedList* list)
{
	if (list->initial)
		return NULL;

	if (list->current)
		return list->current->value;
	else
		return NULL;
}

/*
 * Advances the enumerator to the next element of the LinkedList.
 */

BOOL LinkedList_Enumerator_MoveNext(wLinkedList* list)
{
	if (list->initial)
		list->initial = 0;
	else
		list->current = list->current->next;

	if (!list->current)
		return FALSE;

	return TRUE;
}

/**
 * Construction, Destruction
 */

wLinkedList* LinkedList_New()
{
	wLinkedList* list = NULL;

	list = (wLinkedList*) malloc(sizeof(wLinkedList));

	if (list)
	{
		list->count = 0;
		list->initial = 0;
		list->head = NULL;
		list->tail = NULL;
		list->current = NULL;
	}

	return list;
}

void LinkedList_Free(wLinkedList* list)
{
	if (list)
	{
		LinkedList_Clear(list);
		free(list);
	}
}

