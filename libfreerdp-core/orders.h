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

typedef struct rdp_orders rdpOrders;

#include "rdp.h"
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

typedef struct
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
} DSTBLT_ORDER;

typedef struct
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	uint32 backColor;
	uint32 foreColor;
	uint8 brushOrgX;
	uint8 brushOrgY;
	uint8 brushStyle;
	uint8 brushHatch;
	uint8 brushExtra[7];
} PATBLT_ORDER;

typedef struct
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
} SCRBLT_ORDER;

typedef struct
{
	sint16 srcLeft;
	sint16 srcTop;
	sint16 srcRight;
	sint16 srcBottom;
	uint16 bitmapId;
} DRAW_NINE_GRID_ORDER;

typedef struct
{
	sint16 srcLeft;
	sint16 srcTop;
	sint16 srcRight;
	sint16 srcBottom;
	uint16 bitmapId;
	uint8 nDeltaEntries;
	uint8* codeDeltaList;
} MULTI_DRAW_NINE_GRID_ORDER;

typedef struct
{
	uint16 backMode;
	sint16 nXStart;
	sint16 nYStart;
	sint16 nXEnd;
	sint16 nYEnd;
	uint32 backColor;
	uint8 bRop2;
	uint8 penStyle;
	uint8 penWidth;
	uint32 penColor;
} LINE_TO_ORDER;

typedef struct
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint32 color;
} OPAQUE_RECT_ORDER;

typedef struct
{
	uint32 savedBitmapPosition;
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nRightRect;
	sint16 nBottomRect;
	uint8 operation;
} SAVE_BITMAP_ORDER;

typedef struct
{
	uint16 cacheId;
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
	uint16 cacheIndex;
} MEMBLT_ORDER;

typedef struct
{
	uint16 cacheId;
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
	uint32 backColor;
	uint32 foreColor;
	uint8 brushOrgX;
	uint8 brushOrgY;
	uint8 brushStyle;
	uint8 brushHatch;
	uint8 brushExtra[7];
	uint16 cacheIndex;
} MEM3BLT_ORDER;

typedef struct
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	uint8 nDeltaEntries;
	uint8* codeDeltaList;
} MULTI_DSTBLT_ORDER;

typedef struct
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	uint32 backColor;
	uint32 foreColor;
	uint8 brushOrgX;
	uint8 brushOrgY;
	uint8 brushStyle;
	uint8 brushHatch;
	uint8 brushExtra[7];
	uint8 nDeltaEntries;
	uint8* codeDeltaList;
} MULTI_PATBLT_ORDER;

typedef struct
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
	uint8 nDeltaEntries;
	uint8* codeDeltaList;
} MULTI_SCRBLT_ORDER;

typedef struct
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint32 color;
	uint8 nDeltaEntries;
	uint8* codeDeltaList;
} MULTI_OPAQUE_RECT_ORDER;

typedef struct
{
	uint8 cacheId;
	uint16 fDrawing;
	uint32 backColor;
	uint32 foreColor;
	sint16 bkLeft;
	sint16 bkTop;
	sint16 bkRight;
	sint16 bkBottom;
	sint16 opLeft;
	sint16 opTop;
	sint16 opRight;
	sint16 opBottom;
	uint16 x;
	uint16 y;
	uint8* data;
} FAST_INDEX_ORDER;

typedef struct
{
	sint16 xStart;
	sint16 yStart;
	uint8 bRop2;
	uint8 fillMode;
	uint32 brushColor;
	uint8 nDeltaEntries;
	uint8* codeDeltaList;
} POLYGON_SC_ORDER;

typedef struct
{
	sint16 xStart;
	sint16 yStart;
	uint8 bRop2;
	uint8 fillMode;
	uint32 backColor;
	uint32 foreColor;
	uint8 brushOrgX;
	uint8 brushOrgY;
	uint8 brushStyle;
	uint8 brushHatch;
	uint8 brushExtra[7];
	uint8 nDeltaEntries;
	uint8* codeDeltaList;
} POLYGON_CB_ORDER;

typedef struct
{
	sint16 xStart;
	sint16 yStart;
	uint8 bRop2;
	uint32 penColor;
	uint8 nDeltaEntries;
	uint8* codeDeltaList;
} POLYLINE_ORDER;

typedef struct
{
	uint8 cacheId;
	uint16 fDrawing;
	uint32 backColor;
	uint32 foreColor;
	sint16 bkLeft;
	sint16 bkTop;
	sint16 bkRight;
	sint16 bkBottom;
	sint16 opLeft;
	sint16 opTop;
	sint16 opRight;
	sint16 opBottom;
	uint16 x;
	uint16 y;
	uint8* data;
} FAST_GLYPH_ORDER;

typedef struct
{
	sint16 leftRect;
	sint16 topRect;
	sint16 rightRect;
	sint16 bottomRect;
	uint8 bRop2;
	uint8 fillMode;
	uint32 color;
} ELLIPSE_SC_ORDER;

typedef struct
{
	sint16 leftRect;
	sint16 topRect;
	sint16 rightRect;
	sint16 bottomRect;
	uint8 bRop2;
	uint8 fillMode;
	uint32 backColor;
	uint32 foreColor;
	uint8 brushOrgX;
	uint8 brushOrgY;
	uint8 brushStyle;
	uint8 brushHatch;
	uint8 brushExtra[7];
} ELLIPSE_CB_ORDER;

typedef struct
{
	uint8 cacheId;
	uint8 flAccel;
	uint8 ulCharInc;
	uint8 fOpRedundant;
	uint32 backColor;
	uint32 foreColor;
	sint16 bkLeft;
	sint16 bkTop;
	sint16 bkRight;
	sint16 bkBottom;
	sint16 opLeft;
	sint16 opTop;
	sint16 opRight;
	sint16 opBottom;
	uint8 brushOrgX;
	uint8 brushOrgY;
	uint8 brushStyle;
	uint8 brushHatch;
	uint8 brushExtra[7];
	sint16 x;
	sint16 y;
	uint8* data;
} GLYPH_INDEX_ORDER;

struct rdp_orders
{
	ORDER_INFO order_info;
	DSTBLT_ORDER dstblt;
	PATBLT_ORDER patblt;
	SCRBLT_ORDER scrblt;
	DRAW_NINE_GRID_ORDER draw_nine_grid;
	MULTI_DRAW_NINE_GRID_ORDER multi_draw_nine_grid;
	LINE_TO_ORDER line_to;
	OPAQUE_RECT_ORDER opaque_rect;
	SAVE_BITMAP_ORDER save_bitmap;
	MEMBLT_ORDER memblt;
	MEM3BLT_ORDER mem3blt;
	MULTI_DSTBLT_ORDER multi_dstblt;
	MULTI_PATBLT_ORDER multi_patblt;
	MULTI_SCRBLT_ORDER multi_scrblt;
	MULTI_OPAQUE_RECT_ORDER multi_opaque_rect;
	FAST_INDEX_ORDER fast_index;
	POLYGON_SC_ORDER polygon_sc;
	POLYGON_CB_ORDER polygon_cb;
	POLYLINE_ORDER polyline;
	FAST_GLYPH_ORDER fast_glyph;
	ELLIPSE_SC_ORDER ellipse_sc;
	ELLIPSE_CB_ORDER ellipse_cb;
	GLYPH_INDEX_ORDER glyph_index;
};

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

/* Field Presence Flags */
#define ORDER_FIELD_01				0x000001
#define ORDER_FIELD_02				0x000002
#define ORDER_FIELD_03				0x000004
#define ORDER_FIELD_04				0x000008
#define ORDER_FIELD_05				0x000010
#define ORDER_FIELD_06				0x000020
#define ORDER_FIELD_07				0x000040
#define ORDER_FIELD_08				0x000080
#define ORDER_FIELD_09				0x000100
#define ORDER_FIELD_10				0x000200
#define ORDER_FIELD_11				0x000400
#define ORDER_FIELD_12				0x000800
#define ORDER_FIELD_13				0x001000
#define ORDER_FIELD_14				0x002000
#define ORDER_FIELD_15				0x004000
#define ORDER_FIELD_16				0x008000
#define ORDER_FIELD_17				0x010000
#define ORDER_FIELD_18				0x020000
#define ORDER_FIELD_19				0x040000
#define ORDER_FIELD_20				0x080000
#define ORDER_FIELD_21				0x100000
#define ORDER_FIELD_22				0x200000
#define ORDER_FIELD_23				0x400000

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

rdpOrders* orders_new();
void orders_free(rdpOrders* orders);

void rdp_recv_order(rdpRdp* rdp, STREAM* s);

#endif /* __ORDERS_H */
