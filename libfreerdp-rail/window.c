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
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/unicode.h>

#include "librail.h"

#include <freerdp/rail/window.h>

struct _WINDOW_STYLE
{
	uint32 style;
	const char* name;
	boolean multi;
};
typedef struct _WINDOW_STYLE WINDOW_STYLE;

static const WINDOW_STYLE WINDOW_STYLES[] =
{
	{ WS_BORDER, "WS_BORDER", false },
	{ WS_CAPTION, "WS_CAPTION", false },
	{ WS_CHILD, "WS_CHILD", false },
	{ WS_CLIPCHILDREN, "WS_CLIPCHILDREN", false },
	{ WS_CLIPSIBLINGS, "WS_CLIPSIBLINGS", false },
	{ WS_DISABLED, "WS_DISABLED", false },
	{ WS_DLGFRAME, "WS_DLGFRAME", false },
	{ WS_GROUP, "WS_GROUP", false },
	{ WS_HSCROLL, "WS_HSCROLL", false },
	{ WS_ICONIC, "WS_ICONIC", false },
	{ WS_MAXIMIZE, "WS_MAXIMIZE", false },
	{ WS_MAXIMIZEBOX, "WS_MAXIMIZEBOX", false },
	{ WS_MINIMIZE, "WS_MINIMIZE", false },
	{ WS_MINIMIZEBOX, "WS_MINIMIZEBOX", false },
	{ WS_OVERLAPPED, "WS_OVERLAPPED", false },
	{ WS_OVERLAPPEDWINDOW, "WS_OVERLAPPEDWINDOW", true },
	{ WS_POPUP, "WS_POPUP", false },
	{ WS_POPUPWINDOW, "WS_POPUPWINDOW", true },
	{ WS_SIZEBOX, "WS_SIZEBOX", false },
	{ WS_SYSMENU, "WS_SYSMENU", false },
	{ WS_TABSTOP, "WS_TABSTOP", false },
	{ WS_THICKFRAME, "WS_THICKFRAME", false },
	{ WS_VISIBLE, "WS_VISIBLE", false }
};

static const WINDOW_STYLE EXTENDED_WINDOW_STYLES[] =
{
	{ WS_EX_ACCEPTFILES, "WS_EX_ACCEPTFILES", false },
	{ WS_EX_APPWINDOW, "WS_EX_APPWINDOW", false },
	{ WS_EX_CLIENTEDGE, "WS_EX_CLIENTEDGE", false },
	{ WS_EX_COMPOSITED, "WS_EX_COMPOSITED", false },
	{ WS_EX_CONTEXTHELP, "WS_EX_CONTEXTHELP", false },
	{ WS_EX_CONTROLPARENT, "WS_EX_CONTROLPARENT", false },
	{ WS_EX_DLGMODALFRAME, "WS_EX_DLGMODALFRAME", false },
	{ WS_EX_LAYERED, "WS_EX_LAYERED", false },
	{ WS_EX_LAYOUTRTL, "WS_EX_LAYOUTRTL", false },
	{ WS_EX_LEFT, "WS_EX_LEFT", false },
	{ WS_EX_LEFTSCROLLBAR, "WS_EX_LEFTSCROLLBAR", false },
	{ WS_EX_LTRREADING, "WS_EX_LTRREADING", false },
	{ WS_EX_MDICHILD, "WS_EX_MDICHILD", false },
	{ WS_EX_NOACTIVATE, "WS_EX_NOACTIVATE", false },
	{ WS_EX_NOINHERITLAYOUT, "WS_EX_NOINHERITLAYOUT", false },
	{ WS_EX_NOPARENTNOTIFY, "WS_EX_NOPARENTNOTIFY", false },
	{ WS_EX_OVERLAPPEDWINDOW, "WS_EX_OVERLAPPEDWINDOW", true },
	{ WS_EX_PALETTEWINDOW, "WS_EX_PALETTEWINDOW", true },
	{ WS_EX_RIGHT, "WS_EX_RIGHT", false },
	{ WS_EX_RIGHTSCROLLBAR, "WS_EX_RIGHTSCROLLBAR", false },
	{ WS_EX_RTLREADING, "WS_EX_RTLREADING", false },
	{ WS_EX_STATICEDGE, "WS_EX_STATICEDGE", false },
	{ WS_EX_TOOLWINDOW, "WS_EX_TOOLWINDOW", false },
	{ WS_EX_TOPMOST, "WS_EX_TOPMOST", false },
	{ WS_EX_TRANSPARENT, "WS_EX_TRANSPARENT", false },
	{ WS_EX_WINDOWEDGE, "WS_EX_WINDOWEDGE", false }
};

void print_window_styles(uint32 style)
{
	int i;

	printf("Window Styles:\n{\n");
	for (i = 0; i < sizeof(WINDOW_STYLES) / sizeof(WINDOW_STYLE); i++)
	{
		if (style & WINDOW_STYLES[i].style)
		{
			if (WINDOW_STYLES[i].multi)
			{
				if ((style & WINDOW_STYLES[i].style) != WINDOW_STYLES[i].style)
						continue;
			}

			printf("\t%s\n", WINDOW_STYLES[i].name);
		}
	}
	printf("}\n");
}

void print_extended_window_styles(uint32 style)
{
	int i;

	printf("Extended Window Styles:\n{\n");
	for (i = 0; i < sizeof(EXTENDED_WINDOW_STYLES) / sizeof(WINDOW_STYLE); i++)
	{
		if (style & EXTENDED_WINDOW_STYLES[i].style)
		{
			if (EXTENDED_WINDOW_STYLES[i].multi)
			{
				if ((style & EXTENDED_WINDOW_STYLES[i].style) != EXTENDED_WINDOW_STYLES[i].style)
						continue;
			}

			printf("\t%s\n", EXTENDED_WINDOW_STYLES[i].name);
		}
	}
	printf("}\n");
}

void window_state_update(rdpWindow* window, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	window->fieldFlags = orderInfo->fieldFlags;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		window->ownerWindowId = window_state->ownerWindowId;
		DEBUG_RAIL("ownerWindowId:0x%08X", window->ownerWindowId);
	}

	DEBUG_RAIL("windowId=0x%X ownerWindowId=0x%X",
			window->windowId, window->ownerWindowId);

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		window->style = window_state->style;
		window->extendedStyle = window_state->extendedStyle;

#ifdef WITH_DEBUG_RAIL
		print_window_styles(window->style);
		print_extended_window_styles(window->extendedStyle);
#endif
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		window->showState = window_state->showState;
		DEBUG_RAIL("ShowState:%d", window->showState);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		window->titleInfo.length = window_state->titleInfo.length;
		window->titleInfo.string = xmalloc(window_state->titleInfo.length);
		memcpy(window->titleInfo.string, window_state->titleInfo.string, window->titleInfo.length);

#ifdef WITH_DEBUG_RAIL
		freerdp_hexdump(window->titleInfo.string, window->titleInfo.length);
#endif
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{
		window->clientOffsetX = window_state->clientOffsetX;
		window->clientOffsetY = window_state->clientOffsetY;

		DEBUG_RAIL("Client Area Offset: (%d, %d)",
				window->clientOffsetX, window->clientOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		window->clientAreaWidth = window_state->clientAreaWidth;
		window->clientAreaHeight = window_state->clientAreaHeight;

		DEBUG_RAIL("Client Area Size: (%d, %d)",
				window->clientAreaWidth, window->clientAreaHeight);
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

		DEBUG_RAIL("Window Offset: (%d, %d)",
				window->windowOffsetX, window->windowOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		window->windowClientDeltaX = window_state->windowClientDeltaX;
		window->windowClientDeltaY = window_state->windowClientDeltaY;

		DEBUG_RAIL("Window Client Delta: (%d, %d)",
				window->windowClientDeltaX, window->windowClientDeltaY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		window->windowWidth = window_state->windowWidth;
		window->windowHeight = window_state->windowHeight;

		DEBUG_RAIL("Window Size: (%d, %d)",
				window->windowWidth, window->windowHeight);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		int i;

		if (window->windowRects != NULL)
			xfree(window->windowRects);

		window->windowRects = window_state->windowRects;
		window->numWindowRects = window_state->numWindowRects;

		for (i = 0; i < (int) window_state->numWindowRects; i++)
		{
			DEBUG_RAIL("Window Rect #%d: left:%d top:%d right:%d bottom:%d", i,
					window_state->windowRects[i].left, window_state->windowRects[i].top,
					window_state->windowRects[i].right, window_state->windowRects[i].bottom);
		}
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		window->visibleOffsetX = window_state->visibleOffsetX;
		window->visibleOffsetY = window_state->visibleOffsetY;

		DEBUG_RAIL("Window Visible Offset: (%d, %d)",
				window->visibleOffsetX, window->visibleOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		int i;

		if (window->visibilityRects != NULL)
			xfree(window->visibilityRects);

		window->visibilityRects = window_state->visibilityRects;
		window->numVisibilityRects = window_state->numVisibilityRects;

		for (i = 0; i < (int) window_state->numVisibilityRects; i++)
		{
			DEBUG_RAIL("Visibility Rect #%d: left:%d top:%d right:%d bottom:%d", i,
					window_state->visibilityRects[i].left, window_state->visibilityRects[i].top,
					window_state->visibilityRects[i].right, window_state->visibilityRects[i].bottom);
		}
	}
}

void rail_CreateWindow(rdpRail* rail, rdpWindow* window)
{
	if (window->titleInfo.length > 0)
	{
		window->title = freerdp_uniconv_in(rail->uniconv, window->titleInfo.string, window->titleInfo.length);
	}
	else
	{
		window->title = (char*) xmalloc(sizeof("RAIL"));
		memcpy(window->title, "RAIL", sizeof("RAIL"));
	}

	IFCALL(rail->rail_CreateWindow, rail, window);

	if (window->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		IFCALL(rail->rail_SetWindowRects, rail, window);
	}
	if (window->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		IFCALL(rail->rail_SetWindowVisibilityRects, rail, window);
	}
}

void rail_UpdateWindow(rdpRail* rail, rdpWindow* window)
{
	if (window->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{

	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{

	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		IFCALL(rail->rail_ShowWindow, rail, window, window->showState);
	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		if (window->title != NULL)
			xfree(window->title);

		window->title = freerdp_uniconv_in(rail->uniconv, window->titleInfo.string, window->titleInfo.length);

		IFCALL(rail->rail_SetWindowText, rail, window);
	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{

	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{

	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{

	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{

	}

	if ((window->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET) ||
			(window->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE))
	{
		IFCALL(rail->rail_MoveWindow, rail, window);
	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{

	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		IFCALL(rail->rail_SetWindowRects, rail, window);
	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{

	}

	if (window->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		IFCALL(rail->rail_SetWindowVisibilityRects, rail, window);
	}
}

void rail_DestroyWindow(rdpRail* rail, rdpWindow* window)
{
	IFCALL(rail->rail_DestroyWindow, rail, window);

	if (window != NULL)
	{
		xfree(window);
	}
}
