/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RAIL Window List
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/rail/window_list.h>

rdpWindow* window_list_get_by_id(rdpWindowList* list, uint32 windowId)
{
	rdpWindow* window;

	window = list->head;

	if (window == NULL)
		return NULL;

	while (window->next != NULL)
	{
		if (window->windowId == windowId)
			return window;

		window = window->next;
	}

	return NULL;
}

void window_list_create(rdpWindowList* list, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpWindow* window;

	window = rail_CreateWindow();

	if (list->head == NULL)
	{
		list->head = list->tail = window;
		window->prev = NULL;
		window->next = NULL;
	}
	else
	{
		window->prev = list->tail;
		window->next = NULL;
		list->tail = window;
	}

	window->windowId = orderInfo->windowId;
}

void window_list_update(rdpWindowList* list, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpWindow* window;

	window = window_list_get_by_id(list, orderInfo->windowId);

	if (window == NULL)
		return;
}

void window_list_delete(rdpWindowList* list, WINDOW_ORDER_INFO* orderInfo)
{
	rdpWindow* prev;
	rdpWindow* next;
	rdpWindow* window;

	window = window_list_get_by_id(list, orderInfo->windowId);

	prev = window->prev;
	next = window->next;

	if (prev != NULL)
		prev->next = next;

	if (next != NULL)
		next->prev = prev;

	if (list->head == list->tail)
	{
		list->head = list->tail = NULL;
	}
	else
	{
		if (list->head == window)
			list->head = next;

		if (list->tail == window)
			list->tail = prev;
	}

	rail_DestroyWindow(window);
}

rdpWindowList* window_list_new()
{
	rdpWindowList* list;

	list = (rdpWindowList*) xzalloc(sizeof(rdpWindowList));

	if (list != NULL)
	{
		list->head = NULL;
		list->tail = NULL;
	}

	return list;
}

void window_list_free(rdpWindowList* list)
{
	if (list != NULL)
	{
		xfree(list);
	}
}
