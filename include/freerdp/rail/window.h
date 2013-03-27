/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_RAIL_WINDOW_H
#define FREERDP_RAIL_WINDOW_H

#include <freerdp/api.h>
#include <freerdp/rail.h>
#include <freerdp/types.h>
#include <freerdp/update.h>

#include <winpr/stream.h>

typedef struct rdp_window rdpWindow;

#include <freerdp/rail/rail.h>
#include <freerdp/rail/icon.h>

struct rdp_window
{
	void* extra;
	void* extraId;
	char* title;
	rdpIcon* bigIcon;
	rdpIcon* smallIcon;
	UINT32 fieldFlags;
	rdpWindow* prev;
	rdpWindow* next;
	UINT32 windowId;
	UINT32 ownerWindowId;
	UINT32 style;
	UINT32 extendedStyle;
	BYTE showState;
	RAIL_UNICODE_STRING titleInfo;
	UINT32 clientOffsetX;
	UINT32 clientOffsetY;
	UINT32 clientAreaWidth;
	UINT32 clientAreaHeight;
	BYTE RPContent;
	UINT32 rootParentHandle;
	INT32 windowOffsetX;
	INT32 windowOffsetY;
	UINT32 windowClientDeltaX;
	UINT32 windowClientDeltaY;
	UINT32 windowWidth;
	UINT32 windowHeight;
	UINT16 numWindowRects;
	RECTANGLE_16* windowRects;
	UINT32 visibleOffsetX;
	UINT32 visibleOffsetY;
	UINT16 numVisibilityRects;
	RECTANGLE_16* visibilityRects;
};

FREERDP_API void window_state_update(rdpWindow* window, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);

FREERDP_API void rail_CreateWindow(rdpRail* rail, rdpWindow* window);
FREERDP_API void rail_UpdateWindow(rdpRail* rail, rdpWindow* window);
FREERDP_API void rail_DestroyWindow(rdpRail* rail, rdpWindow* window);

#endif /* FREERDP_RAIL_WINDOW_H */
