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

		XPutImage(xfi->display, xfi->primary, xfw->gc, xfi->image,
				window->windowOffsetX, window->windowOffsetY,
				window->windowOffsetX, window->windowOffsetY,
				window->windowWidth, window->windowHeight);

		XCopyArea(xfi->display, xfi->primary, xfw->handle, xfw->gc,
				window->windowOffsetX, window->windowOffsetY,
				window->windowWidth, window->windowHeight, 0, 0);

		XFlush(xfi->display);
	}
}

void xf_rail_CreateWindow(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;

	xfw = xf_CreateWindow((xfInfo*) rail->extra,
			window->windowOffsetX + xfi->workArea.x,
			window->windowOffsetY + xfi->workArea.y,
			window->windowWidth, window->windowHeight, window->title);

	window->extra = (void*) xfw;
	window->extraId = (void*) xfw->handle;
}

void xf_rail_MoveWindow(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_MoveWindow((xfInfo*) rail->extra, xfw,
			window->windowOffsetX + xfi->workArea.x,
			window->windowOffsetY + xfi->workArea.y,
			window->windowWidth, window->windowHeight);

	XFlush(xfi->display);
}

void xf_rail_DestroyWindow(rdpRail* rail, rdpWindow* window)
{
	xfWindow* xfw;
	xfw = (xfWindow*) window->extra;
	xf_DestroyWindow((xfInfo*) rail->extra, xfw);
}

void xf_rail_register_callbacks(xfInfo* xfi, rdpRail* rail)
{
	rail->extra = (void*) xfi;
	rail->CreateWindow = xf_rail_CreateWindow;
	rail->MoveWindow = xf_rail_MoveWindow;
	rail->DestroyWindow = xf_rail_DestroyWindow;
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

void xf_process_rail_exec_result_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	RAIL_EXEC_RESULT_ORDER * exec_result;
	const char* error_code_names[] =
	{
	"RAIL_EXEC_S_OK",
	"RAIL_EXEC_E_HOOK_NOT_LOADED",
	"RAIL_EXEC_E_DECODE_FAILED",
	"RAIL_EXEC_E_NOT_IN_ALLOWLIST",
	"RAIL_EXEC_E_FILE_NOT_FOUND",
	"RAIL_EXEC_E_FAIL",
	"RAIL_EXEC_E_SESSION_LOCKED"
	};

	exec_result = (RAIL_EXEC_RESULT_ORDER *)event->user_data;

	if (exec_result->execResult != RAIL_EXEC_S_OK)
	{
		printf("RAIL exec error: execResult=%s NtError=0x%X\n",
			error_code_names[exec_result->execResult], exec_result->rawResult);
	}
}

void xf_process_rail_event(xfInfo* xfi, rdpChanMan* chanman, RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_RAIL_CHANNEL_GET_SYSPARAMS:
			xf_process_rail_get_sysparams_event(xfi, chanman, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_EXEC_RESULTS:
			xf_process_rail_exec_result_event(xfi, chanman, event);
			break;

		default:
			break;
	}
}
