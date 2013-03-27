/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/stream.h>

#include "librail.h"

#include <freerdp/rail/window_list.h>

void window_list_rewind(rdpWindowList* list)
{
	list->iterator = list->head;
}

BOOL window_list_has_next(rdpWindowList* list)
{
	if (list->iterator != NULL)
	{
		if (list->iterator != NULL)
			return TRUE;
	}

	return FALSE;
}

rdpWindow* window_list_get_next(rdpWindowList* list)
{
	rdpWindow* next = NULL;

	if (list->iterator != NULL)
	{
		next = list->iterator;
		list->iterator = list->iterator->next;
	}

	return next;
}

rdpWindow* window_list_get_by_extra_id(rdpWindowList* list, void* extraId)
{
	rdpWindow* window;

	window = list->head;

	if (window == NULL)
		return NULL;

	while (window != NULL)
	{
		if (window->extraId == extraId)
			return window;

		window = window->next;
	}

	return NULL;
}

rdpWindow* window_list_get_by_id(rdpWindowList* list, UINT32 windowId)
{
	rdpWindow* window;

	window = list->head;

	if (window == NULL)
		return NULL;

	while (window != NULL)
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

	/* See if the window already exists */
	window = window_list_get_by_id(list, orderInfo->windowId);
	 
        /* If the window already exists, just update the existing window */
	if (window != NULL)
	{
	        window_list_update(list, orderInfo, window_state);
	        return;
	}

	window = (rdpWindow*) malloc(sizeof(rdpWindow));

	if (window == NULL)
		return;

	ZeroMemory(window, sizeof(rdpWindow));

	window->windowId = orderInfo->windowId;

	if (list->head == NULL)
	{
		list->head = list->tail = window;
		window->prev = NULL;
		window->next = NULL;
	}
	else
	{
		window->prev = list->tail;
		list->tail->next = window;
		window->next = NULL;
		list->tail = window;
	}

	window->windowId = orderInfo->windowId;

	window_state_update(window, orderInfo, window_state);

	rail_CreateWindow(list->rail, window);
}

void window_list_update(rdpWindowList* list, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpWindow* window;

	window = window_list_get_by_id(list, orderInfo->windowId);

	if (window == NULL)
		return;

	window_state_update(window, orderInfo, window_state);

	rail_UpdateWindow(list->rail, window);
}

void window_list_delete(rdpWindowList* list, WINDOW_ORDER_INFO* orderInfo)
{
	rdpWindow* prev;
	rdpWindow* next;
	rdpWindow* window;

	window = window_list_get_by_id(list, orderInfo->windowId);

	if (window == NULL)
		return;

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

	rail_DestroyWindow(list->rail, window);
}

void window_list_clear(rdpWindowList* list)
{
       rdpWindow* current = list->head;

       while (current != NULL)
       {
               list->head = current->next;
               rail_DestroyWindow(list->rail, current);
               current = list->head;
       }       
       
       list->tail = NULL;
}

rdpWindowList* window_list_new(rdpRail* rail)
{
	rdpWindowList* list;

	list = (rdpWindowList*) malloc(sizeof(rdpWindowList));

	if (list != NULL)
	{
		ZeroMemory(list, sizeof(rdpWindowList));

		list->head = NULL;
		list->tail = NULL;
		list->rail = rail;
	}

	return list;
}

void window_list_free(rdpWindowList* list)
{
	if (list != NULL)
	{
		free(list);
	}
}
