/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Type Definitions
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifndef FREERDP_TYPES_H
#define FREERDP_TYPES_H

#include <winpr/wtypes.h>

#ifndef MIN
#define MIN(x,y)	(((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y)	(((x) > (y)) ? (x) : (y))
#endif

#include <freerdp/settings.h>

struct _RDP_PLUGIN_DATA
{
	DWORD size;
	void* data[4];
};
typedef struct _RDP_PLUGIN_DATA RDP_PLUGIN_DATA;

struct _RDP_RECT
{
	INT16 x;
	INT16 y;
	INT16 width;
	INT16 height;
};
typedef struct _RDP_RECT RDP_RECT;

struct _RECTANGLE_16
{
	UINT16 left;
	UINT16 top;
	UINT16 right;
	UINT16 bottom;
};
typedef struct _RECTANGLE_16 RECTANGLE_16;

/* Plugin events */
typedef struct _RDP_EVENT RDP_EVENT;

typedef void (*RDP_EVENT_CALLBACK) (RDP_EVENT* event);

struct _RDP_EVENT
{
	UINT16 event_class;
	UINT16 event_type;
	RDP_EVENT_CALLBACK on_event_free_callback;
	void* user_data;
};

enum RDP_EVENT_CLASS
{
	RDP_EVENT_CLASS_DEBUG = 0,
	RDP_EVENT_CLASS_CLIPRDR,
	RDP_EVENT_CLASS_TSMF,
	RDP_EVENT_CLASS_RAIL
};

#endif /* __RDP_TYPES_H */
