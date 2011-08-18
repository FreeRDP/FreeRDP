/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 RAIL
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

#include <freerdp/utils/event.h>
#include <freerdp/rail/rail.h>
#include "xf_window.h"

#include "xf_rail.h"

void xf_rail_paint(xfInfo* xfi, rdpRail* rail)
{
	xfWindow* xfw;
	rdpWindow* window;
	window_list_rewind(rail->list);

	while (window_list_has_next(rail->list))
	{
		window = window_list_get_next(rail->list);
		xfw = (xfWindow*) window->extra;

		//printf("painting window 0x%08X\n", window->windowId);

		XCopyArea(xfi->display, xfi->window->handle, xfw->handle, xfw->gc,
				window->windowOffsetX, window->windowOffsetY,
				window->windowWidth, window->windowHeight,
				0, 0);

		XFlush(xfi->display);
	}
}

void xf_rail_CreateWindow(rdpRail* rail, rdpWindow* window)
{
	xfWindow* xfw;

	xfw = xf_CreateWindow((xfInfo*) rail->extra,
			window->windowOffsetX, window->windowOffsetY,
			window->windowWidth, window->windowHeight, "RAIL");

	window->extra = (void*) xfw;
	window->extraId = (void*) xfw->handle;
}

void xf_rail_register_callbacks(xfInfo* xfi, rdpRail* rail)
{
	rail->extra = (void*) xfi;
	rail->CreateWindow = xf_rail_CreateWindow;
}

void xf_process_rail_get_sysparams_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RDP_EVENT* new_event;
	RAIL_SYSPARAM_ORDER* sysparam;

	sysparam = (RAIL_SYSPARAM_ORDER*) event->user_data;

	sysparam->workArea.left = xfi->workArea.x;
	sysparam->workArea.top = xfi->workArea.y;
	sysparam->workArea.right = xfi->workArea.x + xfi->workArea.width;
	sysparam->workArea.bottom = xfi->workArea.y + xfi->workArea.height;

	sysparam->displayChange.left = xfi->workArea.x;
	sysparam->displayChange.top = xfi->workArea.y;
	sysparam->displayChange.right = xfi->workArea.x + xfi->workArea.width;
	sysparam->displayChange.bottom = xfi->workArea.y + xfi->workArea.height;

	sysparam->taskbarPos.left = 0;
	sysparam->taskbarPos.top = 0;
	sysparam->taskbarPos.right = 0;
	sysparam->taskbarPos.bottom = 0;

	new_event = freerdp_event_new(RDP_EVENT_CLASS_RAIL,
			RDP_EVENT_TYPE_RAIL_CLIENT_SET_SYSPARAMS, NULL, (void*) sysparam);

	freerdp_chanman_send_event(chanman, new_event);
}

void xf_process_rail_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_RAIL_CHANNEL_GET_SYSPARAMS:
			xf_process_rail_get_sysparams_event(xfi, chanman, event);
			break;

		default:
			break;
	}
}
