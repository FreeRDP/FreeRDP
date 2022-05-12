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

#include <winpr/config.h>

#include <winpr/collections.h>
#include <winpr/assert.h>

typedef struct s_wLinkedListItem wLinkedListNode;

struct s_wLinkedListItem
{
	void* value;
	wLinkedListNode* prev;
	wLinkedListNode* next;
};

struct s_wLinkedList
{
	int count;
	int initial;
	wLinkedListNode* head;
	wLinkedListNode* tail;
	wLinkedListNode* current;
	wObject object;
};

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
	WINPR_ASSERT(list);
	return list->count;
}

/**
 * Gets the first node of the LinkedList.
 */

void* LinkedList_First(wLinkedList* list)
{
	WINPR_ASSERT(list);
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
	WINPR_ASSERT(list);
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

BOOL LinkedList_Contains(wLinkedList* list, const void* value)
{
	wLinkedListNode* item;
	OBJECT_EQUALS_FN keyEquals;

	WINPR_ASSERT(list);
	if (!list->head)
		return FALSE;

	item = list->head;
	keyEquals = list->object.fnObjectEquals;

	while (item)
	{
		if (keyEquals(item->value, value))
			break;

		item = item->next;
	}

	return (item) ? TRUE : FALSE;
}

static wLinkedListNode* LinkedList_FreeNode(wLinkedList* list, wLinkedListNode* node)
{
	wLinkedListNode* next;
	wLinkedListNode* prev;

	WINPR_ASSERT(list);
	WINPR_ASSERT(node);

	next = node->next;
	prev = node->prev;
	if (prev)
		prev->next = next;

	if (next)
		next->prev = prev;

	if (node == list->head)
		list->head = node->next;

	if (node == list->tail)
		list->tail = node->prev;

	if (list->object.fnObjectUninit)
		list->object.fnObjectUninit(node);

	if (list->object.fnObjectFree)
		list->object.fnObjectFree(node);

	free(node);
	list->count--;
	return next;
}

/**
 * Removes all entries from the LinkedList.
 */

void LinkedList_Clear(wLinkedList* list)
{
	wLinkedListNode* node;
	WINPR_ASSERT(list);
	if (!list->head)
		return;

	node = list->head;

	while (node)
		node = LinkedList_FreeNode(list, node);

	list->head = list->tail = NULL;
	list->count = 0;
}

static wLinkedListNode* LinkedList_Create(wLinkedList* list, const void* value)
{
	wLinkedListNode* node;

	WINPR_ASSERT(list);
	node = (wLinkedListNode*)calloc(1, sizeof(wLinkedListNode));

	if (!node)
		return NULL;

	if (list->object.fnObjectNew)
		node->value = list->object.fnObjectNew(value);
	else
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		cnv.cpv = value;
		node->value = cnv.pv;
	}

	if (list->object.fnObjectInit)
		list->object.fnObjectInit(node);

	return node;
}
/**
 * Adds a new node containing the specified value at the start of the LinkedList.
 */

BOOL LinkedList_AddFirst(wLinkedList* list, const void* value)
{
	wLinkedListNode* node = LinkedList_Create(list, value);

	if (!node)
		return FALSE;

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
	return TRUE;
}

/**
 * Adds a new node containing the specified value at the end of the LinkedList.
 */

BOOL LinkedList_AddLast(wLinkedList* list, const void* value)
{
	wLinkedListNode* node = LinkedList_Create(list, value);

	if (!node)
		return FALSE;

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
	return TRUE;
}

/**
 * Removes the first occurrence of the specified value from the LinkedList.
 */

BOOL LinkedList_Remove(wLinkedList* list, const void* value)
{
	wLinkedListNode* node;
	OBJECT_EQUALS_FN keyEquals;
	WINPR_ASSERT(list);

	keyEquals = list->object.fnObjectEquals;
	node = list->head;

	while (node)
	{
		if (keyEquals(node->value, value))
		{
			LinkedList_FreeNode(list, node);
			return TRUE;
		}

		node = node->next;
	}

	return FALSE;
}

/**
 * Removes the node at the start of the LinkedList.
 */

void LinkedList_RemoveFirst(wLinkedList* list)
{
	WINPR_ASSERT(list);
	if (list->head)
		LinkedList_FreeNode(list, list->head);
}

/**
 * Removes the node at the end of the LinkedList.
 */

void LinkedList_RemoveLast(wLinkedList* list)
{
	WINPR_ASSERT(list);
	if (list->tail)
		LinkedList_FreeNode(list, list->tail);
}

/**
 * Sets the enumerator to its initial position, which is before the first element in the collection.
 */

void LinkedList_Enumerator_Reset(wLinkedList* list)
{
	WINPR_ASSERT(list);
	list->initial = 1;
	list->current = list->head;
}

/*
 * Gets the element at the current position of the enumerator.
 */

void* LinkedList_Enumerator_Current(wLinkedList* list)
{
	WINPR_ASSERT(list);
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
	WINPR_ASSERT(list);
	if (list->initial)
		list->initial = 0;
	else if (list->current)
		list->current = list->current->next;

	if (!list->current)
		return FALSE;

	return TRUE;
}

static BOOL default_equal_function(const void* objA, const void* objB)
{
	return objA == objB;
}

/**
 * Construction, Destruction
 */

wLinkedList* LinkedList_New(void)
{
	wLinkedList* list = NULL;
	list = (wLinkedList*)calloc(1, sizeof(wLinkedList));

	if (list)
	{
		list->object.fnObjectEquals = default_equal_function;
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

wObject* LinkedList_Object(wLinkedList* list)
{
	WINPR_ASSERT(list);

	return &list->object;
}
