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
		xf_DestroyDesktopWindow(xfc, xfc->window);
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

void xf_rail_send_activate(xfContext* xfc, Window xwindow, BOOL enabled)
{
	xfAppWindow* appWindow;
	RAIL_ACTIVATE_ORDER activate;

	appWindow = xf_AppWindowFromX11Window(xfc, xwindow);

	if (!appWindow)
		return;

	activate.windowId = appWindow->windowId;
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
void xf_rail_adjust_position(xfContext* xfc, xfAppWindow* appWindow)
{
	RAIL_WINDOW_MOVE_ORDER windowMove;

	if (!appWindow->is_mapped || appWindow->local_move.state != LMS_NOT_ACTIVE)
		return;

	/* If current window position disagrees with RDP window position, send update to RDP server */
	if (appWindow->x != appWindow->windowOffsetX ||
			appWindow->y != appWindow->windowOffsetY ||
			appWindow->width != appWindow->windowWidth ||
			appWindow->height != appWindow->windowHeight)
	{
		windowMove.windowId = appWindow->windowId;
		/*
		 * Calculate new size/position for the rail window(new values for windowOffsetX/windowOffsetY/windowWidth/windowHeight) on the server
		 */
		windowMove.left = appWindow->x;
		windowMove.top = appWindow->y;
		windowMove.right = windowMove.left + appWindow->width;
		windowMove.bottom = windowMove.top + appWindow->height;

		xfc->rail->ClientWindowMove(xfc->rail, &windowMove);
	}
}

void xf_rail_end_local_move(xfContext* xfc, xfAppWindow* appWindow)
{
	int x, y;
	int child_x;
	int child_y;
	unsigned int mask;
	Window root_window;
	Window child_window;
	RAIL_WINDOW_MOVE_ORDER windowMove;
	rdpInput* input = xfc->instance->input;

	/*
	 * For keyboard moves send and explicit update to RDP server
	 */
	windowMove.windowId = appWindow->windowId;

	/*
	 * Calculate new size/position for the rail window(new values for windowOffsetX/windowOffsetY/windowWidth/windowHeight) on the server
	 *
	 */
	windowMove.left = appWindow->x;
	windowMove.top = appWindow->y;
	windowMove.right = windowMove.left + appWindow->width; /* In the update to RDP the position is one past the window */
	windowMove.bottom = windowMove.top + appWindow->height;

	xfc->rail->ClientWindowMove(xfc->rail, &windowMove);

	/*
	 * Simulate button up at new position to end the local move (per RDP spec)
	 */
	XQueryPointer(xfc->display, appWindow->handle,
			&root_window, &child_window, &x, &y, &child_x, &child_y, &mask);

	/* only send the mouse coordinates if not a keyboard move or size */
	if ((appWindow->local_move.direction != _NET_WM_MOVERESIZE_MOVE_KEYBOARD) &&
			(appWindow->local_move.direction != _NET_WM_MOVERESIZE_SIZE_KEYBOARD))
	{
		input->MouseEvent(input, PTR_FLAGS_BUTTON1, x, y);
	}

	/*
	 * Proactively update the RAIL window dimensions.  There is a race condition where
	 * we can start to receive GDI orders for the new window dimensions before we
	 * receive the RAIL ORDER for the new window size.  This avoids that race condition.
	 */
	appWindow->windowOffsetX = appWindow->x;
	appWindow->windowOffsetY = appWindow->y;
	appWindow->windowWidth = appWindow->width;
	appWindow->windowHeight = appWindow->height;
	appWindow->local_move.state = LMS_TERMINATING;
}

void xf_rail_invalidate_region(xfContext* xfc, REGION16* invalidRegion)
{
	int index;
	int count;
	RECTANGLE_16 updateRect;
	RECTANGLE_16 windowRect;
	ULONG_PTR* pKeys = NULL;
	xfAppWindow* appWindow;
	const RECTANGLE_16* extents;
	REGION16 windowInvalidRegion;

	region16_init(&windowInvalidRegion);

	count = HashTable_GetKeys(xfc->railWindows, &pKeys);

	for (index = 0; index < count; index++)
	{
		appWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows, (void*) pKeys[index]);

		if (appWindow)
		{
			windowRect.left = MAX(appWindow->x, 0);
			windowRect.top = MAX(appWindow->y, 0);
			windowRect.right = MAX(appWindow->x + appWindow->width, 0);
			windowRect.bottom = MAX(appWindow->y + appWindow->height, 0);

			region16_clear(&windowInvalidRegion);
			region16_intersect_rect(&windowInvalidRegion, invalidRegion, &windowRect);

			if (!region16_is_empty(&windowInvalidRegion))
			{
				extents = region16_extents(&windowInvalidRegion);

				updateRect.left = extents->left - appWindow->x;
				updateRect.top = extents->top - appWindow->y;
				updateRect.right = extents->right - appWindow->x;
				updateRect.bottom = extents->bottom - appWindow->y;

				if (appWindow)
				{
					xf_UpdateWindowArea(xfc, appWindow, updateRect.left, updateRect.top,
							updateRect.right - updateRect.left,
							updateRect.bottom - updateRect.top);
				}
			}
		}
	}

	region16_uninit(&windowInvalidRegion);
}

void xf_rail_paint(xfContext* xfc, INT32 uleft, INT32 utop, UINT32 uright, UINT32 ubottom)
{
	REGION16 invalidRegion;
	RECTANGLE_16 invalidRect;

	invalidRect.left = uleft;
	invalidRect.top = utop;
	invalidRect.right = uright;
	invalidRect.bottom = ubottom;

	region16_init(&invalidRegion);
	region16_union_rect(&invalidRegion, &invalidRegion, &invalidRect);

	xf_rail_invalidate_region(xfc, &invalidRegion);

	region16_uninit(&invalidRegion);
}

/* RemoteApp Core Protocol Extension */

static BOOL xf_rail_window_common(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	xfAppWindow* appWindow = NULL;
	xfContext* xfc = (xfContext*) context;
	UINT32 fieldFlags = orderInfo->fieldFlags;
	BOOL position_or_size_updated = FALSE;

	if (fieldFlags & WINDOW_ORDER_STATE_NEW)
	{
		appWindow = (xfAppWindow*) calloc(1, sizeof(xfAppWindow));

		if (!appWindow)
			return FALSE;

		appWindow->xfc = xfc;

		appWindow->windowId = orderInfo->windowId;
		appWindow->dwStyle = windowState->style;
		appWindow->dwExStyle = windowState->extendedStyle;

		appWindow->x = appWindow->windowOffsetX = windowState->windowOffsetX;
		appWindow->y = appWindow->windowOffsetY = windowState->windowOffsetY;
		appWindow->width = appWindow->windowWidth = windowState->windowWidth;
		appWindow->height = appWindow->windowHeight = windowState->windowHeight;

		/* Ensure window always gets a window title */
		if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
		{
			char* title = NULL;

			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
				   windowState->titleInfo.length / 2, &title, 0, NULL, NULL);

			appWindow->title = title;
		}
		else
		{
			appWindow->title = _strdup("RdpRailWindow");
		}
		if (!appWindow->title)
		{
			free(appWindow);
			return FALSE;
		}

		HashTable_Add(xfc->railWindows, (void*) (UINT_PTR) orderInfo->windowId, (void*) appWindow);

		xf_AppWindowInit(xfc, appWindow);
	}
	else
	{
		appWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);
	}

	if (!appWindow)
		return FALSE;

	/* Keep track of any position/size update so that we can force a refresh of the window */
	if ((fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET) ||
		(fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)   ||
		(fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET) ||
		(fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE) ||
		(fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA) ||
		(fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET) ||
		(fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY))
	{
		position_or_size_updated = TRUE;
	}				


	/* Update Parameters */

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		appWindow->windowOffsetX = windowState->windowOffsetX;
		appWindow->windowOffsetY = windowState->windowOffsetY;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		appWindow->windowWidth = windowState->windowWidth;
		appWindow->windowHeight = windowState->windowHeight;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		appWindow->ownerWindowId = windowState->ownerWindowId;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		appWindow->dwStyle = windowState->style;
		appWindow->dwExStyle = windowState->extendedStyle;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		appWindow->showState = windowState->showState;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		char* title = NULL;

		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
			   windowState->titleInfo.length / 2, &title, 0, NULL, NULL);

		free(appWindow->title);
		appWindow->title = title;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{
		appWindow->clientOffsetX = windowState->clientOffsetX;
		appWindow->clientOffsetY = windowState->clientOffsetY;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		appWindow->clientAreaWidth = windowState->clientAreaWidth;
		appWindow->clientAreaHeight = windowState->clientAreaHeight;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		appWindow->windowClientDeltaX = windowState->windowClientDeltaX;
		appWindow->windowClientDeltaY = windowState->windowClientDeltaY;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		if (appWindow->windowRects)
		{
			free(appWindow->windowRects);
			appWindow->windowRects = NULL;
		}

		appWindow->numWindowRects = windowState->numWindowRects;

		if (appWindow->numWindowRects)
		{
			appWindow->windowRects = (RECTANGLE_16*) calloc(appWindow->numWindowRects, sizeof(RECTANGLE_16));

			if (!appWindow->windowRects)
				return FALSE;

			CopyMemory(appWindow->windowRects, windowState->windowRects,
					appWindow->numWindowRects * sizeof(RECTANGLE_16));
		}
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		appWindow->visibleOffsetX = windowState->visibleOffsetX;
		appWindow->visibleOffsetY = windowState->visibleOffsetY;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		if (appWindow->visibilityRects)
		{
			free(appWindow->visibilityRects);
			appWindow->visibilityRects = NULL;
		}

		appWindow->numVisibilityRects = windowState->numVisibilityRects;

		if (appWindow->numVisibilityRects)
		{
			appWindow->visibilityRects = (RECTANGLE_16*) calloc(appWindow->numVisibilityRects, sizeof(RECTANGLE_16));

			if (!appWindow->visibilityRects)
				return FALSE;

			CopyMemory(appWindow->visibilityRects, windowState->visibilityRects,
					appWindow->numVisibilityRects * sizeof(RECTANGLE_16));
		}
	}

	/* Update Window */

	if (fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		xf_ShowWindow(xfc, appWindow, appWindow->showState);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		if (appWindow->title)
			xf_SetWindowText(xfc, appWindow, appWindow->title);
	}

	if (position_or_size_updated)
	{
		UINT32 visibilityRectsOffsetX = (appWindow->visibleOffsetX - (appWindow->clientOffsetX - appWindow->windowClientDeltaX));
		UINT32 visibilityRectsOffsetY = (appWindow->visibleOffsetY - (appWindow->clientOffsetY - appWindow->windowClientDeltaY));

		/*
		 * The rail server like to set the window to a small size when it is minimized even though it is hidden
		 * in some cases this can cause the window not to restore back to its original size. Therefore we don't
		 * update our local window when that rail window state is minimized
		 */
		if (appWindow->rail_state != WINDOW_SHOW_MINIMIZED)
		{

			/* Redraw window area if already in the correct position */
			if (appWindow->x == appWindow->windowOffsetX &&
				appWindow->y == appWindow->windowOffsetY &&
				appWindow->width == appWindow->windowWidth &&
				appWindow->height == appWindow->windowHeight)
			{
				xf_UpdateWindowArea(xfc, appWindow, 0, 0, appWindow->windowWidth, appWindow->windowHeight);
			}
			else
			{
				xf_MoveWindow(xfc, appWindow, appWindow->windowOffsetX, appWindow->windowOffsetY,
					appWindow->windowWidth, appWindow->windowHeight);
			}

	                xf_SetWindowVisibilityRects(xfc, appWindow, visibilityRectsOffsetX, visibilityRectsOffsetY, appWindow->visibilityRects, appWindow->numVisibilityRects);
		}
	}

	/* We should only be using the visibility rects for shaping the window */
	/*if (fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		xf_SetWindowRects(xfc, appWindow, appWindow->windowRects, appWindow->numWindowRects);
	}*/

	return TRUE;
}

static BOOL xf_rail_window_delete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	xfAppWindow* appWindow = NULL;
	xfContext* xfc = (xfContext*) context;

	appWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);

	if (!appWindow)
		return TRUE;

	HashTable_Remove(xfc->railWindows, (void*) (UINT_PTR) orderInfo->windowId);

	xf_DestroyWindow(xfc, appWindow);
	return TRUE;
}

static BOOL xf_rail_window_icon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{
	BOOL bigIcon;
	xfAppWindow* railWindow;
	xfContext* xfc = (xfContext*) context;

	railWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);

	if (!railWindow)
		return FALSE;

	bigIcon = (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_BIG) ? TRUE : FALSE;

	return TRUE;
}

static BOOL xf_rail_window_cached_icon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	return TRUE;
}

static BOOL xf_rail_notify_icon_common(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_VERSION)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_STATE)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_ICON)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{

	}
	return TRUE;
}

static BOOL xf_rail_notify_icon_create(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	return xf_rail_notify_icon_common(context, orderInfo, notifyIconState);
}

static BOOL xf_rail_notify_icon_update(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	return xf_rail_notify_icon_common(context, orderInfo, notifyIconState);
}

static BOOL xf_rail_notify_icon_delete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	return TRUE;
}

static BOOL xf_rail_monitored_desktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	return TRUE;
}

static BOOL xf_rail_non_monitored_desktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	xfContext* xfc = (xfContext*) context;
	xf_rail_disable_remoteapp_mode(xfc);
	return TRUE;
}

void xf_rail_register_update_callbacks(rdpUpdate* update)
{
	rdpWindowUpdate* window = update->window;

	window->WindowCreate = xf_rail_window_common;
	window->WindowUpdate = xf_rail_window_common;
	window->WindowDelete = xf_rail_window_delete;
	window->WindowIcon = xf_rail_window_icon;
	window->WindowCachedIcon = xf_rail_window_cached_icon;
	window->NotifyIconCreate = xf_rail_notify_icon_create;
	window->NotifyIconUpdate = xf_rail_notify_icon_update;
	window->NotifyIconDelete = xf_rail_notify_icon_delete;
	window->MonitoredDesktop = xf_rail_monitored_desktop;
	window->NonMonitoredDesktop = xf_rail_non_monitored_desktop;
}

/* RemoteApp Virtual Channel Extension */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_rail_server_execute_result(RailClientContext* context, RAIL_EXEC_RESULT_ORDER* execResult)
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

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_rail_server_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_rail_server_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
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

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_rail_server_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_rail_server_local_move_size(RailClientContext* context, RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	int x = 0, y = 0;
	int direction = 0;
	Window child_window;
	xfAppWindow* appWindow = NULL;
	xfContext* xfc = (xfContext*) context->custom;

	appWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows,
			(void*) (UINT_PTR) localMoveSize->windowId);

	if (!appWindow)
		return ERROR_INTERNAL_ERROR;

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
			XTranslateCoordinates(xfc->display, appWindow->handle,
					RootWindowOfScreen(xfc->screen),
					localMoveSize->posX, localMoveSize->posY, &x, &y, &child_window);
			break;

		case RAIL_WMSZ_KEYMOVE:
			direction = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			/* FIXME: local keyboard moves not working */
			return CHANNEL_RC_OK;
			break;

		case RAIL_WMSZ_KEYSIZE:
			direction = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			/* FIXME: local keyboard moves not working */
			return CHANNEL_RC_OK;
			break;
	}

	if (localMoveSize->isMoveSizeStart)
	{
		xf_StartLocalMoveSize(xfc, appWindow, direction, x, y);
	}
	else
	{
		xf_EndLocalMoveSize(xfc, appWindow);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_rail_server_min_max_info(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	xfAppWindow* appWindow = NULL;
	xfContext* xfc = (xfContext*) context->custom;

	appWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows,
			(void*) (UINT_PTR) minMaxInfo->windowId);

	if (!appWindow)
		return ERROR_INTERNAL_ERROR;

	xf_SetWindowMinMaxInfo(xfc, appWindow,
			minMaxInfo->maxWidth, minMaxInfo->maxHeight,
			minMaxInfo->maxPosX, minMaxInfo->maxPosY,
			minMaxInfo->minTrackWidth, minMaxInfo->minTrackHeight,
			minMaxInfo->maxTrackWidth, minMaxInfo->maxTrackHeight);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_rail_server_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_rail_server_get_appid_response(RailClientContext* context, RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	return CHANNEL_RC_OK;
}

int xf_rail_init(xfContext* xfc, RailClientContext* rail)
{
	rdpContext* context = (rdpContext*) xfc;

	xfc->rail = rail;

	xf_rail_register_update_callbacks(context->update);

	rail->custom = (void*) xfc;

	rail->ServerExecuteResult = xf_rail_server_execute_result;
	rail->ServerSystemParam = xf_rail_server_system_param;
	rail->ServerHandshake = xf_rail_server_handshake;
	rail->ServerHandshakeEx = xf_rail_server_handshake_ex;
	rail->ServerLocalMoveSize = xf_rail_server_local_move_size;
	rail->ServerMinMaxInfo = xf_rail_server_min_max_info;
	rail->ServerLanguageBarInfo = xf_rail_server_language_bar_info;
	rail->ServerGetAppIdResponse = xf_rail_server_get_appid_response;

	xfc->railWindows = HashTable_New(TRUE);
	if (!xfc->railWindows)
		return 0;

	return 1;
}

int xf_rail_uninit(xfContext* xfc, RailClientContext* rail)
{
	if (xfc->rail)
	{
		xfc->rail->custom = NULL;
		xfc->rail = NULL;
	}

	if (xfc->railWindows)
	{
		HashTable_Free(xfc->railWindows);
		xfc->railWindows = NULL;
	}

	return 1;
}
