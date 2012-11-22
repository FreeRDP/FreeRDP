/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Double-linked List Utils
 *
 * Copyright 2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/utils/list.h>

/**
 * Allocates a new LIST_ITEM element. This will be used to store the data provided by the caller,
 * and will be used as a new element in a list.
 *
 * @param data - pointer to the data that must be stored by the new item.
 *
 * @return An allocated LIST_ITEM element that contains the 'data' pointer.
 */
static LIST_ITEM* list_item_new(void* data)
{
	LIST_ITEM* item;

	item = (LIST_ITEM*) malloc(sizeof(LIST_ITEM));
	ZeroMemory(item, sizeof(LIST_ITEM));

	if (item)
		item->data = data;

	return item;
}

/**
 * Searches an element in the list.
 * The element is verified by its pointer value - there is no way to verify the buffer's content.
 *
 * @param list - pointer to a valid LIST structure
 * @param data - pointer to the data that must be found.
 *
 * @return the LIST_ITEM element that contains the 'data' pointer. NULL if 'data' was not found.
 */
static LIST_ITEM* list_item_find(LIST* list, void* data)
{
	LIST_ITEM* item;

	for (item = list->head; item; item = item->next)
	{
		if (item->data == data)
			return item;
	}
	return NULL;
}

/**
 * Allocates a new LIST structure.
 * The list_*() API implements a chainlist and allows to store data of arbitrary type in FIFO mode.
 * @see list_enqueue() to add elements to the list.
 * @see list_dequeue() to remove the first element of the list and get a pointer to it.
 * @see list_peek() to retrieve the first element of the list (and keep it there).
 * @see list_free() to deallocate the list.
 * @see list_size() to get the current number of elements in the list.
 *
 * @return A pointer to the allocated LIST structure. It must be deallocated by a call to list_free().
 *
 */
LIST* list_new(void)
{
	LIST* list;

	list = (LIST*) malloc(sizeof(LIST));
	ZeroMemory(list, sizeof(LIST));

	list->count = 0;

	return list;
}

/**
 * Deallocates a LIST structure.
 * All elements of the list will be removed (but not deallocated). Only the list-specific resources are freed.
 *
 * @param list - pointer to the LIST that must be deallocated.
 */
void list_free(LIST* list)
{
	while (list->head)
		list_dequeue(list);

	free(list);
}

/**
 * Add an element at the end of an existing list.
 *
 * @param list - pointer to the LIST that will contain the new element
 * @param data - pointer to the buffer that will be added to the list
 */
void list_enqueue(LIST* list, void* data)
{
	LIST_ITEM* item;

	item = list_item_new(data);

	if (list->tail == NULL)
	{
		list->head = item;
		list->tail = item;
	}
	else
	{
		item->prev = list->tail;
		list->tail->next = item;
		list->tail = item;
	}

	list->count++;
}

/**
 * Removes the first element of a list, and returns a pointer to it.
 * The list-specific resources associated to this element are freed in the process.
 *
 * @param list - pointer to a valid LIST structure
 *
 * @return a pointer to the data of the first element of this list. NULL if the list is empty.
 */
void* list_dequeue(LIST* list)
{
	LIST_ITEM* item;
	void* data = NULL;

	item = list->head;

	if (item != NULL)
	{
		list->head = item->next;

		if (list->head == NULL)
			list->tail = NULL;
		else
			list->head->prev = NULL;

		data = item->data;
		free(item);
		list->count--;
	}

	return data;
}

/**
 * Returns a pointer to the data from the first element of the list.
 * The element itself is not removed from the list by this call.
 *
 * @param list - pointerto a valid LIST structure
 *
 * @return a pointer to the data of the first element of this list. NULL if the list is empty.
 */
void* list_peek(LIST* list)
{
	LIST_ITEM* item;

	item = list->head;
	return item ? item->data : NULL;
}

/**
 * Searches for the data provided in parameter, and returns a pointer to the element next to it.
 * If the item is not found, or if it is the last in the list, the function will return NULL.
 *
 * @param list - pointer to the list that must be searched.
 * @param data - pointer to the buffer that must be found. The comparison is done on the pointer value - not the buffer's content.
 *
 * @return a pointer to the data of the element that resides after the 'data' parameter in the list.
 * NULL if 'data' was not found in the list, or if it is the last element.
 *
 * @see list_item_find() for more information on list item searches.
 */
void* list_next(LIST* list, void* data)
{
	LIST_ITEM* item;

	item = list_item_find(list, data);
	data = NULL;

	if (item != NULL)
	{
		if (item->next != NULL)
			data = item->next->data;
	}

	return data;
}

/**
 * Searches for the data provided in parameter, and removes it from the list if it is found.
 *
 * @param list - pointer to the list that must be searched.
 * @param data - pointer to the buffer that must be found. The comparison is done on the pointer value - not the buffer's content.
 *
 * @return the 'data' pointer, if the element was found, and successfully removed from the list.
 * NULL if the element was not found.
 */
void* list_remove(LIST* list, void* data)
{
	LIST_ITEM* item;

	item = list_item_find(list, data);

	if (item != NULL)
	{
		if (item->prev != NULL)
			item->prev->next = item->next;
		if (item->next != NULL)
			item->next->prev = item->prev;
		if (list->head == item)
			list->head = item->next;
		if (list->tail == item)
			list->tail = item->prev;
		free(item);
		list->count--;
	}
	else
	{
		data = NULL;
	}

	return data;
}

/**
 * Returns the current size of the list (number of elements).
 * This number is kept up to date by the list_enqueue and list_dequeue functions.
 *
 * @param list - pointer to a valide LIST structure.
 *
 * @return the number of elements in that list.
 */
int list_size(LIST* list)
{
	return list->count;
}
