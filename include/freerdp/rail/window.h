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

#ifndef __RAIL_WINDOW_H
#define __RAIL_WINDOW_H

#include <freerdp/api.h>
#include <freerdp/rail.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/utils/stream.h>

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
	uint32 fieldFlags;
	rdpWindow* prev;
	rdpWindow* next;
	uint32 windowId;
	uint32 ownerWindowId;
	rdpWindow* ownerWindow;
	uint32 style;
	uint32 extendedStyle;
	uint8 showState;
	UNICODE_STRING titleInfo;
	uint32 clientOffsetX;
	uint32 clientOffsetY;
	uint32 clientAreaWidth;
	uint32 clientAreaHeight;
	uint8 RPContent;
	uint32 rootParentHandle;
	sint32 windowOffsetX;
	sint32 windowOffsetY;
	uint32 windowClientDeltaX;
	uint32 windowClientDeltaY;
	uint32 windowWidth;
	uint32 windowHeight;
	uint16 numWindowRects;
	RECTANGLE_16* windowRects;
	uint32 visibleOffsetX;
	uint32 visibleOffsetY;
	uint16 numVisibilityRects;
	RECTANGLE_16* visibilityRects;
};

FREERDP_API void window_state_update(rdpWindow* window, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);

FREERDP_API void rail_CreateWindow(rdpRail* rail, rdpWindow* window);
FREERDP_API void rail_UpdateWindow(rdpRail* rail, rdpWindow* window);
FREERDP_API void rail_DestroyWindow(rdpRail* rail, rdpWindow* window);

#endif /* __RAIL_WINDOW_H */
