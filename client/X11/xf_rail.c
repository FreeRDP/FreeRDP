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
			windowRect.left = appWindow->x;
			windowRect.top = appWindow->y;
			windowRect.right = appWindow->x + appWindow->width;
			windowRect.bottom = appWindow->y + appWindow->height;

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
	rdpRail* rail;
	rdpWindow* window;
	xfAppWindow* appWindow;
	BOOL intersect;
	UINT32 iwidth, iheight;
	INT32 ileft, itop;
	UINT32 iright, ibottom;
	INT32 wleft, wtop;
	UINT32 wright, wbottom;

	if (0)
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

		return;
	}

	rail = ((rdpContext*) xfc)->rail;

	window_list_rewind(rail->list);

	while (window_list_has_next(rail->list))
	{
		window = window_list_get_next(rail->list);
		appWindow = (xfAppWindow*) window->extra;

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
			xf_UpdateWindowArea(xfc, appWindow, ileft - wleft, itop - wtop, iwidth, iheight);
		}
	}
}

#if 1

static void xf_rail_CreateWindow(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfAppWindow* appWindow;

	xfc = (xfContext*) rail->extra;

	xf_rail_enable_remoteapp_mode(xfc);

	appWindow = xf_CreateWindow(xfc, window, window->windowOffsetX, window->windowOffsetY,
			window->windowWidth, window->windowHeight, window->windowId);

	xf_SetWindowStyle(xfc, appWindow, window->style, window->extendedStyle);
	xf_SetWindowText(xfc, appWindow, window->title);

	window->extra = (void*) appWindow;
	window->extraId = (void*) appWindow->handle;
}

static void xf_rail_MoveWindow(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfAppWindow* appWindow;

	xfc = (xfContext*) rail->extra;
	appWindow = (xfAppWindow*) window->extra;

	/*
	 * The rail server like to set the window to a small size when it is minimized even though it is hidden
	 * in some cases this can cause the window not to restore back to its original size. Therefore we don't
	 * update our local window when that rail window state is minimized
	 */
	if (appWindow->rail_state == WINDOW_SHOW_MINIMIZED)
		return;

	/* Do nothing if window is already in the correct position */
	if (appWindow->x == window->visibleOffsetX &&
			appWindow->y == window->visibleOffsetY &&
			appWindow->width == window->windowWidth &&
			appWindow->height == window->windowHeight)
	{
		/*
		 * Just ensure entire window area is updated to handle cases where we
		 * have drawn locally before getting new bitmap from the server
		 */
		xf_UpdateWindowArea(xfc, appWindow, 0, 0, window->windowWidth, window->windowHeight);
		return;
	}

	xf_MoveWindow(xfc, appWindow, window->visibleOffsetX, window->visibleOffsetY,
			window->windowWidth, window->windowHeight);
}

static void xf_rail_ShowWindow(rdpRail* rail, rdpWindow* window, BYTE state)
{
	xfContext* xfc;
	xfAppWindow* appWindow;

	xfc = (xfContext*) rail->extra;
	appWindow = (xfAppWindow*) window->extra;

	xf_ShowWindow(xfc, appWindow, state);
}

static void xf_rail_SetWindowText(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfAppWindow* appWindow;

	xfc = (xfContext*) rail->extra;
	appWindow = (xfAppWindow*) window->extra;

	xf_SetWindowText(xfc, appWindow, window->title);
}

static void xf_rail_SetWindowIcon(rdpRail* rail, rdpWindow* window, rdpIcon* icon)
{
	xfContext* xfc;
	xfAppWindow* appWindow;

	xfc = (xfContext*) rail->extra;
	appWindow = (xfAppWindow*) window->extra;

	icon->extra = freerdp_icon_convert(icon->entry->bitsColor, NULL, icon->entry->bitsMask,
			icon->entry->width, icon->entry->height, icon->entry->bpp, rail->clrconv);

	xf_SetWindowIcon(xfc, appWindow, icon);
}

static void xf_rail_SetWindowRects(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfAppWindow* appWindow;

	xfc = (xfContext*) rail->extra;
	appWindow = (xfAppWindow*) window->extra;

	xf_SetWindowRects(xfc, appWindow, window->windowRects, window->numWindowRects);
}

static void xf_rail_SetWindowVisibilityRects(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfAppWindow* appWindow;

	xfc = (xfContext*) rail->extra;
	appWindow = (xfAppWindow*) window->extra;

	xf_SetWindowVisibilityRects(xfc, appWindow, window->windowRects, window->numWindowRects);
}

static void xf_rail_DestroyWindow(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc;
	xfAppWindow* appWindow;

	xfc = (xfContext*) rail->extra;
	appWindow = (xfAppWindow*) window->extra;

	xf_DestroyWindow(xfc, appWindow);
}

void xf_rail_DesktopNonMonitored(rdpRail* rail, rdpWindow* window)
{
	xfContext* xfc = (xfContext*) rail->extra;
	xf_rail_disable_remoteapp_mode(xfc);
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

#endif

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
void xf_rail_adjust_position(xfContext* xfc, xfAppWindow* appWindow)
{
	rdpWindow* window;
	RAIL_WINDOW_MOVE_ORDER windowMove;

	window = appWindow->window;

	if (!appWindow->is_mapped || appWindow->local_move.state != LMS_NOT_ACTIVE)
		return;

	/* If current window position disagrees with RDP window position, send update to RDP server */
	if (appWindow->x != window->visibleOffsetX ||
			appWindow->y != window->visibleOffsetY ||
			appWindow->width != window->windowWidth ||
			appWindow->height != window->windowHeight)
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
		windowMove.windowId = window->windowId;
		/*
		 * Calculate new offsets for the rail server window
		 * Negative offset correction + rail server window offset + (difference in visibleOffset and new window local offset)
		 */
		windowMove.left = offsetX + window->windowOffsetX + (appWindow->x - window->visibleOffsetX);
		windowMove.top = offsetY + window->windowOffsetY + (appWindow->y - window->visibleOffsetY);
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
	rdpWindow* window;
	unsigned int mask;
	Window root_window;
	Window child_window;
	RAIL_WINDOW_MOVE_ORDER windowMove;
	rdpInput* input = xfc->instance->input;

	window = appWindow->window;

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
	windowMove.windowId = window->windowId;

	/*
	 * Calculate new offsets for the rail server window
	 * Negative offset correction + rail server window offset + (difference in visibleOffset and new window local offset)
	 */
	windowMove.left = offsetX + window->windowOffsetX + (appWindow->x - window->visibleOffsetX);
	windowMove.top = offsetY + window->windowOffsetY + (appWindow->y - window->visibleOffsetY);
	windowMove.right = windowMove.left + appWindow->width; /* In the update to RDP the position is one past the window */
	windowMove.bottom = windowMove.top + appWindow->height;

	xfc->rail->ClientWindowMove(xfc->rail, &windowMove);

	/*
	 * Simulate button up at new position to end the local move (per RDP spec)
	 */
	XQueryPointer(xfc->display, appWindow->handle,
			&root_window, &child_window, &x, &y, &child_x, &child_y, &mask);

	input->MouseEvent(input, PTR_FLAGS_BUTTON1, x, y);

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
	window->windowOffsetX = offsetX + window->windowOffsetX + (appWindow->x - window->visibleOffsetX);
	window->windowOffsetY = offsetY + window->windowOffsetY + (appWindow->y - window->visibleOffsetY);
	window->windowWidth = appWindow->width;
	window->windowHeight = appWindow->height;
	appWindow->local_move.state = LMS_TERMINATING;
}

#if 0

/* RemoteApp Core Protocol Extension */

static void xf_rail_window_common(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	xfAppWindow* railWindow = NULL;
	xfContext* xfc = (xfContext*) context;
	UINT32 fieldFlags = orderInfo->fieldFlags;

	if (fieldFlags & WINDOW_ORDER_STATE_NEW)
	{
		railWindow = (xfAppWindow*) calloc(1, sizeof(xfAppWindow));

		if (!railWindow)
			return;

		railWindow->xfc = xfc;

		railWindow->id = orderInfo->windowId;
		railWindow->dwStyle = windowState->style;
		railWindow->dwExStyle = windowState->extendedStyle;

		railWindow->x = windowState->windowOffsetX;
		railWindow->y = windowState->windowOffsetY;
		railWindow->width = windowState->windowWidth;
		railWindow->height = windowState->windowHeight;

		if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
		{
			char* title = NULL;

			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
				   windowState->titleInfo.length / 2, &title, 0, NULL, NULL);

			railWindow->title = title;
		}
		else
		{
			railWindow->title = _strdup("RdpRailWindow");
		}

		railWindow->xfw = xf_CreateWindow(xfc, railWindow, railWindow->x, railWindow->y,
				railWindow->width, railWindow->height, railWindow->id);

		HashTable_Add(xfc->railWindows, (void*) (UINT_PTR) orderInfo->windowId, (void*) railWindow);

		return;
	}
	else
	{
		railWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);
	}

	if (!railWindow)
		return;

	if ((fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET) ||
		(fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE))
	{
		if (fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
		{
			railWindow->x = windowState->windowOffsetX;
			railWindow->y = windowState->windowOffsetY;
		}

		if (fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
		{
			railWindow->width = windowState->windowWidth;
			railWindow->height = windowState->windowHeight;
		}
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		railWindow->dwStyle = windowState->style;
		railWindow->dwExStyle = windowState->extendedStyle;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		char* title = NULL;

		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
			   windowState->titleInfo.length / 2, &title, 0, NULL, NULL);

		free(railWindow->title);
		railWindow->title = title;
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{

	}
}

static void xf_rail_window_delete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	xfAppWindow* railWindow = NULL;
	xfContext* xfc = (xfContext*) context;

	railWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);

	if (!railWindow)
		return;

	HashTable_Remove(xfc->railWindows, (void*) (UINT_PTR) orderInfo->windowId);

	free(railWindow);
}

static void xf_rail_window_icon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{
	BOOL bigIcon;
	xfAppWindow* railWindow;
	xfContext* xfc = (xfContext*) context;

	railWindow = (xfAppWindow*) HashTable_GetItemValue(xfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);

	if (!railWindow)
		return;

	bigIcon = (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_BIG) ? TRUE : FALSE;
}

static void xf_rail_window_cached_icon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{

}

static void xf_rail_notify_icon_common(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
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
		//ICON_INFO* iconInfo = &(notifyIconState->icon);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{

	}
}

static void xf_rail_notify_icon_create(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	xf_rail_notify_icon_common(context, orderInfo, notifyIconState);
}

static void xf_rail_notify_icon_update(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	xf_rail_notify_icon_common(context, orderInfo, notifyIconState);
}

static void xf_rail_notify_icon_delete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{

}

static void xf_rail_monitored_desktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{

}

static void xf_rail_non_monitored_desktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{

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

#endif

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
	int x = 0, y = 0;
	int direction = 0;
	Window child_window;
	rdpWindow* rail_window;
	xfAppWindow* appWindow;
	xfContext* xfc = (xfContext*) context->custom;

	rail = ((rdpContext*) xfc)->rail;

	rail_window = window_list_get_by_id(rail->list, localMoveSize->windowId);

	if (!rail_window)
		return -1;

	appWindow = (xfAppWindow*) rail_window->extra;

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
		xf_StartLocalMoveSize(xfc, appWindow, direction, x, y);
	}
	else
	{
		xf_EndLocalMoveSize(xfc, appWindow);
	}

	return 1;
}

static int xf_rail_server_min_max_info(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	rdpRail* rail;
	xfAppWindow* appWindow;
	rdpWindow* rail_window;
	xfContext* xfc = (xfContext*) context->custom;

	rail = ((rdpContext*) xfc)->rail;

	rail_window = window_list_get_by_id(rail->list, minMaxInfo->windowId);

	if (!rail_window)
		return -1;

	appWindow = (xfAppWindow*) rail_window->extra;

	xf_SetWindowMinMaxInfo(xfc, appWindow,
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

	//xf_rail_register_update_callbacks(context->update);

	rail->custom = (void*) xfc;

	rail->ServerExecuteResult = xf_rail_server_execute_result;
	rail->ServerSystemParam = xf_rail_server_system_param;
	rail->ServerHandshake = xf_rail_server_handshake;
	rail->ServerHandshakeEx = xf_rail_server_handshake_ex;
	rail->ServerLocalMoveSize = xf_rail_server_local_move_size;
	rail->ServerMinMaxInfo = xf_rail_server_min_max_info;
	rail->ServerLanguageBarInfo = xf_rail_server_language_bar_info;
	rail->ServerGetAppIdResponse = xf_rail_server_get_appid_response;

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
