/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_UPDATE_WINDOW_H
#define FREERDP_UPDATE_WINDOW_H

#include <freerdp/types.h>

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

/**
 * This is a custom extended window style used by XRDP
 * instructing the client to use local window decorations
 */

#define WS_EX_DECORATIONS		0x40000000

struct _WINDOW_ORDER_INFO
{
	UINT32 windowId;
	UINT32 fieldFlags;
	UINT32 notifyIconId;
};
typedef struct _WINDOW_ORDER_INFO WINDOW_ORDER_INFO;

struct _ICON_INFO
{
	UINT32 cacheEntry;
	UINT32 cacheId;
	UINT32 bpp;
	UINT32 width;
	UINT32 height;
	UINT32 cbColorTable;
	UINT32 cbBitsMask;
	UINT32 cbBitsColor;
	BYTE* bitsMask;
	BYTE* colorTable;
	BYTE* bitsColor;
};
typedef struct _ICON_INFO ICON_INFO;

struct _CACHED_ICON_INFO
{
	UINT32 cacheEntry;
	UINT32 cacheId;
};
typedef struct _CACHED_ICON_INFO CACHED_ICON_INFO;

struct _NOTIFY_ICON_INFOTIP
{
	UINT32 timeout;
	UINT32 flags;
	RAIL_UNICODE_STRING text;
	RAIL_UNICODE_STRING title;
};
typedef struct _NOTIFY_ICON_INFOTIP NOTIFY_ICON_INFOTIP;

struct _WINDOW_STATE_ORDER
{
	UINT32 ownerWindowId;
	UINT32 style;
	UINT32 extendedStyle;
	UINT32 showState;
	RAIL_UNICODE_STRING titleInfo;
	INT32 clientOffsetX;
	INT32 clientOffsetY;
	UINT32 clientAreaWidth;
	UINT32 clientAreaHeight;
	UINT32 RPContent;
	UINT32 rootParentHandle;
	INT32 windowOffsetX;
	INT32 windowOffsetY;
	INT32 windowClientDeltaX;
	INT32 windowClientDeltaY;
	UINT32 windowWidth;
	UINT32 windowHeight;
	UINT32 numWindowRects;
	RECTANGLE_16* windowRects;
	INT32 visibleOffsetX;
	INT32 visibleOffsetY;
	UINT32 numVisibilityRects;
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
	UINT32 version;
	RAIL_UNICODE_STRING toolTip;
	NOTIFY_ICON_INFOTIP infoTip;
	UINT32 state;
	ICON_INFO icon;
	CACHED_ICON_INFO cachedIcon;
};
typedef struct _NOTIFY_ICON_STATE_ORDER NOTIFY_ICON_STATE_ORDER;

struct _MONITORED_DESKTOP_ORDER
{
	UINT32 activeWindowId;
	UINT32 numWindowIds;
	UINT32* windowIds;
};
typedef struct _MONITORED_DESKTOP_ORDER MONITORED_DESKTOP_ORDER;

typedef BOOL (*pWindowCreate)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);
typedef BOOL (*pWindowUpdate)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);
typedef BOOL (*pWindowIcon)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* window_icon);
typedef BOOL (*pWindowCachedIcon)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* window_cached_icon);
typedef BOOL (*pWindowDelete)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo);
typedef BOOL (*pNotifyIconCreate)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state);
typedef BOOL (*pNotifyIconUpdate)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state);
typedef BOOL (*pNotifyIconDelete)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo);
typedef BOOL (*pMonitoredDesktop)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitored_desktop);
typedef BOOL (*pNonMonitoredDesktop)(rdpContext* context, WINDOW_ORDER_INFO* orderInfo);

struct rdp_window_update
{
	rdpContext* context; /* 0 */
	UINT32 paddingA[16 - 1]; /* 1 */

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
	UINT32 paddingB[32 - 26]; /* 26 */

	/* internal */

	WINDOW_ORDER_INFO orderInfo;
	WINDOW_STATE_ORDER window_state;
	WINDOW_ICON_ORDER window_icon;
	WINDOW_CACHED_ICON_ORDER window_cached_icon;
	NOTIFY_ICON_STATE_ORDER notify_icon_state;
	MONITORED_DESKTOP_ORDER monitored_desktop;
};
typedef struct rdp_window_update rdpWindowUpdate;

#endif /* FREERDP_UPDATE_WINDOW_H */
