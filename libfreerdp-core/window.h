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

void update_recv_altsec_window_order(rdpUpdate* update, STREAM* s);

#ifdef WITH_DEBUG_WND
#define DEBUG_WND(fmt, ...) DEBUG_CLASS(WND, fmt, ## __VA_ARGS__)
#else
#define DEBUG_WND(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __WINDOW_H */
