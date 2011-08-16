/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RAIL Windows
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

#include <freerdp/rail/window.h>

void window_state_update(rdpWindow* window, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		window->ownerWindowId = window_state->ownerWindowId;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		window->style = window_state->style;
		window->extendedStyle = window_state->extendedStyle;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		window->showState = window_state->showState;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		window->clientOffsetX = window_state->clientOffsetX;
		window->clientOffsetY = window_state->clientOffsetY;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{
		window->clientOffsetX = window_state->clientOffsetX;
		window->clientOffsetY = window_state->clientOffsetY;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		window->clientAreaWidth = window_state->clientAreaWidth;
		window->clientAreaHeight = window_state->clientAreaHeight;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{
		window->RPContent = window_state->RPContent;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{
		window->rootParentHandle = window_state->rootParentHandle;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		window->windowOffsetX = window_state->windowOffsetX;
		window->windowOffsetY = window_state->windowOffsetY;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		window->windowClientDeltaX = window_state->windowClientDeltaX;
		window->windowClientDeltaY = window_state->windowClientDeltaY;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		window->windowWidth = window_state->windowWidth;
		window->windowHeight = window_state->windowHeight;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		window->visibleOffsetX = window_state->visibleOffsetX;
		window->visibleOffsetY = window_state->visibleOffsetY;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{

	}
}

rdpWindow* rail_CreateWindow(uint32 windowId)
{
	rdpWindow* window;

	window = (rdpWindow*) xzalloc(sizeof(rdpWindow));

	if (window != NULL)
	{
		window->windowId = windowId;
	}

	return window;
}

void rail_DestroyWindow(rdpWindow* window)
{
	if (window != NULL)
	{
		xfree(window);
	}
}
