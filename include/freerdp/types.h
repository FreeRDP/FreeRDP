/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __RDP_TYPES_H
#define __RDP_TYPES_H

/* Base Types */

typedef unsigned char uint8;
typedef signed char sint8;
typedef unsigned short uint16;
typedef signed short sint16;
typedef unsigned int uint32;
typedef signed int sint32;
#ifdef _WIN32
typedef unsigned __int64 uint64;
typedef signed __int64 sint64;
#else
typedef unsigned long long uint64;
typedef signed long long sint64;
#endif

#ifndef True
#define True  (1)
#endif

#ifndef False
#define False (0)
#endif

typedef int boolean;

#ifndef MIN
#define MIN(x,y)	(((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y)	(((x) > (y)) ? (x) : (y))
#endif

#include <freerdp/settings.h>

typedef void *FRDP_HBITMAP;
typedef void *FRDP_HGLYPH;
typedef void *FRDP_HPALETTE;
typedef void *FRDP_HCURSOR;

typedef struct _FRDP_POINT
{
	sint16 x, y;
}
FRDP_POINT;

typedef struct _FRDP_PALETTEENTRY
{
	uint8 red;
	uint8 green;
	uint8 blue;
}
FRDP_PALETTEENTRY;

typedef struct _FRDP_PALETTE
{
	uint16 count;
	FRDP_PALETTEENTRY *entries;
}
FRDP_PALETTE;

typedef struct _FRDP_PEN
{
	uint8 style;
	uint8 width;
	uint32 color;
}
FRDP_PEN;

/* this is what is in the brush cache */
typedef struct _FRDP_BRUSHDATA
{
	uint32 color_code;
	uint32 data_size;
	uint8 *data;
}
FRDP_BRUSHDATA;

typedef struct _FRDP_BRUSH
{
	uint8 xorigin;
	uint8 yorigin;
	uint8 style;
	uint8 pattern[8];
	FRDP_BRUSHDATA *bd;
}
FRDP_BRUSH;

typedef struct _FRDP_PLUGIN_DATA
{
	uint16 size;
	void * data[4];
}
FRDP_PLUGIN_DATA;

typedef struct _FRDP_RECT
{
	sint16 x;
	sint16 y;
	sint16 width;
	sint16 height;
}
FRDP_RECT;

typedef struct _FRDP_EVENT FRDP_EVENT;

typedef void (*FRDP_EVENT_CALLBACK) (FRDP_EVENT * event);

struct _FRDP_EVENT
{
	uint16 event_type;
	FRDP_EVENT_CALLBACK event_callback;
	void * user_data;
};

struct _FRDP_VIDEO_FRAME_EVENT
{
	FRDP_EVENT event;
	uint8 * frame_data;
	uint32 frame_size;
	uint32 frame_pixfmt;
	sint16 frame_width;
	sint16 frame_height;
	sint16 x;
	sint16 y;
	sint16 width;
	sint16 height;
	uint16 num_visible_rects;
	FRDP_RECT * visible_rects;
};
typedef struct _FRDP_VIDEO_FRAME_EVENT FRDP_VIDEO_FRAME_EVENT;

struct _FRDP_REDRAW_EVENT
{
	FRDP_EVENT event;
	sint16 x;
	sint16 y;
	sint16 width;
	sint16 height;
};
typedef struct _FRDP_REDRAW_EVENT FRDP_REDRAW_EVENT;

typedef struct rdp_inst rdpInst;

#endif /* __RDP_TYPES_H */
