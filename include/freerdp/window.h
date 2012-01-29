/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Window Alternate Secondary Drawing Orders Interface API
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

#ifndef __UPDATE_WINDOW_H
#define __UPDATE_WINDOW_H

#include <freerdp/types.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Window Order Header Flags */
#define WINDOW_ORDER_TYPE_WINDOW			0x01000000
#define WINDOW_ORDER_TYPE_NOTIFY			0x02000000
#define WINDOW_ORDER_TYPE_DESKTOP			0x04000000
#define WINDOW_ORDER_STATE_NEW				0x10000000
#define WINDOW_ORDER_STATE_DELETED			0x20000000
#define WINDOW_ORDER_FIELD_OWNER			0x00000002
#define WINDOW_ORDER_FIELD_STYLE			0x00000008
#define WINDOW_ORDER_FIELD_SHOW				0x00000010
#define WINDOW_ORDER_FIELD_TITLE			0x00000004
#define WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET		0x00004000
#define WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE		0x00010000
#define WINDOW_ORDER_FIELD_RP_CONTENT			0x00020000
#define WINDOW_ORDER_FIELD_ROOT_PARENT			0x00040000
#define WINDOW_ORDER_FIELD_WND_OFFSET			0x00000800
#define WINDOW_ORDER_FIELD_WND_CLIENT_DELTA		0x00008000
#define WINDOW_ORDER_FIELD_WND_SIZE			0x00000400
#define WINDOW_ORDER_FIELD_WND_RECTS			0x00000100
#define WINDOW_ORDER_FIELD_VIS_OFFSET			0x00001000
#define WINDOW_ORDER_FIELD_VISIBILITY			0x00000200
#define WINDOW_ORDER_FIELD_ICON_BIG			0x00002000
#define WINDOW_ORDER_ICON				0x40000000
#define WINDOW_ORDER_CACHED_ICON			0x80000000
#define WINDOW_ORDER_FIELD_NOTIFY_VERSION		0x00000008
#define WINDOW_ORDER_FIELD_NOTIFY_TIP			0x00000001
#define WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP		0x00000002
#define WINDOW_ORDER_FIELD_NOTIFY_STATE			0x00000004
#define WINDOW_ORDER_FIELD_DESKTOP_NONE			0x00000001
#define WINDOW_ORDER_FIELD_DESKTOP_HOOKED		0x00000002
#define WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED	0x00000004
#define WINDOW_ORDER_FIELD_DESKTOP_ARC_BEGAN		0x00000008
#define WINDOW_ORDER_FIELD_DESKTOP_ZORDER		0x00000010
#define WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND		0x00000020

/* Window Show States */
#define WINDOW_HIDE					0x00
#define WINDOW_SHOW_MINIMIZED				0x02
#define WINDOW_SHOW_MAXIMIZED				0x03
#define WINDOW_SHOW					0x05

/* Window Styles */
#ifndef _WIN32
#define WS_BORDER			0x00800000
#define WS_CAPTION			0x00C00000
#define WS_CHILD			0x40000000
#define WS_CLIPCHILDREN			0x02000000
#define WS_CLIPSIBLINGS			0x04000000
#define WS_DISABLED			0x08000000
#define WS_DLGFRAME			0x00400000
#define WS_GROUP			0x00020000
#define WS_HSCROLL			0x00100000
#define WS_ICONIC			0x20000000
#define WS_MAXIMIZE			0x01000000
#define WS_MAXIMIZEBOX			0x00010000
#define WS_MINIMIZE			0x20000000
#define WS_MINIMIZEBOX			0x00020000
#define WS_OVERLAPPED			0x00000000
#define WS_OVERLAPPEDWINDOW		(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUP			0x80000000
#define WS_POPUPWINDOW			(WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_SIZEBOX			0x00040000
#define WS_SYSMENU			0x00080000
#define WS_TABSTOP			0x00010000
#define WS_THICKFRAME			0x00040000
#define WS_VISIBLE			0x10000000
#define WS_VSCROLL			0x00200000
#endif

/* Extended Window Styles */
#ifndef _WIN32
#define WS_EX_ACCEPTFILES		0x00000010
#define WS_EX_APPWINDOW			0x00040000
#define WS_EX_CLIENTEDGE		0x00000200
#define WS_EX_COMPOSITED		0x02000000
#define WS_EX_CONTEXTHELP		0x00000400
#define WS_EX_CONTROLPARENT		0x00010000
#define WS_EX_DLGMODALFRAME		0x00000001
#define WS_EX_LAYERED			0x00080000
#define WS_EX_LAYOUTRTL			0x00400000
#define WS_EX_LEFT			0x00000000
#define WS_EX_LEFTSCROLLBAR		0x00004000
#define WS_EX_LTRREADING		0x00000000
#define WS_EX_MDICHILD			0x00000040
#define WS_EX_NOACTIVATE		0x08000000
#define WS_EX_NOINHERITLAYOUT		0x00100000
#define WS_EX_NOPARENTNOTIFY		0x00000004
#define WS_EX_OVERLAPPEDWINDOW		(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)
#define WS_EX_PALETTEWINDOW		(WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST)
#define WS_EX_RIGHT			0x00001000
#define WS_EX_RIGHTSCROLLBAR		0x00000000
#define WS_EX_RTLREADING		0x00002000
#define WS_EX_STATICEDGE		0x00020000
#define WS_EX_TOOLWINDOW		0x00000080
#define WS_EX_TOPMOST			0x00000008
#define WS_EX_TRANSPARENT		0x00000020
#define WS_EX_WINDOWEDGE		0x00000100
#endif

struct _WINDOW_ORDER_INFO
{
	uint32 windowId;
	uint32 fieldFlags;
	uint32 notifyIconId;
};
typedef struct _WINDOW_ORDER_INFO WINDOW_ORDER_INFO;

struct _ICON_INFO
{
	uint32 cacheEntry;
	uint32 cacheId;
	uint32 bpp;
	uint32 width;
	uint32 height;
	uint32 cbColorTable;
	uint32 cbBitsMask;
	uint32 cbBitsColor;
	uint8* bitsMask;
	uint8* colorTable;
	uint8* bitsColor;
};
typedef struct _ICON_INFO ICON_INFO;

struct _CACHED_ICON_INFO
{
	uint32 cacheEntry;
	uint32 cacheId;
};
typedef struct _CACHED_ICON_INFO CACHED_ICON_INFO;

struct _NOTIFY_ICON_INFOTIP
{
	uint32 timeout;
	uint32 flags;
	UNICODE_STRING text;
	UNICODE_STRING title;
};
typedef struct _NOTIFY_ICON_INFOTIP NOTIFY_ICON_INFOTIP;

struct _WINDOW_STATE_ORDER
{
	uint32 ownerWindowId;
	uint32 style;
	uint32 extendedStyle;
	uint32 showState;
	UNICODE_STRING titleInfo;
	uint32 clientOffsetX;
	uint32 clientOffsetY;
	uint32 clientAreaWidth;
	uint32 clientAreaHeight;
	uint32 RPContent;
	uint32 rootParentHandle;
	uint32 windowOffsetX;
	uint32 windowOffsetY;
	uint32 windowClientDeltaX;
	uint32 windowClientDeltaY;
	uint32 windowWidth;
	uint32 windowHeight;
	uint32 numWindowRects;
	RECTANGLE_16* windowRects;
	uint32 visibleOffsetX;
	uint32 visibleOffsetY;
	uint32 numVisibilityRects;
	RECTANGLE_16* visibilityRects;
};
typedef struct _WINDOW_STATE_ORDER WINDOW_STATE_ORDER;

struct _WINDOW_ICON_ORDER
{
	ICON_INFO* iconInfo;
};
typedef struct _WINDOW_ICON_ORDER WINDOW_ICON_ORDER;

struct _WINDOW_CACHED_ICON_ORDER
{
	CACHED_ICON_INFO cachedIcon;
};
typedef struct _WINDOW_CACHED_ICON_ORDER WINDOW_CACHED_ICON_ORDER;

struct _NOTIFY_ICON_STATE_ORDER
{
	uint32 version;
	UNICODE_STRING toolTip;
	NOTIFY_ICON_INFOTIP infoTip;
	uint32 state;
	ICON_INFO icon;
	CACHED_ICON_INFO cachedIcon;
};
typedef struct _NOTIFY_ICON_STATE_ORDER NOTIFY_ICON_STATE_ORDER;

struct _MONITORED_DESKTOP_ORDER
{
	uint32 activeWindowId;
	uint32 numWindowIds;
	uint32* windowIds;
};
typedef struct _MONITORED_DESKTOP_ORDER MONITORED_DESKTOP_ORDER;

typedef void (*pWindowCreate)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);
typedef void (*pWindowUpdate)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);
typedef void (*pWindowIcon)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* window_icon);
typedef void (*pWindowCachedIcon)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* window_cached_icon);
typedef void (*pWindowDelete)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo);
typedef void (*pNotifyIconCreate)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state);
typedef void (*pNotifyIconUpdate)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state);
typedef void (*pNotifyIconDelete)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo);
typedef void (*pMonitoredDesktop)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitored_desktop);
typedef void (*pNonMonitoredDesktop)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo);

struct rdp_window_update
{
	rdpContext* context; /* 0 */
	uint32 paddingA[16 - 1]; /* 1 */

	pWindowCreate WindowCreate; /* 16 */
	pWindowUpdate WindowUpdate; /* 17 */
	pWindowIcon WindowIcon; /* 18 */
	pWindowCachedIcon WindowCachedIcon; /* 19 */
	pWindowDelete WindowDelete; /* 20 */
	pNotifyIconCreate NotifyIconCreate; /* 21 */
	pNotifyIconUpdate NotifyIconUpdate; /* 22 */
	pNotifyIconDelete NotifyIconDelete; /* 23 */
	pMonitoredDesktop MonitoredDesktop; /* 24 */
	pNonMonitoredDesktop NonMonitoredDesktop; /* 25 */
	uint32 paddingB[32 - 26]; /* 26 */

	/* internal */

	WINDOW_ORDER_INFO orderInfo;
	WINDOW_STATE_ORDER window_state;
	WINDOW_ICON_ORDER window_icon;
	WINDOW_CACHED_ICON_ORDER window_cached_icon;
	NOTIFY_ICON_STATE_ORDER notify_icon_state;
	MONITORED_DESKTOP_ORDER monitored_desktop;
};
typedef struct rdp_window_update rdpWindowUpdate;

#endif /* __UPDATE_WINDOW_H */
