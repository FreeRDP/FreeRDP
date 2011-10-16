/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/list.h>

static LIST_ITEM* list_item_new(void* data)
{
	LIST_ITEM* item;

	item = xnew(LIST_ITEM);
	item->data = data;
	return item;
}

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

LIST* list_new(void)
{
	LIST* list;

	list = xnew(LIST);
	list->count = 0;
	return list;
}

void list_free(LIST* list)
{
	while (list->head)
		list_dequeue(list);
	xfree(list);
}

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
		xfree(item);
		list->count--;
	}
	return data;
}

void* list_peek(LIST* list)
{
	LIST_ITEM* item;

	item = list->head;
	return item ? item->data : NULL;
}

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
		xfree(item);
		list->count--;
	}
	else
		data = NULL;
	return data;
}

int list_size(LIST* list)
{
	return list->count;
}
