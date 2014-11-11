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

#include <winpr/wlog.h>
#include <winpr/print.h>

#include <freerdp/utils/rail.h>
#include <freerdp/rail/rail.h>

#include "xf_window.h"
#include "xf_rail.h"

#define TAG CLIENT_TAG("x11")

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

void xf_rail_invalidate_region(xfContext* xfc, REGION16* invalidRegion)
{
	int index;
	int count;
	RECTANGLE_16 updateRect;
	RECTANGLE_16 windowRect;
	ULONG_PTR* pKeys = NULL;
	xfRailWindow* railWindow;
	const RECTANGLE_16* extents;
	REGION16 windowInvalidRegion;

	region16_init(&windowInvalidRegion);

	count = HashTable_GetKeys(xfc->railWindows, &pKeys);

	for (index = 0; index < count; index++)
	{
		railWindow = (xfRailWindow*) HashTable_GetItemValue(xfc->railWindows, (void*) pKeys[index]);

		if (railWindow)
		{
			windowRect.left = railWindow->x;
			windowRect.top = railWindow->y;
			windowRect.right = railWindow->x + railWindow->width;
			windowRect.bottom = railWindow->y + railWindow->height;

			region16_clear(&windowInvalidRegion);
			region16_intersect_rect(&windowInvalidRegion, invalidRegion, &windowRect);

			if (!region16_is_empty(&windowInvalidRegion))
			{
				extents = region16_extents(&windowInvalidRegion);

				updateRect.left = extents->left - railWindow->x;
				updateRect.top = extents->top - railWindow->y;
				updateRect.right = extents->right - railWindow->x;
				updateRect.bottom = extents->bottom - railWindow->y;

				//InvalidateRect(railWindow->hWnd, &updateRect, FALSE);
			}
		}
	}

	region16_uninit(&windowInvalidRegion);
}

void xf_rail_paint(xfContext* xfc, INT32 uleft, INT32 utop, UINT32 uright, UINT32 ubottom)
{
	rdpRail* rail;
	xfWindow* xfw;
	rdpWindow* window;
	BOOL intersect;
	UINT32 iwidth, iheight;
	INT32 ileft, itop;
	UINT32 iright, ibottom;
	INT32 wleft, wtop;
	UINT32 wright, wbottom;

	rail = ((rdpContext*) xfc)->rail;

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

void xf_rail_DesktopNonMonitored(rdpRail* rail, rdpWindow* window)
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

	xfw = xf_CreateWindow(xfc, window, window->windowOffsetX, window->windowOffsetY,
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
	if (xfw->left == window->visibleOffsetX &&
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

	xf_MoveWindow(xfc, xfw, window->visibleOffsetX, window->visibleOffsetY,
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

void xf_rail_send_activate(xfContext* xfc, Window xwindow, BOOL enabled)
{
	rdpRail* rail;
	rdpWindow* rail_window;
	RAIL_ACTIVATE_ORDER activate;

	rail = ((rdpContext*) xfc)->rail;
	rail_window = window_list_get_by_extra_id(rail->list, (void*) xwindow);

	if (!rail_window)
		return;

	activate.windowId = rail_window->windowId;
	activate.enabled = enabled;

	xfc->rail->ClientActivate(xfc->rail, &activate);
}

void xf_rail_send_client_system_command(xfContext* xfc, UINT32 windowId, UINT16 command)
{
	RAIL_SYSCOMMAND_ORDER syscommand;

	syscommand.windowId = windowId;
	syscommand.command = command;

	xfc->rail->ClientSystemCommand(xfc->rail, &syscommand);
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
	RAIL_WINDOW_MOVE_ORDER window_move;

	xfw = (xfWindow*) window->extra;

	if (! xfw->is_mapped || xfw->local_move.state != LMS_NOT_ACTIVE)
		return;

	/* If current window position disagrees with RDP window position, send update to RDP server */
	if (xfw->left != window->visibleOffsetX ||
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
		window_move.left = offsetX + window->windowOffsetX + (xfw->left - window->visibleOffsetX);
		window_move.top = offsetY + window->windowOffsetY + (xfw->top - window->visibleOffsetY);
		window_move.right = window_move.left + xfw->width;
		window_move.bottom = window_move.top + xfw->height;

		xfc->rail->ClientWindowMove(xfc->rail, &window_move);
	}
}

void xf_rail_end_local_move(xfContext* xfc, rdpWindow* window)
{
	int x, y;
	int child_x;
	int child_y;
	xfWindow* xfw;
	unsigned int mask;
	Window root_window;
	Window child_window;
	RAIL_WINDOW_MOVE_ORDER window_move;
	rdpInput* input = xfc->instance->input;

	xfw = (xfWindow*) window->extra;

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
	window_move.left = offsetX + window->windowOffsetX + (xfw->left - window->visibleOffsetX);
	window_move.top = offsetY + window->windowOffsetY + (xfw->top - window->visibleOffsetY);
	window_move.right = window_move.left + xfw->width; /* In the update to RDP the position is one past the window */
	window_move.bottom = window_move.top + xfw->height;

	xfc->rail->ClientWindowMove(xfc->rail, &window_move);

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
	}

	/*
	 * Proactively update the RAIL window dimensions.  There is a race condition where
	 * we can start to receive GDI orders for the new window dimensions before we
	 * receive the RAIL ORDER for the new window size.  This avoids that race condition.
	 */
	window->windowOffsetX = offsetX + window->windowOffsetX + (xfw->left - window->visibleOffsetX);
	window->windowOffsetY = offsetY + window->windowOffsetY + (xfw->top - window->visibleOffsetY);
	window->windowWidth = xfw->width;
	window->windowHeight = xfw->height;
	xfw->local_move.state = LMS_TERMINATING;
}

/* RemoteApp Virtual Channel Extension */

static int xf_rail_server_execute_result(RailClientContext* context, RAIL_EXEC_RESULT_ORDER* execResult)
{
	xfContext* xfc = (xfContext*) context->custom;

	if (execResult->execResult != RAIL_EXEC_S_OK)
	{
		WLog_ERR(TAG, "RAIL exec error: execResult=%s NtError=0x%X\n",
				 error_code_names[execResult->execResult], execResult->rawResult);
		xfc->disconnect = TRUE;
	}
	else
	{
		xf_rail_enable_remoteapp_mode(xfc);
	}

	return 1;
}

static int xf_rail_server_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return 1;
}

static int xf_rail_server_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	RAIL_EXEC_ORDER exec;
	RAIL_SYSPARAM_ORDER sysparam;
	RAIL_HANDSHAKE_ORDER clientHandshake;
	RAIL_CLIENT_STATUS_ORDER clientStatus;
	xfContext* xfc = (xfContext*) context->custom;
	rdpSettings* settings = xfc->settings;

	clientHandshake.buildNumber = 0x00001DB0;
	context->ClientHandshake(context, &clientHandshake);

	ZeroMemory(&clientStatus, sizeof(RAIL_CLIENT_STATUS_ORDER));
	clientStatus.flags = RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;
	context->ClientInformation(context, &clientStatus);

	if (settings->RemoteAppLanguageBarSupported)
	{
		RAIL_LANGBAR_INFO_ORDER langBarInfo;
		langBarInfo.languageBarStatus = 0x00000008; /* TF_SFT_HIDDEN */
		context->ClientLanguageBarInfo(context, &langBarInfo);
	}

	ZeroMemory(&sysparam, sizeof(RAIL_SYSPARAM_ORDER));

	sysparam.params = 0;

	sysparam.params |= SPI_MASK_SET_HIGH_CONTRAST;
	sysparam.highContrast.colorScheme.string = NULL;
	sysparam.highContrast.colorScheme.length = 0;
	sysparam.highContrast.flags = 0x7E;

	sysparam.params |= SPI_MASK_SET_MOUSE_BUTTON_SWAP;
	sysparam.mouseButtonSwap = FALSE;

	sysparam.params |= SPI_MASK_SET_KEYBOARD_PREF;
	sysparam.keyboardPref = FALSE;

	sysparam.params |= SPI_MASK_SET_DRAG_FULL_WINDOWS;
	sysparam.dragFullWindows = FALSE;

	sysparam.params |= SPI_MASK_SET_KEYBOARD_CUES;
	sysparam.keyboardCues = FALSE;

	sysparam.params |= SPI_MASK_SET_WORK_AREA;
	sysparam.workArea.left = 0;
	sysparam.workArea.top = 0;
	sysparam.workArea.right = settings->DesktopWidth;
	sysparam.workArea.bottom = settings->DesktopHeight;

	sysparam.dragFullWindows = FALSE;

	context->ClientSystemParam(context, &sysparam);

	ZeroMemory(&exec, sizeof(RAIL_EXEC_ORDER));

	exec.RemoteApplicationProgram = settings->RemoteApplicationProgram;
	exec.RemoteApplicationWorkingDir = settings->ShellWorkingDirectory;
	exec.RemoteApplicationArguments = settings->RemoteApplicationCmdLine;

	context->ClientExecute(context, &exec);

	return 1;
}

static int xf_rail_server_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	return 1;
}

static int xf_rail_server_local_move_size(RailClientContext* context, RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	rdpRail* rail;
	xfWindow* window;
	int x = 0, y = 0;
	int direction = 0;
	Window child_window;
	rdpWindow* rail_window;
	xfContext* xfc = (xfContext*) context->custom;

	rail = ((rdpContext*) xfc)->rail;

	rail_window = window_list_get_by_id(rail->list, localMoveSize->windowId);

	if (!rail_window)
		return -1;

	window = (xfWindow*) rail_window->extra;

	switch (localMoveSize->moveSizeType)
	{
		case RAIL_WMSZ_LEFT:
			direction = _NET_WM_MOVERESIZE_SIZE_LEFT;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_RIGHT:
			direction = _NET_WM_MOVERESIZE_SIZE_RIGHT;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_TOP:
			direction = _NET_WM_MOVERESIZE_SIZE_TOP;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_TOPLEFT:
			direction = _NET_WM_MOVERESIZE_SIZE_TOPLEFT;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_TOPRIGHT:
			direction = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_BOTTOM:
			direction = _NET_WM_MOVERESIZE_SIZE_BOTTOM;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_BOTTOMLEFT:
			direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_BOTTOMRIGHT:
			direction = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_MOVE:
			direction = _NET_WM_MOVERESIZE_MOVE;
			XTranslateCoordinates(xfc->display, window->handle,
					RootWindowOfScreen(xfc->screen),
					localMoveSize->posX, localMoveSize->posY, &x, &y, &child_window);
			break;

		case RAIL_WMSZ_KEYMOVE:
			direction = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			/* FIXME: local keyboard moves not working */
			return 1;
			break;

		case RAIL_WMSZ_KEYSIZE:
			direction = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			/* FIXME: local keyboard moves not working */
			return 1;
			break;
	}

	if (localMoveSize->isMoveSizeStart)
	{
		xf_StartLocalMoveSize(xfc, window, direction, x, y);
	}
	else
	{
		xf_EndLocalMoveSize(xfc, window);
	}

	return 1;
}

static int xf_rail_server_min_max_info(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	rdpRail* rail;
	xfWindow* window;
	rdpWindow* rail_window;
	xfContext* xfc = (xfContext*) context->custom;

	rail = ((rdpContext*) xfc)->rail;

	rail_window = window_list_get_by_id(rail->list, minMaxInfo->windowId);

	if (!rail_window)
		return -1;

	window = (xfWindow*) rail_window->extra;

	xf_SetWindowMinMaxInfo(xfc, window,
			minMaxInfo->maxWidth, minMaxInfo->maxHeight,
			minMaxInfo->maxPosX, minMaxInfo->maxPosY,
			minMaxInfo->minTrackWidth, minMaxInfo->minTrackHeight,
			minMaxInfo->maxTrackWidth, minMaxInfo->maxTrackHeight);

	return 1;
}

static int xf_rail_server_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	return 1;
}

static int xf_rail_server_get_appid_response(RailClientContext* context, RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	return 1;
}

int xf_rail_init(xfContext* xfc, RailClientContext* rail)
{
	rdpContext* context = (rdpContext*) xfc;

	xfc->rail = rail;

	context->rail = rail_new(context->settings);
	rail_register_update_callbacks(context->rail, context->update);

	xf_rail_register_callbacks(xfc, context->rail);

	if (1)
	{
		rail->custom = (void*) xfc;

		rail->ServerExecuteResult = xf_rail_server_execute_result;
		rail->ServerSystemParam = xf_rail_server_system_param;
		rail->ServerHandshake = xf_rail_server_handshake;
		rail->ServerHandshakeEx = xf_rail_server_handshake_ex;
		rail->ServerLocalMoveSize = xf_rail_server_local_move_size;
		rail->ServerMinMaxInfo = xf_rail_server_min_max_info;
		rail->ServerLanguageBarInfo = xf_rail_server_language_bar_info;
		rail->ServerGetAppIdResponse = xf_rail_server_get_appid_response;
	}

	return 1;
}

int xf_rail_uninit(xfContext* xfc, RailClientContext* rail)
{
	rdpContext* context = (rdpContext*) xfc;

	if (xfc->rail)
	{
		xfc->rail->custom = NULL;
		xfc->rail = NULL;
	}

	if (context->rail)
	{
		rail_free(context->rail);
		context->rail = NULL;
	}

	return 1;
}
