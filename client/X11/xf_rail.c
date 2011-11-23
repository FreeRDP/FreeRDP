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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <freerdp/utils/event.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/rail.h>
#include <freerdp/rail/rail.h>

#include "xf_window.h"
#include "xf_rail.h"

void xf_rail_paint(xfInfo* xfi, rdpRail* rail, uint32 uleft, uint32 utop, uint32 uright, uint32 ubottom)
{
	xfWindow* xfw;
	rdpWindow* window;
	boolean intersect;
	uint32 iwidth, iheight;
	uint32 ileft, itop, iright, ibottom;
	uint32 wleft, wtop, wright, wbottom;

	window_list_rewind(rail->list);

	while (window_list_has_next(rail->list))
	{
		window = window_list_get_next(rail->list);
		xfw = (xfWindow*) window->extra;

		wleft = window->windowOffsetX;
		wtop = window->windowOffsetY;
		wright = window->windowOffsetX + window->windowWidth - 1;
		wbottom = window->windowOffsetY + window->windowHeight - 1;

		ileft = MAX(uleft, wleft);
		itop = MAX(utop, wtop);
		iright = MIN(uright, wright);
		ibottom = MIN(ubottom, wbottom);

		iwidth = iright - ileft + 1;
		iheight = ibottom - itop + 1;

		intersect = ((iright > ileft) && (ibottom > itop)) ? true : false;

		if (intersect)
		{
			xf_UpdateWindowArea(xfi, xfw, ileft - wleft, itop - wtop, iwidth, iheight);
		}
	}
}

void xf_rail_CreateWindow(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;

	xfw = xf_CreateWindow((xfInfo*) rail->extra, window,
			window->windowOffsetX, window->windowOffsetY,
			window->windowWidth, window->windowHeight,
			window->windowId);

	xf_SetWindowStyle(xfi, xfw, window->style, window->extendedStyle);

	XStoreName(xfi->display, xfw->handle, window->title);

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
			window->windowOffsetX, window->windowOffsetY,
			window->windowWidth, window->windowHeight);
}

void xf_rail_ShowWindow(rdpRail* rail, rdpWindow* window, uint8 state)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_ShowWindow((xfInfo*) rail->extra, xfw, state);
}

void xf_rail_SetWindowText(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;
	xfw = (xfWindow*) window->extra;

	XStoreName(xfi->display, xfw->handle, window->title);
}

void xf_rail_SetWindowIcon(rdpRail* rail, rdpWindow* window, rdpIcon* icon)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;
	xfw = (xfWindow*) window->extra;

	icon->extra = freerdp_icon_convert(icon->entry->bitsColor, NULL, icon->entry->bitsMask,
			icon->entry->width, icon->entry->height, icon->entry->bpp, rail->clrconv);

	xf_SetWindowIcon(xfi, xfw, icon);
}

void xf_rail_SetWindowRects(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_SetWindowRects(xfi, xfw, window->windowRects, window->numWindowRects);
}

void xf_rail_SetWindowVisibilityRects(rdpRail* rail, rdpWindow* window)
{
	xfInfo* xfi;
	xfWindow* xfw;

	xfi = (xfInfo*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_SetWindowVisibilityRects(xfi, xfw, window->windowRects, window->numWindowRects);
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
	rail->rail_CreateWindow = xf_rail_CreateWindow;
	rail->rail_MoveWindow = xf_rail_MoveWindow;
	rail->rail_ShowWindow = xf_rail_ShowWindow;
	rail->rail_SetWindowText = xf_rail_SetWindowText;
	rail->rail_SetWindowIcon = xf_rail_SetWindowIcon;
	rail->rail_SetWindowRects = xf_rail_SetWindowRects;
	rail->rail_SetWindowVisibilityRects = xf_rail_SetWindowVisibilityRects;
	rail->rail_DestroyWindow = xf_rail_DestroyWindow;
}

static void xf_on_free_rail_client_event(RDP_EVENT* event)
{
	if (event->event_class == RDP_EVENT_CLASS_RAIL)
	{
		rail_free_cloned_order(event->event_type, event->user_data);
	}
}

static void xf_send_rail_client_event(rdpChannels* channels, uint16 event_type, void* param)
{
	RDP_EVENT* out_event = NULL;
	void * payload = NULL;

	payload = rail_clone_order(event_type, param);
	if (payload != NULL)
	{
		out_event = freerdp_event_new(RDP_EVENT_CLASS_RAIL, event_type,
			xf_on_free_rail_client_event, payload);
		freerdp_channels_send_event(channels, out_event);
	}
}

void xf_rail_send_windowmove(xfInfo* xfi, uint32 windowId, uint32 left, uint32 top, uint32 right, uint32 bottom)
{
	rdpChannels* channels;
	RAIL_WINDOW_MOVE_ORDER window_move;

	channels = xfi->_context->channels;

	window_move.windowId = windowId;
	window_move.left = left;
	window_move.top = top;
	window_move.right = right;
	window_move.bottom = bottom;

	xf_send_rail_client_event(channels, RDP_EVENT_TYPE_RAIL_CLIENT_WINDOW_MOVE, &window_move);
}

void xf_rail_send_activate(xfInfo* xfi, Window xwindow, boolean enabled)
{
	rdpRail* rail;
	rdpChannels* channels;
	rdpWindow* rail_window;
	RAIL_ACTIVATE_ORDER activate;

	rail = xfi->_context->rail;
	channels = xfi->_context->channels;

	rail_window = window_list_get_by_extra_id(rail->list, (void*) xwindow);

	if (rail_window == NULL)
		return;

	activate.windowId = rail_window->windowId;
	activate.enabled = enabled;

	xf_send_rail_client_event(channels, RDP_EVENT_TYPE_RAIL_CLIENT_ACTIVATE, &activate);
}

void xf_rail_send_client_system_command(xfInfo* xfi, uint32 windowId, uint16 command)
{
	rdpChannels* channels;
	RAIL_SYSCOMMAND_ORDER syscommand;

	channels = xfi->_context->channels;

	syscommand.windowId = windowId;
	syscommand.command = command;

	xf_send_rail_client_event(channels, RDP_EVENT_TYPE_RAIL_CLIENT_SYSCOMMAND, &syscommand);
}

void xf_process_rail_get_sysparams_event(xfInfo* xfi, rdpChannels* channels, RDP_EVENT* event)
{
	RAIL_SYSPARAM_ORDER* sysparam;

	sysparam = (RAIL_SYSPARAM_ORDER*) event->user_data;

	sysparam->workArea.left = xfi->workArea.x;
	sysparam->workArea.top = xfi->workArea.y;
	sysparam->workArea.right = xfi->workArea.x + xfi->workArea.width;
	sysparam->workArea.bottom = xfi->workArea.y + xfi->workArea.height;

	sysparam->taskbarPos.left = 0;
	sysparam->taskbarPos.top = 0;
	sysparam->taskbarPos.right = 0;
	sysparam->taskbarPos.bottom = 0;

	sysparam->dragFullWindows = false;

	xf_send_rail_client_event(channels, RDP_EVENT_TYPE_RAIL_CLIENT_SET_SYSPARAMS, sysparam);
}

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

void xf_process_rail_exec_result_event(xfInfo* xfi, rdpChannels* channels, RDP_EVENT* event)
{
	RAIL_EXEC_RESULT_ORDER* exec_result;

	exec_result = (RAIL_EXEC_RESULT_ORDER*) event->user_data;

	if (exec_result->execResult != RAIL_EXEC_S_OK)
	{
		printf("RAIL exec error: execResult=%s NtError=0x%X\n",
			error_code_names[exec_result->execResult], exec_result->rawResult);
	}
}

void xf_process_rail_server_sysparam_event(xfInfo* xfi, rdpChannels* channels, RDP_EVENT* event)
{
	RAIL_SYSPARAM_ORDER* sysparam = (RAIL_SYSPARAM_ORDER*) event->user_data;

	switch (sysparam->param)
	{
		case SPI_SET_SCREEN_SAVE_ACTIVE:
			break;

		case SPI_SET_SCREEN_SAVE_SECURE:
			break;
	}
}

void xf_process_rail_server_minmaxinfo_event(xfInfo* xfi, rdpChannels* channels, RDP_EVENT* event)
{
	rdpRail* rail;
	rdpWindow* rail_window = NULL;
	RAIL_MINMAXINFO_ORDER* minmax = (RAIL_MINMAXINFO_ORDER*) event->user_data;

	rail = ((rdpContext*) xfi->context)->rail;
	rail_window = window_list_get_by_id(rail->list, minmax->windowId);

	if (rail_window != NULL)
	{
		xfWindow * window = NULL;
		window = (xfWindow *) rail_window->extra;

		DEBUG_X11_LMS("windowId=0x%X maxWidth=%d maxHeight=%d maxPosX=%d maxPosY=%d "
			"minTrackWidth=%d minTrackHeight=%d maxTrackWidth=%d maxTrackHeight=%d",
			minmax->windowId, minmax->maxWidth, minmax->maxHeight,
			(sint16)minmax->maxPosX, (sint16)minmax->maxPosY,
			minmax->minTrackWidth, minmax->minTrackHeight,
			minmax->maxTrackWidth, minmax->maxTrackHeight);

		xf_SetWindowMinMaxInfo(xfi, window, minmax->maxWidth, minmax->maxHeight, minmax->maxPosX, minmax->maxPosY,
			minmax->minTrackWidth, minmax->minTrackHeight, minmax->maxTrackWidth, minmax->maxTrackHeight);
	}
}

const char* movetype_names[] =
{
	"(invalid)",
	"RAIL_WMSZ_LEFT",
	"RAIL_WMSZ_RIGHT",
	"RAIL_WMSZ_TOP",
	"RAIL_WMSZ_TOPLEFT",
	"RAIL_WMSZ_TOPRIGHT",
	"RAIL_WMSZ_BOTTOM",
	"RAIL_WMSZ_BOTTOMLEFT",
	"RAIL_WMSZ_BOTTOMRIGHT",
	"RAIL_WMSZ_MOVE",
	"RAIL_WMSZ_KEYMOVE",
	"RAIL_WMSZ_KEYSIZE"
};

void xf_process_rail_server_localmovesize_event(xfInfo* xfi, rdpChannels* channels, RDP_EVENT* event)
{
	rdpRail* rail;
	rdpWindow* rail_window = NULL;
	RAIL_LOCALMOVESIZE_ORDER* movesize = (RAIL_LOCALMOVESIZE_ORDER*) event->user_data;

	rail = ((rdpContext*) xfi->context)->rail;
	rail_window = window_list_get_by_id(rail->list, movesize->windowId);

	if (rail_window != NULL)
	{
		xfWindow* window = NULL;
		window = (xfWindow*) rail_window->extra;

		DEBUG_X11_LMS("windowId=0x%X isMoveSizeStart=%d moveSizeType=%s PosX=%d PosY=%d",
			movesize->windowId, movesize->isMoveSizeStart,
			movetype_names[movesize->moveSizeType], (sint16) movesize->posX, (sint16) movesize->posY);

		if (movesize->isMoveSizeStart)
			xf_StartLocalMoveSize(xfi, window, movesize->moveSizeType, (int) movesize->posX, (int) movesize->posY);
		else
			xf_StopLocalMoveSize(xfi, window, movesize->moveSizeType, (int) movesize->posX, (int) movesize->posY);
	}

}

void xf_process_rail_appid_resp_event(xfInfo* xfi, rdpChannels* channels, RDP_EVENT* event)
{
	RAIL_GET_APPID_RESP_ORDER* appid_resp =
		(RAIL_GET_APPID_RESP_ORDER*)event->user_data;

	printf("Server Application ID Response PDU: windowId=0x%X "
		"applicationId=(length=%d dump)\n",
		appid_resp->windowId, appid_resp->applicationId.length);

	freerdp_hexdump(appid_resp->applicationId.string, appid_resp->applicationId.length);
}

void xf_process_rail_langbarinfo_event(xfInfo* xfi, rdpChannels* channels, RDP_EVENT* event)
{
	RAIL_LANGBAR_INFO_ORDER* langbar =
		(RAIL_LANGBAR_INFO_ORDER*) event->user_data;

	printf("Language Bar Information PDU: languageBarStatus=0x%X\n",
		langbar->languageBarStatus);
}

void xf_process_rail_event(xfInfo* xfi, rdpChannels* channels, RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_RAIL_CHANNEL_GET_SYSPARAMS:
			xf_process_rail_get_sysparams_event(xfi, channels, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_EXEC_RESULTS:
			xf_process_rail_exec_result_event(xfi, channels, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_SYSPARAM:
			xf_process_rail_server_sysparam_event(xfi, channels, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_MINMAXINFO:
			xf_process_rail_server_minmaxinfo_event(xfi, channels, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_LOCALMOVESIZE:
			xf_process_rail_server_localmovesize_event(xfi, channels, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_APPID_RESP:
			xf_process_rail_appid_resp_event(xfi, channels, event);
			break;

		case RDP_EVENT_TYPE_RAIL_CHANNEL_LANGBARINFO:
			xf_process_rail_langbarinfo_event(xfi, channels, event);
			break;

		default:
			break;
	}
}
