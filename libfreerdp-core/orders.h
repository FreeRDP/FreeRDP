/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Drawing Orders
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

#ifndef __ORDERS_H
#define __ORDERS_H

#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

typedef struct
{
	uint8 orderType;
	uint32 fieldFlags;
	uint16 boundLeft;
	uint16 boundTop;
	uint16 boundRight;
	uint16 boundBottom;
	sint8 deltaBoundLeft;
	sint8 deltaBoundTop;
	sint8 deltaBoundRight;
	sint8 deltaBoundBottom;
	boolean deltaCoordinates;
} ORDER_INFO;

#include "rdp.h"

/* Order Control Flags */
#define ORDER_STANDARD				0x01
#define ORDER_SECONDARY				0x02
#define ORDER_BOUNDS				0x04
#define ORDER_TYPE_CHANGE			0x08
#define ORDER_DELTA_COORDINATES			0x10
#define ORDER_ZERO_BOUNDS_DELTAS		0x20
#define ORDER_ZERO_FIELD_BYTE_BIT0		0x40
#define ORDER_ZERO_FIELD_BYTE_BIT1		0x80

/* Bound Field Flags */
#define BOUND_LEFT				0x01
#define BOUND_TOP				0x02
#define BOUND_RIGHT				0x04
#define BOUND_BOTTOM				0x08
#define BOUND_DELTA_LEFT			0x10
#define BOUND_DELTA_TOP				0x20
#define BOUND_DELTA_RIGHT			0x40
#define BOUND_DELTA_BOTTOM			0x80

/* Order Classes */
#define ORDER_PRIMARY_CLASS			0x01
#define ORDER_SECONDARY_CLASS 			0x03
#define ORDER_ALTSEC_CLASS			0x02
#define ORDER_CLASS_MASK			0x03

/* Primary Drawing Orders */
#define ORDER_TYPE_DSTBLT			0x00
#define ORDER_TYPE_PATBLT			0x01
#define ORDER_TYPE_SCRBLT			0x02
#define ORDER_TYPE_DRAW_NINE_GRID		0x07
#define ORDER_TYPE_MULTI_DRAW_NINE_GRID		0x08
#define ORDER_TYPE_LINE_TO			0x09
#define ORDER_TYPE_OPAQUE_RECT			0x0A
#define ORDER_TYPE_SAVE_BITMAP			0x0B
#define ORDER_TYPE_MEMBLT			0x0D
#define ORDER_TYPE_MEM3BLT			0x0E
#define ORDER_TYPE_MULTI_DSTBLT			0x0F
#define ORDER_TYPE_MULTI_PATBLT			0x10
#define ORDER_TYPE_MULTI_SCRBLT			0x11
#define ORDER_TYPE_MULTI_OPAQUE_RECT		0x12
#define ORDER_TYPE_FAST_INDEX			0x13
#define ORDER_TYPE_POLYGON_SC			0x14
#define ORDER_TYPE_POLYGON_CB			0x15
#define ORDER_TYPE_POLYLINE			0x16
#define ORDER_TYPE_FAST_GLYPH			0x18
#define ORDER_TYPE_ELLIPSE_SC			0x19
#define ORDER_TYPE_ELLIPSE_CB			0x1A
#define ORDER_TYPE_GLYPH_INDEX			0x1B

/* Primary Drawing Orders Fields */
#define DSTBLT_ORDER_FIELDS			5
#define PATBLT_ORDER_FIELDS			12
#define SCRBLT_ORDER_FIELDS			7
#define DRAW_NINE_GRID_ORDER_FIELDS		5
#define MULTI_DRAW_NINE_GRID_ORDER_FIELDS	7
#define LINE_TO_ORDER_FIELDS			10
#define OPAQUE_RECT_ORDER_FIELDS		7
#define SAVE_BITMAP_ORDER_FIELDS		6
#define MEMBLT_ORDER_FIELDS			9
#define MEM3BLT_ORDER_FIELDS			16
#define MULTI_DSTBLT_ORDER_FIELDS		7
#define MULTI_PATBLT_ORDER_FIELDS		14
#define MULTI_SCRBLT_ORDER_FIELDS		9
#define MULTI_OPAQUE_RECT_ORDER_FIELDS		9
#define FAST_INDEX_ORDER_FIELDS			15
#define POLYGON_SC_ORDER_FIELDS			7
#define POLYGON_CB_ORDER_FIELDS			13
#define POLYLINE_ORDER_FIELDS			7
#define FAST_GLYPH_ORDER_FIELDS			15
#define ELLIPSE_SC_ORDER_FIELDS			7
#define ELLIPSE_CB_ORDER_FIELDS			13
#define GLYPH_INDEX_ORDER_FIELDS		22

/* Primary Drawing Orders Field Bytes */
#define DSTBLT_ORDER_FIELD_BYTES		1
#define PATBLT_ORDER_FIELD_BYTES		2
#define SCRBLT_ORDER_FIELD_BYTES		1
#define DRAW_NINE_GRID_ORDER_FIELD_BYTES	1
#define MULTI_DRAW_NINE_GRID_ORDER_FIELD_BYTES	1
#define LINE_TO_ORDER_FIELD_BYTES		2
#define OPAQUE_RECT_ORDER_FIELD_BYTES		1
#define SAVE_BITMAP_ORDER_FIELD_BYTES		1
#define MEMBLT_ORDER_FIELD_BYTES		2
#define MEM3BLT_ORDER_FIELD_BYTES		3
#define MULTI_DSTBLT_ORDER_FIELD_BYTES		1
#define MULTI_PATBLT_ORDER_FIELD_BYTES		2
#define MULTI_SCRBLT_ORDER_FIELD_BYTES		2
#define MULTI_OPAQUE_RECT_ORDER_FIELD_BYTES	2
#define FAST_INDEX_ORDER_FIELD_BYTES		2
#define POLYGON_SC_ORDER_FIELD_BYTES		1
#define POLYGON_CB_ORDER_FIELD_BYTES		2
#define POLYLINE_ORDER_FIELD_BYTES		1
#define FAST_GLYPH_ORDER_FIELD_BYTES		2
#define ELLIPSE_SC_ORDER_FIELD_BYTES		1
#define ELLIPSE_CB_ORDER_FIELD_BYTES		2
#define GLYPH_INDEX_ORDER_FIELD_BYTES		3

/* Secondary Drawing Orders */
#define ORDER_TYPE_BITMAP_UNCOMPRESSED		0x00
#define ORDER_TYPE_CACHE_COLOR_TABLE		0x01
#define ORDER_TYPE_CACHE_BITMAP_COMPRESSED	0x02
#define ORDER_TYPE_CACHE_GLYPH			0x03
#define ORDER_TYPE_BITMAP_UNCOMPRESSED_V2	0x04
#define ORDER_TYPE_BITMAP_COMPRESSED_V2		0x05
#define ORDER_TYPE_CACHE_BRUSH			0x07
#define ORDER_TYPE_BITMAP_COMPRESSED_V3		0x08

/* Alternate Secondary Drawing Orders */
#define ORDER_TYPE_SWITCH_SURFACE		0x00
#define ORDER_TYPE_CREATE_OFFSCR_BITMAP		0x01
#define ORDER_TYPE_STREAM_BITMAP_FIRST		0x02
#define ORDER_TYPE_STREAM_BITMAP_NEXT		0x03
#define ORDER_TYPE_CREATE_NINE_GRID_BITMAP	0x04
#define ORDER_TYPE_GDIPLUS_FIRST		0x05
#define ORDER_TYPE_GDIPLUS_NEXT			0x06
#define ORDER_TYPE_GDIPLUS_END			0x07
#define ORDER_TYPE_GDIPLUS_CACHE_FIRST		0x08
#define ORDER_TYPE_GDIPLUS_CACHE_NEXT		0x09
#define ORDER_TYPE_GDIPLUS_CACHE_END		0x0A
#define ORDER_TYPE_WINDOW			0x0B
#define ORDER_TYPE_COMPDESK_FIRST		0x0C
#define ORDER_TYPE_FRAME_MARKER			0x0D

void rdp_recv_order(rdpRdp* rdp, STREAM* s);

#endif /* __ORDERS_H */
