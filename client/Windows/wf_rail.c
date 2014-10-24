/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/print.h>

#include "wf_rail.h"

/* RemoteApp Core Protocol Extension */

struct _WINDOW_STYLE
{
	UINT32 style;
	const char* name;
	BOOL multi;
};
typedef struct _WINDOW_STYLE WINDOW_STYLE;

static const WINDOW_STYLE WINDOW_STYLES[] =
{
	{ WS_BORDER, "WS_BORDER", FALSE },
	{ WS_CAPTION, "WS_CAPTION", FALSE },
	{ WS_CHILD, "WS_CHILD", FALSE },
	{ WS_CLIPCHILDREN, "WS_CLIPCHILDREN", FALSE },
	{ WS_CLIPSIBLINGS, "WS_CLIPSIBLINGS", FALSE },
	{ WS_DISABLED, "WS_DISABLED", FALSE },
	{ WS_DLGFRAME, "WS_DLGFRAME", FALSE },
	{ WS_GROUP, "WS_GROUP", FALSE },
	{ WS_HSCROLL, "WS_HSCROLL", FALSE },
	{ WS_ICONIC, "WS_ICONIC", FALSE },
	{ WS_MAXIMIZE, "WS_MAXIMIZE", FALSE },
	{ WS_MAXIMIZEBOX, "WS_MAXIMIZEBOX", FALSE },
	{ WS_MINIMIZE, "WS_MINIMIZE", FALSE },
	{ WS_MINIMIZEBOX, "WS_MINIMIZEBOX", FALSE },
	{ WS_OVERLAPPED, "WS_OVERLAPPED", FALSE },
	{ WS_OVERLAPPEDWINDOW, "WS_OVERLAPPEDWINDOW", TRUE },
	{ WS_POPUP, "WS_POPUP", FALSE },
	{ WS_POPUPWINDOW, "WS_POPUPWINDOW", TRUE },
	{ WS_SIZEBOX, "WS_SIZEBOX", FALSE },
	{ WS_SYSMENU, "WS_SYSMENU", FALSE },
	{ WS_TABSTOP, "WS_TABSTOP", FALSE },
	{ WS_THICKFRAME, "WS_THICKFRAME", FALSE },
	{ WS_VISIBLE, "WS_VISIBLE", FALSE }
};

static const WINDOW_STYLE EXTENDED_WINDOW_STYLES[] =
{
	{ WS_EX_ACCEPTFILES, "WS_EX_ACCEPTFILES", FALSE },
	{ WS_EX_APPWINDOW, "WS_EX_APPWINDOW", FALSE },
	{ WS_EX_CLIENTEDGE, "WS_EX_CLIENTEDGE", FALSE },
	{ WS_EX_COMPOSITED, "WS_EX_COMPOSITED", FALSE },
	{ WS_EX_CONTEXTHELP, "WS_EX_CONTEXTHELP", FALSE },
	{ WS_EX_CONTROLPARENT, "WS_EX_CONTROLPARENT", FALSE },
	{ WS_EX_DLGMODALFRAME, "WS_EX_DLGMODALFRAME", FALSE },
	{ WS_EX_LAYERED, "WS_EX_LAYERED", FALSE },
	{ WS_EX_LAYOUTRTL, "WS_EX_LAYOUTRTL", FALSE },
	{ WS_EX_LEFT, "WS_EX_LEFT", FALSE },
	{ WS_EX_LEFTSCROLLBAR, "WS_EX_LEFTSCROLLBAR", FALSE },
	{ WS_EX_LTRREADING, "WS_EX_LTRREADING", FALSE },
	{ WS_EX_MDICHILD, "WS_EX_MDICHILD", FALSE },
	{ WS_EX_NOACTIVATE, "WS_EX_NOACTIVATE", FALSE },
	{ WS_EX_NOINHERITLAYOUT, "WS_EX_NOINHERITLAYOUT", FALSE },
	{ WS_EX_NOPARENTNOTIFY, "WS_EX_NOPARENTNOTIFY", FALSE },
	{ WS_EX_OVERLAPPEDWINDOW, "WS_EX_OVERLAPPEDWINDOW", TRUE },
	{ WS_EX_PALETTEWINDOW, "WS_EX_PALETTEWINDOW", TRUE },
	{ WS_EX_RIGHT, "WS_EX_RIGHT", FALSE },
	{ WS_EX_RIGHTSCROLLBAR, "WS_EX_RIGHTSCROLLBAR", FALSE },
	{ WS_EX_RTLREADING, "WS_EX_RTLREADING", FALSE },
	{ WS_EX_STATICEDGE, "WS_EX_STATICEDGE", FALSE },
	{ WS_EX_TOOLWINDOW, "WS_EX_TOOLWINDOW", FALSE },
	{ WS_EX_TOPMOST, "WS_EX_TOPMOST", FALSE },
	{ WS_EX_TRANSPARENT, "WS_EX_TRANSPARENT", FALSE },
	{ WS_EX_WINDOWEDGE, "WS_EX_WINDOWEDGE", FALSE }
};

void PrintWindowStyles(UINT32 style)
{
	int i;
	
	fprintf(stderr, "\tWindow Styles:\n\t{\n");
	for (i = 0; i < ARRAYSIZE(WINDOW_STYLES); i++)
	{
		if (style & WINDOW_STYLES[i].style)
		{
			if (WINDOW_STYLES[i].multi)
			{
				if ((style & WINDOW_STYLES[i].style) != WINDOW_STYLES[i].style)
					continue;
			}
			
			fprintf(stderr, "\t\t%s\n", WINDOW_STYLES[i].name);
		}
	}
	fprintf(stderr, "\t}\n");
}

void PrintExtendedWindowStyles(UINT32 style)
{
	int i;
	
	fprintf(stderr, "\tExtended Window Styles:\n\t{\n");
	for (i = 0; i < ARRAYSIZE(EXTENDED_WINDOW_STYLES); i++)
	{
		if (style & EXTENDED_WINDOW_STYLES[i].style)
		{
			if (EXTENDED_WINDOW_STYLES[i].multi)
			{
				if ((style & EXTENDED_WINDOW_STYLES[i].style) != EXTENDED_WINDOW_STYLES[i].style)
					continue;
			}
			
			fprintf(stderr, "\t\t%s\n", EXTENDED_WINDOW_STYLES[i].name);
		}
	}
	fprintf(stderr, "\t}\n");
}

void PrintRailWindowState(WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
		fprintf(stderr, "WindowCreate: WindowId: 0x%04X\n", orderInfo->windowId);
	else
		fprintf(stderr, "WindowUpdate: WindowId: 0x%04X\n", orderInfo->windowId);

	fprintf(stderr, "{\n");

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		fprintf(stderr, "\tOwnerWindowId: 0x%04X\n", windowState->ownerWindowId);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		fprintf(stderr, "\tStyle: 0x%04X ExtendedStyle: 0x%04X\n",
			windowState->style, windowState->extendedStyle);
		
		PrintWindowStyles(windowState->style);
		PrintExtendedWindowStyles(windowState->extendedStyle);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		fprintf(stderr, "\tShowState: %d\n", windowState->showState);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		char* title = NULL;

		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
				   windowState->titleInfo.length / 2, &title, 0, NULL, NULL);

		fprintf(stderr, "\tTitleInfo: %s (length = %d)\n", title,
			windowState->titleInfo.length);

		free(title);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{
		fprintf(stderr, "\tClientOffsetX: %d ClientOffsetY: %d\n",
			windowState->clientOffsetX, windowState->clientOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		fprintf(stderr, "\tClientAreaWidth: %d ClientAreaHeight: %d\n",
			windowState->clientAreaWidth, windowState->clientAreaHeight);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{
		fprintf(stderr, "\tRPContent: %d\n", windowState->RPContent);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{
		fprintf(stderr, "\tRootParentHandle: 0x%04X\n", windowState->rootParentHandle);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		fprintf(stderr, "\tWindowOffsetX: %d WindowOffsetY: %d\n",
			windowState->windowOffsetX, windowState->windowOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		fprintf(stderr, "\tWindowClientDeltaX: %d WindowClientDeltaY: %d\n",
			windowState->windowClientDeltaX, windowState->windowClientDeltaY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		fprintf(stderr, "\tWindowWidth: %d WindowHeight: %d\n",
			windowState->windowWidth, windowState->windowHeight);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		UINT32 index;
		RECTANGLE_16* rect;

		fprintf(stderr, "\tnumWindowRects: %d\n", windowState->numWindowRects);

		for (index = 0; index < windowState->numWindowRects; index++)
		{
			rect = &windowState->windowRects[index];

			fprintf(stderr, "\twindowRect[%d]: left: %d top: %d right: %d bottom: %d\n",
				index, rect->left, rect->top, rect->right, rect->bottom);
		}
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		fprintf(stderr, "\tvisibileOffsetX: %d visibleOffsetY: %d\n",
			windowState->visibleOffsetX, windowState->visibleOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		UINT32 index;
		RECTANGLE_16* rect;

		fprintf(stderr, "\tnumVisibilityRects: %d\n", windowState->numVisibilityRects);

		for (index = 0; index < windowState->numVisibilityRects; index++)
		{
			rect = &windowState->visibilityRects[index];

			fprintf(stderr, "\tvisibilityRect[%d]: left: %d top: %d right: %d bottom: %d\n",
				index, rect->left, rect->top, rect->right, rect->bottom);
		}
	}

	fprintf(stderr, "}\n");
}

static void PrintRailIconInfo(WINDOW_ORDER_INFO* orderInfo, ICON_INFO* iconInfo)
{
	fprintf(stderr, "ICON_INFO\n");
	fprintf(stderr, "{\n");

	fprintf(stderr, "\tbigIcon: %s\n", (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_BIG) ? "true" : "false");
	fprintf(stderr, "\tcacheEntry; 0x%04X\n", iconInfo->cacheEntry);
	fprintf(stderr, "\tcacheId: 0x%04X\n", iconInfo->cacheId);
	fprintf(stderr, "\tbpp: %d\n", iconInfo->bpp);
	fprintf(stderr, "\twidth: %d\n", iconInfo->width);
	fprintf(stderr, "\theight: %d\n", iconInfo->height);
	fprintf(stderr, "\tcbColorTable: %d\n", iconInfo->cbColorTable);
	fprintf(stderr, "\tcbBitsMask: %d\n", iconInfo->cbBitsMask);
	fprintf(stderr, "\tcbBitsColor: %d\n", iconInfo->cbBitsColor);
	fprintf(stderr, "\tcolorTable: %p\n", iconInfo->colorTable);
	fprintf(stderr, "\tbitsMask: %p\n", iconInfo->bitsMask);
	fprintf(stderr, "\tbitsColor: %p\n", iconInfo->bitsColor);

	fprintf(stderr, "}\n");
}

static void wf_rail_window_common(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	PrintRailWindowState(orderInfo, windowState);

	if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
	{

	}
	else
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
	
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
	
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		char* title = NULL;

		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
				   windowState->titleInfo.length / 2, &title, 0, NULL, NULL);

		free(title);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{

	}
}

static void wf_rail_window_delete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	fprintf(stderr, "RailWindowDelete\n");
}

static void wf_rail_window_icon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{
	BOOL bigIcon;
	ICON_INFO* iconInfo;
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	fprintf(stderr, "RailWindowIcon\n");

	PrintRailIconInfo(orderInfo, windowIcon->iconInfo);

	iconInfo = windowIcon->iconInfo;

	bigIcon = (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_BIG) ? TRUE : FALSE;

	if (iconInfo->cacheEntry != 0xFFFF)
	{
		/* icon should be cached */
	}
}

static void wf_rail_window_cached_icon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	fprintf(stderr, "RailWindowCachedIcon\n");
}

static void wf_rail_notify_icon_common(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
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
		ICON_INFO* iconInfo = &(notifyIconState->icon);

		PrintRailIconInfo(orderInfo, iconInfo);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{

	}
}

static void wf_rail_notify_icon_create(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	fprintf(stderr, "RailNotifyIconCreate\n");

	wf_rail_notify_icon_common(context, orderInfo, notifyIconState);
}

static void wf_rail_notify_icon_update(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	fprintf(stderr, "RailNotifyIconUpdate\n");

	wf_rail_notify_icon_common(context, orderInfo, notifyIconState);
}

static void wf_rail_notify_icon_delete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	fprintf(stderr, "RailNotifyIconDelete\n");
}

static void wf_rail_monitored_desktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	fprintf(stderr, "RailMonitorDesktop\n");
}

static void wf_rail_non_monitored_desktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	fprintf(stderr, "RailNonMonitorDesktop\n");
}

void wf_rail_register_update_callbacks(rdpUpdate* update)
{
	rdpWindowUpdate* window = update->window;

	window->WindowCreate = wf_rail_window_common;
	window->WindowUpdate = wf_rail_window_common;
	window->WindowDelete = wf_rail_window_delete;
	window->WindowIcon = wf_rail_window_icon;
	window->WindowCachedIcon = wf_rail_window_cached_icon;
	window->NotifyIconCreate = wf_rail_notify_icon_create;
	window->NotifyIconUpdate = wf_rail_notify_icon_update;
	window->NotifyIconDelete = wf_rail_notify_icon_delete;
	window->MonitoredDesktop = wf_rail_monitored_desktop;
	window->NonMonitoredDesktop = wf_rail_non_monitored_desktop;
}

/* RemoteApp Virtual Channel Extension */

static int wf_rail_server_execute_result(RailClientContext* context, RAIL_EXEC_RESULT_ORDER* execResult)
{
	fprintf(stderr, "RailServerExecuteResult: 0x%04X\n", execResult->rawResult);
	return 1;
}

static int wf_rail_server_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return 1;
}

static int wf_rail_server_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	RAIL_EXEC_ORDER exec;
	RAIL_SYSPARAM_ORDER sysparam;
	RAIL_HANDSHAKE_ORDER clientHandshake;
	RAIL_CLIENT_STATUS_ORDER clientStatus;
	wfContext* wfc = (wfContext*) context->custom;
	rdpSettings* settings = wfc->settings;

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

static int wf_rail_server_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	return 1;
}

static int wf_rail_server_local_move_size(RailClientContext* context, RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	return 1;
}

static int wf_rail_server_min_max_info(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	return 1;
}

static int wf_rail_server_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	return 1;
}

static int wf_rail_server_get_appid_response(RailClientContext* context, RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	return 1;
}

void wf_rail_init(wfContext* wfc, RailClientContext* rail)
{
	wfc->rail = rail;
	rail->custom = (void*) wfc;

	rail->ServerExecuteResult = wf_rail_server_execute_result;
	rail->ServerSystemParam = wf_rail_server_system_param;
	rail->ServerHandshake = wf_rail_server_handshake;
	rail->ServerHandshakeEx = wf_rail_server_handshake_ex;
	rail->ServerLocalMoveSize = wf_rail_server_local_move_size;
	rail->ServerMinMaxInfo = wf_rail_server_min_max_info;
	rail->ServerLanguageBarInfo = wf_rail_server_language_bar_info;
	rail->ServerGetAppIdResponse = wf_rail_server_get_appid_response;
}

void wf_rail_uninit(wfContext* wfc, RailClientContext* rail)
{
	wfc->rail = NULL;
	rail->custom = NULL;
}
