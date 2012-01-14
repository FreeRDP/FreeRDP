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

void xf_rail_enable_remoteapp_mode(xfInfo* xfi)
{
	if (xfi->remote_app == false)
	{
		xfi->remote_app = true;
		xfi->drawable = DefaultRootWindow(xfi->display);
		xf_DestroyWindow(xfi, xfi->window);
		xfi->window = NULL;
	}
}

void xf_rail_paint(xfInfo* xfi, rdpRail* rail, sint32 uleft, sint32 utop, uint32 uright, uint32 ubottom)
{
	xfWindow* xfw;
	rdpWindow* window;
	boolean intersect;
	uint32 iwidth, iheight;
	sint32 ileft, itop;
	uint32 iright, ibottom;
	sint32 wleft, wtop; 
	uint32 wright, wbottom;

	window_list_rewind(rail->list);

	while (window_list_has_next(rail->list))
	{
		window = window_list_get_next(rail->list);
		xfw = (xfWindow*) window->extra;

                // RDP can have zero width or height windows.  X cannot, so we ignore these.

                if (window->windowWidth == 0 || window->windowHeight == 0)
                {
                        continue;
                }

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

	xf_rail_enable_remoteapp_mode(xfi);

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

	// Do nothing if window is already in the correct position
        if ( xfw->left == window->windowOffsetX && 
        	xfw->top == window->windowOffsetY && 
                xfw->width == window->windowWidth && 
                xfw->height == window->windowHeight)
        {
		return;
	}

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

/**
 * The position of the X window can become out of sync with the RDP window
 * if the X window is moved locally by the window manager.  In this event
 * send an update to the RDP server informing it of the new window position
 * and size.
 */
void xf_rail_adjust_position(xfInfo* xfi, rdpWindow *window)
{
	xfWindow* xfw;
	rdpChannels* channels;
	RAIL_WINDOW_MOVE_ORDER window_move;

	xfw = (xfWindow*) window->extra;
	channels = xfi->_context->channels;

	if (! xfw->is_mapped || xfw->local_move.state != LMS_NOT_ACTIVE)
		return;

	// If current window position disagrees with RDP window position, send
	// update to RDP server
	if ( xfw->left != window->windowOffsetX ||
        	xfw->top != window->windowOffsetY ||
                xfw->width != window->windowWidth ||
                xfw->height != window->windowHeight)
        {
		window_move.windowId = window->windowId;
		window_move.left = xfw->left;
		window_move.top = xfw->top;
		window_move.right = xfw->right;
		window_move.bottom = xfw->bottom;

		DEBUG_X11_LMS("window=0x%X rc={l=%d t=%d r=%d b=%d} w=%u h=%u"
			"  RDP=0x%X rc={l=%d t=%d} w=%d h=%d",
			(uint32) xfw->handle, xfw->left, xfw->top, 
			xfw->right, xfw->bottom, xfw->width, xfw->height,
			window->windowId,
			window->windowOffsetX, window->windowOffsetY, 
			window->windowWidth, window->windowHeight);

		xf_send_rail_client_event(channels, RDP_EVENT_TYPE_RAIL_CLIENT_WINDOW_MOVE, &window_move);
        }
}

void xf_rail_end_local_move(xfInfo* xfi, rdpWindow *window)
{
	xfWindow* xfw;
	rdpChannels* channels;
	RAIL_WINDOW_MOVE_ORDER window_move;
	int x,y;
	rdpInput* input = xfi->instance->input;

	xfw = (xfWindow*) window->extra;
	channels = xfi->_context->channels;

	// Send RDP client event to inform RDP server

	window_move.windowId = window->windowId;
	window_move.left = xfw->left;
	window_move.top = xfw->top;
	window_move.right = xfw->right + 1;   // In the update to RDP the position is one past the window
	window_move.bottom = xfw->bottom + 1;

	DEBUG_X11_LMS("window=0x%X rc={l=%d t=%d r=%d b=%d} w=%d h=%d",
        	(uint32) xfw->handle, 
		xfw->left, xfw->top, xfw->right, xfw->bottom,
		xfw->width, xfw->height);

	xf_send_rail_client_event(channels, RDP_EVENT_TYPE_RAIL_CLIENT_WINDOW_MOVE, &window_move);

	// Send synthetic button up event to the RDP server.  This is per the RDP spec to
	// indicate a local move has finished.

	x = xfw->left + xfw->local_move.window_x;
	y = xfw->top + xfw->local_move.window_y;
        input->MouseEvent(input, PTR_FLAGS_BUTTON1, x, y);

	// Proactively update the RAIL window dimensions.  There is a race condition where
	// we can start to receive GDI orders for the new window dimensions before we 
	// receive the RAIL ORDER for the new window size.  This avoids that race condition.

	window->windowOffsetX = xfw->left;
	window->windowOffsetY = xfw->top;
	window->windowWidth = xfw->width;
	window->windowHeight = xfw->height;

	xfw->local_move.state = LMS_TERMINATING;
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
		xfi->disconnect = True;
	}
	else
	{
		xf_rail_enable_remoteapp_mode(xfi);
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
	int direction = 0;
	Window child_window;
	int x,y;

	rail = ((rdpContext*) xfi->context)->rail;
	rail_window = window_list_get_by_id(rail->list, movesize->windowId);

	if (rail_window != NULL)
	{
		xfWindow* xfw = NULL;
		xfw = (xfWindow*) rail_window->extra;

		DEBUG_X11_LMS("windowId=0x%X isMoveSizeStart=%d moveSizeType=%s PosX=%d PosY=%d",
			movesize->windowId, movesize->isMoveSizeStart,
			movetype_names[movesize->moveSizeType], (sint16) movesize->posX, (sint16) movesize->posY);

		switch (movesize->moveSizeType)
		{
			case RAIL_WMSZ_LEFT: //0x1
				direction = _NET_WM_MOVERESIZE_SIZE_LEFT;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_RIGHT: //0x2
				direction = _NET_WM_MOVERESIZE_SIZE_RIGHT;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_TOP: //0x3
				direction = _NET_WM_MOVERESIZE_SIZE_TOP;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_TOPLEFT: //0x4
				direction = _NET_WM_MOVERESIZE_SIZE_TOPLEFT;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_TOPRIGHT: //0x5
				direction = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_BOTTOM: //0x6
				direction = _NET_WM_MOVERESIZE_SIZE_BOTTOM;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_BOTTOMLEFT: //0x7
				direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_BOTTOMRIGHT: //0x8
				direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_MOVE: //0x9
				direction = _NET_WM_MOVERESIZE_MOVE;
				XTranslateCoordinates(xfi->display, xfw->handle, 
					RootWindowOfScreen(xfi->screen), 
					movesize->posX, movesize->posY, &x, &y, &child_window);
				break;
			case RAIL_WMSZ_KEYMOVE: //0xA
				direction = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
				x = movesize->posX;
				y = movesize->posY;
				break;
			case RAIL_WMSZ_KEYSIZE: //0xB
				direction = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
				x = movesize->posX;
				y = movesize->posY;
				break;
		}

		if (movesize->isMoveSizeStart)
		{
			xf_StartLocalMoveSize(xfi, xfw, direction, x, y);
		} else {
			xf_EndLocalMoveSize(xfi, xfw);
		}
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
