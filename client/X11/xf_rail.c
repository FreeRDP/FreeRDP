/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <freerdp/utils/event.h>
#include <winpr/print.h>
#include <freerdp/utils/rail.h>
#include <freerdp/rail/rail.h>

#include "xf_window.h"
#include "xf_rail.h"

#ifdef WITH_DEBUG_X11_LOCAL_MOVESIZE
#define DEBUG_X11_LMS(fmt, ...) DEBUG_CLASS(X11_LMS, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11_LMS(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

void xf_rail_enable_remoteapp_mode(xfContext* xfc)
{
	if (!xfc->remote_app)
	{
		xfc->remote_app = TRUE;
		xfc->drawable = DefaultRootWindow(xfc->display);
		xf_DestroyWindow(xfc, xfc->window);
		xfc->window = NULL;
	}
}

void xf_rail_disable_remoteapp_mode(xfContext* xfc)
{
	if (xfc->remote_app)
	{
		xfc->remote_app = FALSE;
		xf_create_window(xfc);
	}
}

void xf_rail_paint(xfContext* xfc, rdpRail* rail, INT32 uleft, INT32 utop, UINT32 uright, UINT32 ubottom)
{
	xfWindow* xfw;
	rdpWindow* window;
	BOOL intersect;
	UINT32 iwidth, iheight;
	INT32 ileft, itop;
	UINT32 iright, ibottom;
	INT32 wleft, wtop; 
	UINT32 wright, wbottom;

	window_list_rewind(rail->list);

	while (window_list_has_next(rail->list))
	{
		window = window_list_get_next(rail->list);
		xfw = (xfWindow*) window->extra;

                /* RDP can have zero width or height windows. X cannot, so we ignore these. */

                if ((window->windowWidth == 0) || (window->windowHeight == 0))
                {
                        continue;
                }

		wleft = window->visibleOffsetX;
		wtop = window->visibleOffsetY;
		wright = window->visibleOffsetX + window->windowWidth - 1;
		wbottom = window->visibleOffsetY + window->windowHeight - 1;

		ileft = MAX(uleft, wleft);
		itop = MAX(utop, wtop);
		iright = MIN(uright, wright);
		ibottom = MIN(ubottom, wbottom);

		iwidth = iright - ileft + 1;
		iheight = ibottom - itop + 1;

		intersect = ((iright > ileft) && (ibottom > itop)) ? TRUE : FALSE;

		if (intersect)
		{
			xf_UpdateWindowArea(xfc, xfw, ileft - wleft, itop - wtop, iwidth, iheight);
		}
	}
}

void xf_rail_DesktopNonMonitored(rdpRail *rail, rdpWindow* window)
{
	xfContext* xfc;

	xfc = (xfContext*) rail->extra;
	xf_rail_disable_remoteapp_mode(xfc);
}

static void xf_rail_CreateWindow(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfWindow* xfw;

	xfc = (xfContext*) rail->extra;

	xf_rail_enable_remoteapp_mode(xfc);

	xfw = xf_CreateWindow(xfc, window,
			window->windowOffsetX, window->windowOffsetY,
			window->windowWidth, window->windowHeight, window->windowId);

	xf_SetWindowStyle(xfc, xfw, window->style, window->extendedStyle);

	xf_SetWindowText(xfc, xfw, window->title);

	window->extra = (void*) xfw;
	window->extraId = (void*) xfw->handle;
}

static void xf_rail_MoveWindow(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfWindow* xfw;

	xfc = (xfContext*) rail->extra;
	xfw = (xfWindow*) window->extra;

	/*
	 * The rail server like to set the window to a small size when it is minimized even though it is hidden
	 * in some cases this can cause the window not to restore back to its original size. Therefore we don't
	 * update our local window when that rail window state is minimized
	 */
	if (xfw->rail_state == WINDOW_SHOW_MINIMIZED)
               return;

	/* Do nothing if window is already in the correct position */
        if ( xfw->left == window->visibleOffsetX && 
             xfw->top == window->visibleOffsetY &&  
             xfw->width == window->windowWidth &&
             xfw->height == window->windowHeight)
        {
	     /*
	      * Just ensure entire window area is updated to handle cases where we
	      * have drawn locally before getting new bitmap from the server
	      */
             xf_UpdateWindowArea(xfc, xfw, 0, 0, window->windowWidth, window->windowHeight);
             return;
        }

	xf_MoveWindow(xfc, xfw,
		window->visibleOffsetX, window->visibleOffsetY,
		window->windowWidth, window->windowHeight);
}

static void xf_rail_ShowWindow(rdpRail* rail, rdpWindow* window, BYTE state)
{
	xfContext* xfc;
	xfWindow* xfw;

	xfc = (xfContext*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_ShowWindow(xfc, xfw, state);
}

static void xf_rail_SetWindowText(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfWindow* xfw;

	xfc = (xfContext*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_SetWindowText(xfc, xfw, window->title);
}

static void xf_rail_SetWindowIcon(rdpRail* rail, rdpWindow* window, rdpIcon* icon)
{
	xfContext* xfc;
	xfWindow* xfw;

	xfc = (xfContext*) rail->extra;
	xfw = (xfWindow*) window->extra;

	icon->extra = freerdp_icon_convert(icon->entry->bitsColor, NULL, icon->entry->bitsMask,
			icon->entry->width, icon->entry->height, icon->entry->bpp, rail->clrconv);

	xf_SetWindowIcon(xfc, xfw, icon);
}

static void xf_rail_SetWindowRects(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfWindow* xfw;

	xfc = (xfContext*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_SetWindowRects(xfc, xfw, window->windowRects, window->numWindowRects);
}

static void xf_rail_SetWindowVisibilityRects(rdpRail* rail, rdpWindow* window)
{
	xfWindow* xfw;
	xfContext* xfc;

	xfc = (xfContext*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_SetWindowVisibilityRects(xfc, xfw, window->windowRects, window->numWindowRects);
}

static void xf_rail_DestroyWindow(rdpRail* rail, rdpWindow* window)
{
	xfWindow* xfw;
	xfContext* xfc;

	xfc = (xfContext*) rail->extra;
	xfw = (xfWindow*) window->extra;

	xf_DestroyWindow(xfc, xfw);
}

void xf_rail_register_callbacks(xfContext* xfc, rdpRail* rail)
{
	rail->extra = (void*) xfc;
	rail->rail_CreateWindow = xf_rail_CreateWindow;
	rail->rail_MoveWindow = xf_rail_MoveWindow;
	rail->rail_ShowWindow = xf_rail_ShowWindow;
	rail->rail_SetWindowText = xf_rail_SetWindowText;
	rail->rail_SetWindowIcon = xf_rail_SetWindowIcon;
	rail->rail_SetWindowRects = xf_rail_SetWindowRects;
	rail->rail_SetWindowVisibilityRects = xf_rail_SetWindowVisibilityRects;
	rail->rail_DestroyWindow = xf_rail_DestroyWindow;
	rail->rail_DesktopNonMonitored = xf_rail_DesktopNonMonitored;
}

static void xf_on_free_rail_client_event(wMessage* event)
{
	rail_free_cloned_order(GetMessageType(event->id), event->wParam);
}

static void xf_send_rail_client_event(rdpChannels* channels, UINT16 event_type, void* param)
{
	wMessage* out_event = NULL;
	void* payload = NULL;

	payload = rail_clone_order(event_type, param);

	if (payload != NULL)
	{
		out_event = freerdp_event_new(RailChannel_Class, event_type,
			xf_on_free_rail_client_event, payload);

		freerdp_channels_send_event(channels, out_event);
	}
}

void xf_rail_send_activate(xfContext* xfc, Window xwindow, BOOL enabled)
{
	rdpRail* rail;
	rdpChannels* channels;
	rdpWindow* rail_window;
	RAIL_ACTIVATE_ORDER activate;

	rail = ((rdpContext*) xfc)->rail;
	channels = ((rdpContext*) xfc)->channels;

	rail_window = window_list_get_by_extra_id(rail->list, (void*) xwindow);

	if (rail_window == NULL)
		return;

	activate.windowId = rail_window->windowId;
	activate.enabled = enabled;

	xf_send_rail_client_event(channels, RailChannel_ClientActivate, &activate);
}

void xf_rail_send_client_system_command(xfContext* xfc, UINT32 windowId, UINT16 command)
{
	rdpChannels* channels;
	RAIL_SYSCOMMAND_ORDER syscommand;

	channels = ((rdpContext*) xfc)->channels;

	syscommand.windowId = windowId;
	syscommand.command = command;

	xf_send_rail_client_event(channels, RailChannel_ClientSystemCommand, &syscommand);
}

/**
 * The position of the X window can become out of sync with the RDP window
 * if the X window is moved locally by the window manager.  In this event
 * send an update to the RDP server informing it of the new window position
 * and size.
 */
void xf_rail_adjust_position(xfContext* xfc, rdpWindow* window)
{
	xfWindow* xfw;
	rdpChannels* channels;
	RAIL_WINDOW_MOVE_ORDER window_move;

	xfw = (xfWindow*) window->extra;
	channels = ((rdpContext*) xfc)->channels;

	if (! xfw->is_mapped || xfw->local_move.state != LMS_NOT_ACTIVE)
		return;

	/* If current window position disagrees with RDP window position, send update to RDP server */
	if ( xfw->left != window->visibleOffsetX ||
        	xfw->top != window->visibleOffsetY ||
                xfw->width != window->windowWidth ||
                xfw->height != window->windowHeight)
        {
	       /*
	        * Although the rail server can give negative window coordinates when updating windowOffsetX and windowOffsetY,
	        * we can only send unsigned integers to the rail server. Therefore, we always bring negative coordinates up to 0
	        * when attempting to adjust the rail window.
	        */
	       UINT32 offsetX = 0;
               UINT32 offsetY = 0;

               if (window->windowOffsetX < 0)
                       offsetX = offsetX - window->windowOffsetX;

               if (window->windowOffsetY < 0)
                       offsetY = offsetY - window->windowOffsetY;

		/*
		 * windowOffset corresponds to the window location on the rail server
		 * but our local window is based on the visibleOffset since using the windowOffset
		 * can result in blank areas for a maximized window
		 */
		window_move.windowId = window->windowId;

		/*
		 * Calculate new offsets for the rail server window
		 * Negative offset correction + rail server window offset + (difference in visibleOffset and new window local offset)
		 */
		window_move.left = offsetX + window->windowOffsetX +  (xfw->left - window->visibleOffsetX);
                window_move.top = offsetY + window->windowOffsetY + (xfw->top - window->visibleOffsetY);
               
                window_move.right = window_move.left + xfw->width;
                window_move.bottom = window_move.top + xfw->height;

		DEBUG_X11_LMS("window=0x%X rc={l=%d t=%d r=%d b=%d} w=%u h=%u"
			"  RDP=0x%X rc={l=%d t=%d} w=%d h=%d",
			(UINT32) xfw->handle, window_move.left, window_move.top, 
			window_move.right, window_move.bottom, xfw->width, xfw->height,
			window->windowId,
			window->windowOffsetX, window->windowOffsetY, 
			window->windowWidth, window->windowHeight);

		xf_send_rail_client_event(channels, RailChannel_ClientWindowMove, &window_move);
        }
}

void xf_rail_end_local_move(xfContext* xfc, rdpWindow *window)
{
	xfWindow* xfw;
	rdpChannels* channels;
	RAIL_WINDOW_MOVE_ORDER window_move;
	rdpInput* input = xfc->instance->input;
	int x,y;
	Window root_window;
	Window child_window;
	unsigned int mask;
	int child_x;
	int child_y;

	xfw = (xfWindow*) window->extra;
	channels = ((rdpContext*) xfc)->channels;

	DEBUG_X11_LMS("window=0x%X rc={l=%d t=%d r=%d b=%d} w=%d h=%d",
        	(UINT32) xfw->handle, 
		xfw->left, xfw->top, xfw->right, xfw->bottom,
		xfw->width, xfw->height);

	/*
	 * Although the rail server can give negative window coordinates when updating windowOffsetX and windowOffsetY,
	 * we can only send unsigned integers to the rail server. Therefore, we always bring negative coordinates up to 0 when
	 * attempting to adjust the rail window.
	 */
	UINT32 offsetX = 0;
        UINT32 offsetY = 0;

        if (window->windowOffsetX < 0)
                offsetX = offsetX - window->windowOffsetX;

        if (window->windowOffsetY < 0)
                offsetY = offsetY - window->windowOffsetY;

	/* 
	 * For keyboard moves send and explicit update to RDP server 
	 */ 
	window_move.windowId = window->windowId;

	/*
	 * Calculate new offsets for the rail server window
	 * Negative offset correction + rail server window offset + (difference in visibleOffset and new window local offset)
	 */
	window_move.left = offsetX + window->windowOffsetX +  (xfw->left - window->visibleOffsetX);
        window_move.top = offsetY + window->windowOffsetY + (xfw->top - window->visibleOffsetY);
       
        window_move.right = window_move.left + xfw->width; /* In the update to RDP the position is one past the window */
        window_move.bottom = window_move.top + xfw->height;

	xf_send_rail_client_event(channels, RailChannel_ClientWindowMove, &window_move);
	
	/*
	 * Simulate button up at new position to end the local move (per RDP spec)
	 */

	XQueryPointer(xfc->display, xfw->handle, 
		&root_window, &child_window, 
		&x, &y, &child_x, &child_y, &mask);
        input->MouseEvent(input, PTR_FLAGS_BUTTON1, x, y);

	/* only send the mouse coordinates if not a keyboard move or size */
	if ((xfw->local_move.direction != _NET_WM_MOVERESIZE_MOVE_KEYBOARD) &&
            (xfw->local_move.direction != _NET_WM_MOVERESIZE_SIZE_KEYBOARD))
        {       
                input->MouseEvent(input, PTR_FLAGS_BUTTON1, x, y);
                DEBUG_X11_LMS("Mouse coordinates.  x= %i, y= %i", x, y);
        }
	
	/*
	 * Proactively update the RAIL window dimensions.  There is a race condition where
	 * we can start to receive GDI orders for the new window dimensions before we
	 * receive the RAIL ORDER for the new window size.  This avoids that race condition.
	 */

	window->windowOffsetX = offsetX + window->windowOffsetX +  (xfw->left - window->visibleOffsetX);
        window->windowOffsetY = offsetY + window->windowOffsetY + (xfw->top - window->visibleOffsetY);
	window->windowWidth = xfw->width;
	window->windowHeight = xfw->height;

	xfw->local_move.state = LMS_TERMINATING;
}

void xf_process_rail_get_sysparams_event(xfContext* xfc, rdpChannels* channels, wMessage* event)
{
	RAIL_SYSPARAM_ORDER* sysparam;

	sysparam = (RAIL_SYSPARAM_ORDER*) event->wParam;

	sysparam->workArea.left = xfc->workArea.x;
	sysparam->workArea.top = xfc->workArea.y;
	sysparam->workArea.right = xfc->workArea.x + xfc->workArea.width;
	sysparam->workArea.bottom = xfc->workArea.y + xfc->workArea.height;

	sysparam->taskbarPos.left = 0;
	sysparam->taskbarPos.top = 0;
	sysparam->taskbarPos.right = 0;
	sysparam->taskbarPos.bottom = 0;

	sysparam->dragFullWindows = FALSE;

	xf_send_rail_client_event(channels, RailChannel_ClientSystemParam, sysparam);
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

void xf_process_rail_exec_result_event(xfContext* xfc, rdpChannels* channels, wMessage* event)
{
	RAIL_EXEC_RESULT_ORDER* exec_result;

	exec_result = (RAIL_EXEC_RESULT_ORDER*) event->wParam;

	if (exec_result->execResult != RAIL_EXEC_S_OK)
	{
		fprintf(stderr, "RAIL exec error: execResult=%s NtError=0x%X\n",
			error_code_names[exec_result->execResult], exec_result->rawResult);
		xfc->disconnect = True;
	}
	else
	{
		xf_rail_enable_remoteapp_mode(xfc);
	}
}

void xf_process_rail_server_sysparam_event(xfContext* xfc, rdpChannels* channels, wMessage* event)
{
	RAIL_SYSPARAM_ORDER* sysparam = (RAIL_SYSPARAM_ORDER*) event->wParam;

	switch (sysparam->param)
	{
		case SPI_SET_SCREEN_SAVE_ACTIVE:
			break;

		case SPI_SET_SCREEN_SAVE_SECURE:
			break;
	}
}

void xf_process_rail_server_minmaxinfo_event(xfContext* xfc, rdpChannels* channels, wMessage* event)
{
	rdpRail* rail;
	rdpWindow* rail_window = NULL;
	RAIL_MINMAXINFO_ORDER* minmax = (RAIL_MINMAXINFO_ORDER*) event->wParam;

	rail = ((rdpContext*) xfc)->rail;
	rail_window = window_list_get_by_id(rail->list, minmax->windowId);

	if (rail_window != NULL)
	{
		xfWindow * window = NULL;
		window = (xfWindow *) rail_window->extra;

		DEBUG_X11_LMS("windowId=0x%X maxWidth=%d maxHeight=%d maxPosX=%d maxPosY=%d "
			"minTrackWidth=%d minTrackHeight=%d maxTrackWidth=%d maxTrackHeight=%d",
			minmax->windowId, minmax->maxWidth, minmax->maxHeight,
			(INT16)minmax->maxPosX, (INT16)minmax->maxPosY,
			minmax->minTrackWidth, minmax->minTrackHeight,
			minmax->maxTrackWidth, minmax->maxTrackHeight);

		xf_SetWindowMinMaxInfo(xfc, window, minmax->maxWidth, minmax->maxHeight, minmax->maxPosX, minmax->maxPosY,
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

void xf_process_rail_server_localmovesize_event(xfContext* xfc, rdpChannels* channels, wMessage* event)
{
	int x, y;
	rdpRail* rail;
	int direction = 0;
	Window child_window;
	rdpWindow* rail_window = NULL;
	RAIL_LOCALMOVESIZE_ORDER* movesize = (RAIL_LOCALMOVESIZE_ORDER*) event->wParam;

	rail = ((rdpContext*) xfc)->rail;
	rail_window = window_list_get_by_id(rail->list, movesize->windowId);

	if (rail_window != NULL)
	{
		xfWindow* xfw = NULL;
		xfw = (xfWindow*) rail_window->extra;

		DEBUG_X11_LMS("windowId=0x%X isMoveSizeStart=%d moveSizeType=%s PosX=%d PosY=%d",
			movesize->windowId, movesize->isMoveSizeStart,
			movetype_names[movesize->moveSizeType], (INT16) movesize->posX, (INT16) movesize->posY);

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
				XTranslateCoordinates(xfc->display, xfw->handle, 
					RootWindowOfScreen(xfc->screen), 
					movesize->posX, movesize->posY, &x, &y, &child_window);
				break;
			case RAIL_WMSZ_KEYMOVE: //0xA
				direction = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
				x = movesize->posX;
				y = movesize->posY;
				/* FIXME: local keyboard moves not working */
				return;
			case RAIL_WMSZ_KEYSIZE: //0xB
				direction = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
				x = movesize->posX;
				y = movesize->posY;
				/* FIXME: local keyboard moves not working */
				return;
		}

		if (movesize->isMoveSizeStart)
		{
			xf_StartLocalMoveSize(xfc, xfw, direction, x, y);
		} else {
			xf_EndLocalMoveSize(xfc, xfw);
		}
	}
}

void xf_process_rail_appid_resp_event(xfContext* xfc, rdpChannels* channels, wMessage* event)
{
	RAIL_GET_APPID_RESP_ORDER* appid_resp =
		(RAIL_GET_APPID_RESP_ORDER*) event->wParam;

	fprintf(stderr, "Server Application ID Response PDU: windowId=0x%X "
		"applicationId=(length=%d dump)\n",
		appid_resp->windowId, appid_resp->applicationId.length);

	winpr_HexDump(appid_resp->applicationId.string, appid_resp->applicationId.length);
}

void xf_process_rail_langbarinfo_event(xfContext* xfc, rdpChannels* channels, wMessage* event)
{
	RAIL_LANGBAR_INFO_ORDER* langbar =
		(RAIL_LANGBAR_INFO_ORDER*) event->wParam;

	fprintf(stderr, "Language Bar Information PDU: languageBarStatus=0x%X\n",
		langbar->languageBarStatus);
}

void xf_process_rail_event(xfContext* xfc, rdpChannels* channels, wMessage* event)
{
	switch (GetMessageType(event->id))
	{
		case RailChannel_GetSystemParam:
			xf_process_rail_get_sysparams_event(xfc, channels, event);
			break;

		case RailChannel_ServerExecuteResult:
			xf_process_rail_exec_result_event(xfc, channels, event);
			break;

		case RailChannel_ServerSystemParam:
			xf_process_rail_server_sysparam_event(xfc, channels, event);
			break;

		case RailChannel_ServerMinMaxInfo:
			xf_process_rail_server_minmaxinfo_event(xfc, channels, event);
			break;

		case RailChannel_ServerLocalMoveSize:
			xf_process_rail_server_localmovesize_event(xfc, channels, event);
			break;

		case RailChannel_ServerGetAppIdResponse:
			xf_process_rail_appid_resp_event(xfc, channels, event);
			break;

		case RailChannel_ServerLanguageBarInfo:
			xf_process_rail_langbarinfo_event(xfc, channels, event);
			break;

		default:
			break;
	}
}
