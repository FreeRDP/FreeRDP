/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Windowing Alternate Secondary Orders
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
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

#ifndef __WINDOW_H
#define __WINDOW_H

#include "update.h"

#include <freerdp/utils/stream.h>

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

struct _UNICODE_STRING
{
	uint16 cbString;
	uint8* string;
};
typedef struct _UNICODE_STRING UNICODE_STRING;

struct _RECTANGLE_16
{
	uint16 left;
	uint16 top;
	uint16 right;
	uint16 bottom;
};
typedef struct _RECTANGLE_16 RECTANGLE_16;

struct _ICON_INFO
{
	uint16 cacheEntry;
	uint8 cacheId;
	uint8 bpp;
	uint16 width;
	uint16 height;
	uint16 cbColorTable;
	uint16 cbBitsMask;
	uint16 cbBitsColor;
	uint8* bitsMask;
	uint8* colorTable;
	uint8* bitsColor;
};
typedef struct _ICON_INFO ICON_INFO;

struct _CACHED_ICON_INFO
{
	uint16 cacheEntry;
	uint8 cacheId;
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

struct _WINDOW_ORDER_INFO
{
	uint32 windowId;
	uint32 fieldFlags;
	uint32 notifyIconId;
};
typedef struct _WINDOW_ORDER_INFO WINDOW_ORDER_INFO;

struct _WINDOW_STATE_ORDER
{
	uint32 ownerWindowId;
	uint32 style;
	uint32 extendedStyle;
	uint8 showState;
	UNICODE_STRING titleInfo;
	uint32 clientOffsetX;
	uint32 clientOffsetY;
	uint32 clientAreaWidth;
	uint32 clientAreaHeight;
	uint8 RPContent;
	uint32 rootParentHandle;
	uint32 windowOffsetX;
	uint32 windowOffsetY;
	uint32 windowClientDeltaX;
	uint32 windowClientDeltaY;
	uint32 windowWidth;
	uint32 windowHeight;
	uint16 numWindowRects;
	RECTANGLE_16* windowRects;
	uint32 visibleOffsetX;
	uint32 visibleOffsetY;
	uint16 numVisibilityRects;
	RECTANGLE_16* visibilityRects;
};
typedef struct _WINDOW_STATE_ORDER WINDOW_STATE_ORDER;

struct _WINDOW_ICON_ORDER
{
	ICON_INFO iconInfo;
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
	uint8 numWindowIds;
	uint32* windowIds;
};
typedef struct _MONITORED_DESKTOP_ORDER MONITORED_DESKTOP_ORDER;

void update_recv_altsec_window_order(rdpUpdate* update, STREAM* s);

#endif /* __WINDOW_H */
