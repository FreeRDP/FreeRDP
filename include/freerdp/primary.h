/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Primary Drawing Orders Interface API
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

#ifndef __UPDATE_PRIMARY_H
#define __UPDATE_PRIMARY_H

#include <freerdp/types.h>

struct rdp_bounds
{
	sint32 left;
	sint32 top;
	sint32 right;
	sint32 bottom;
};
typedef struct rdp_bounds rdpBounds;

struct rdp_brush
{
	uint32 x;
	uint32 y;
	uint32 bpp;
	uint32 style;
	uint32 hatch;
	uint32 index;
	uint8* data;
	uint8 p8x8[8];
};
typedef struct rdp_brush rdpBrush;

struct _ORDER_INFO
{
	uint32 orderType;
	uint32 fieldFlags;
	rdpBounds bounds;
	sint32 deltaBoundLeft;
	sint32 deltaBoundTop;
	sint32 deltaBoundRight;
	sint32 deltaBoundBottom;
	boolean deltaCoordinates;
};
typedef struct _ORDER_INFO ORDER_INFO;

struct _DSTBLT_ORDER
{
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 bRop;
};
typedef struct _DSTBLT_ORDER DSTBLT_ORDER;

struct _PATBLT_ORDER
{
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 bRop;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
};
typedef struct _PATBLT_ORDER PATBLT_ORDER;

struct _SCRBLT_ORDER
{
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 bRop;
	sint32 nXSrc;
	sint32 nYSrc;
};
typedef struct _SCRBLT_ORDER SCRBLT_ORDER;

struct _OPAQUE_RECT_ORDER
{
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 color;
};
typedef struct _OPAQUE_RECT_ORDER OPAQUE_RECT_ORDER;

struct _DRAW_NINE_GRID_ORDER
{
	sint32 srcLeft;
	sint32 srcTop;
	sint32 srcRight;
	sint32 srcBottom;
	uint32 bitmapId;
};
typedef struct _DRAW_NINE_GRID_ORDER DRAW_NINE_GRID_ORDER;

struct _DELTA_RECT
{
	sint32 left;
	sint32 top;
	sint32 width;
	sint32 height;
};
typedef struct _DELTA_RECT DELTA_RECT;

struct _MULTI_DSTBLT_ORDER
{
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 bRop;
	uint32 numRectangles;
	uint32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_DSTBLT_ORDER MULTI_DSTBLT_ORDER;

struct _MULTI_PATBLT_ORDER
{
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 bRop;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
	uint32 numRectangles;
	uint32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_PATBLT_ORDER MULTI_PATBLT_ORDER;

struct _MULTI_SCRBLT_ORDER
{
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 bRop;
	sint32 nXSrc;
	sint32 nYSrc;
	uint32 numRectangles;
	uint32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_SCRBLT_ORDER MULTI_SCRBLT_ORDER;

struct _MULTI_OPAQUE_RECT_ORDER
{
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 color;
	uint32 numRectangles;
	uint32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_OPAQUE_RECT_ORDER MULTI_OPAQUE_RECT_ORDER;

struct _MULTI_DRAW_NINE_GRID_ORDER
{
	sint32 srcLeft;
	sint32 srcTop;
	sint32 srcRight;
	sint32 srcBottom;
	uint32 bitmapId;
	uint32 nDeltaEntries;
	uint32 cbData;
	uint8* codeDeltaList;
};
typedef struct _MULTI_DRAW_NINE_GRID_ORDER MULTI_DRAW_NINE_GRID_ORDER;

struct _LINE_TO_ORDER
{
	uint32 backMode;
	sint32 nXStart;
	sint32 nYStart;
	sint32 nXEnd;
	sint32 nYEnd;
	uint32 backColor;
	uint32 bRop2;
	uint32 penStyle;
	uint32 penWidth;
	uint32 penColor;
};
typedef struct _LINE_TO_ORDER LINE_TO_ORDER;

struct _DELTA_POINT
{
	sint32 x;
	sint32 y;
};
typedef struct _DELTA_POINT DELTA_POINT;

struct _POLYLINE_ORDER
{
	sint32 xStart;
	sint32 yStart;
	uint32 bRop2;
	uint32 penColor;
	uint32 numPoints;
	uint32 cbData;
	DELTA_POINT* points;
};
typedef struct _POLYLINE_ORDER POLYLINE_ORDER;

struct _MEMBLT_ORDER
{
	uint32 cacheId;
	uint32 colorIndex;
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 bRop;
	sint32 nXSrc;
	sint32 nYSrc;
	uint32 cacheIndex;
	rdpBitmap* bitmap;
};
typedef struct _MEMBLT_ORDER MEMBLT_ORDER;

struct _MEM3BLT_ORDER
{
	uint32 cacheId;
	uint32 colorIndex;
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nWidth;
	sint32 nHeight;
	uint32 bRop;
	sint32 nXSrc;
	sint32 nYSrc;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
	uint32 cacheIndex;
	rdpBitmap* bitmap;
};
typedef struct _MEM3BLT_ORDER MEM3BLT_ORDER;

struct _SAVE_BITMAP_ORDER
{
	uint32 savedBitmapPosition;
	sint32 nLeftRect;
	sint32 nTopRect;
	sint32 nRightRect;
	sint32 nBottomRect;
	uint32 operation;
};
typedef struct _SAVE_BITMAP_ORDER SAVE_BITMAP_ORDER;

struct _GLYPH_FRAGMENT_INDEX
{
	uint32 index;
	uint32 delta;
};
typedef struct _GLYPH_FRAGMENT_INDEX GLYPH_FRAGMENT_INDEX;

struct _GLYPH_FRAGMENT
{
	uint32 operation;
	uint32 index;
	uint32 size;
	uint32 nindices;
	GLYPH_FRAGMENT_INDEX* indices;
};
typedef struct _GLYPH_FRAGMENT GLYPH_FRAGMENT;

struct _GLYPH_INDEX_ORDER
{
	uint32 cacheId;
	uint32 flAccel;
	uint32 ulCharInc;
	uint32 fOpRedundant;
	uint32 backColor;
	uint32 foreColor;
	sint32 bkLeft;
	sint32 bkTop;
	sint32 bkRight;
	sint32 bkBottom;
	sint32 opLeft;
	sint32 opTop;
	sint32 opRight;
	sint32 opBottom;
	rdpBrush brush;
	sint32 x;
	sint32 y;
	uint32 cbData;
	uint8 data[256];
};
typedef struct _GLYPH_INDEX_ORDER GLYPH_INDEX_ORDER;

struct _FAST_INDEX_ORDER
{
	uint32 cacheId;
	uint32 flAccel;
	uint32 ulCharInc;
	uint32 backColor;
	uint32 foreColor;
	sint32 bkLeft;
	sint32 bkTop;
	sint32 bkRight;
	sint32 bkBottom;
	sint32 opLeft;
	sint32 opTop;
	sint32 opRight;
	sint32 opBottom;
	boolean opaqueRect;
	sint32 x;
	sint32 y;
	uint32 cbData;
	uint8 data[256];
};
typedef struct _FAST_INDEX_ORDER FAST_INDEX_ORDER;

struct _FAST_GLYPH_ORDER
{
	uint32 cacheId;
	uint32 flAccel;
	uint32 ulCharInc;
	uint32 backColor;
	uint32 foreColor;
	sint32 bkLeft;
	sint32 bkTop;
	sint32 bkRight;
	sint32 bkBottom;
	sint32 opLeft;
	sint32 opTop;
	sint32 opRight;
	sint32 opBottom;
	sint32 x;
	sint32 y;
	uint32 cbData;
	uint8 data[256];
	void* glyph_data;
};
typedef struct _FAST_GLYPH_ORDER FAST_GLYPH_ORDER;

struct _POLYGON_SC_ORDER
{
	sint32 xStart;
	sint32 yStart;
	uint32 bRop2;
	uint32 fillMode;
	uint32 brushColor;
	uint32 nDeltaEntries;
	uint32 cbData;
	uint8* codeDeltaList;
};
typedef struct _POLYGON_SC_ORDER POLYGON_SC_ORDER;

struct _POLYGON_CB_ORDER
{
	sint32 xStart;
	sint32 yStart;
	uint32 bRop2;
	uint32 fillMode;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
	uint32 nDeltaEntries;
	uint32 cbData;
	uint8* codeDeltaList;
};
typedef struct _POLYGON_CB_ORDER POLYGON_CB_ORDER;

struct _ELLIPSE_SC_ORDER
{
	sint32 leftRect;
	sint32 topRect;
	sint32 rightRect;
	sint32 bottomRect;
	uint32 bRop2;
	uint32 fillMode;
	uint32 color;
};
typedef struct _ELLIPSE_SC_ORDER ELLIPSE_SC_ORDER;

struct _ELLIPSE_CB_ORDER
{
	sint32 leftRect;
	sint32 topRect;
	sint32 rightRect;
	sint32 bottomRect;
	uint32 bRop2;
	uint32 fillMode;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
};
typedef struct _ELLIPSE_CB_ORDER ELLIPSE_CB_ORDER;

typedef void (*pDstBlt)(rdpContext* context, DSTBLT_ORDER* dstblt);
typedef void (*pPatBlt)(rdpContext* context, PATBLT_ORDER* patblt);
typedef void (*pScrBlt)(rdpContext* context, SCRBLT_ORDER* scrblt);
typedef void (*pOpaqueRect)(rdpContext* context, OPAQUE_RECT_ORDER* opaque_rect);
typedef void (*pDrawNineGrid)(rdpContext* context, DRAW_NINE_GRID_ORDER* draw_nine_grid);
typedef void (*pMultiDstBlt)(rdpContext* context, MULTI_DSTBLT_ORDER* multi_dstblt);
typedef void (*pMultiPatBlt)(rdpContext* context, MULTI_PATBLT_ORDER* multi_patblt);
typedef void (*pMultiScrBlt)(rdpContext* context, MULTI_SCRBLT_ORDER* multi_scrblt);
typedef void (*pMultiOpaqueRect)(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect);
typedef void (*pMultiDrawNineGrid)(rdpContext* context, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid);
typedef void (*pLineTo)(rdpContext* context, LINE_TO_ORDER* line_to);
typedef void (*pPolyline)(rdpContext* context, POLYLINE_ORDER* polyline);
typedef void (*pMemBlt)(rdpContext* context, MEMBLT_ORDER* memblt);
typedef void (*pMem3Blt)(rdpContext* context, MEM3BLT_ORDER* memblt);
typedef void (*pSaveBitmap)(rdpContext* context, SAVE_BITMAP_ORDER* save_bitmap);
typedef void (*pGlyphIndex)(rdpContext* context, GLYPH_INDEX_ORDER* glyph_index);
typedef void (*pFastIndex)(rdpContext* context, FAST_INDEX_ORDER* fast_index);
typedef void (*pFastGlyph)(rdpContext* context, FAST_GLYPH_ORDER* fast_glyph);
typedef void (*pPolygonSC)(rdpContext* context, POLYGON_SC_ORDER* polygon_sc);
typedef void (*pPolygonCB)(rdpContext* context, POLYGON_CB_ORDER* polygon_cb);
typedef void (*pEllipseSC)(rdpContext* context, ELLIPSE_SC_ORDER* ellipse_sc);
typedef void (*pEllipseCB)(rdpContext* context, ELLIPSE_CB_ORDER* ellipse_cb);

struct rdp_primary_update
{
	rdpContext* context; /* 0 */
	uint32 paddingA[16 - 1]; /* 1 */

	pDstBlt DstBlt; /* 16 */
	pPatBlt PatBlt; /* 17 */
	pScrBlt ScrBlt; /* 18 */
	pOpaqueRect OpaqueRect; /* 19 */
	pDrawNineGrid DrawNineGrid; /* 20 */
	pMultiDstBlt MultiDstBlt; /* 21 */
	pMultiPatBlt MultiPatBlt; /* 22 */
	pMultiScrBlt MultiScrBlt; /* 23 */
	pMultiOpaqueRect MultiOpaqueRect; /* 24 */
	pMultiDrawNineGrid MultiDrawNineGrid; /* 25 */
	pLineTo LineTo; /* 26 */
	pPolyline Polyline; /* 27 */
	pMemBlt MemBlt; /* 28 */
	pMem3Blt Mem3Blt; /* 29 */
	pSaveBitmap SaveBitmap; /* 30 */
	pGlyphIndex GlyphIndex; /* 31 */
	pFastIndex FastIndex; /* 32 */
	pFastGlyph FastGlyph; /* 33 */
	pPolygonSC PolygonSC; /* 34 */
	pPolygonCB PolygonCB; /* 35 */
	pEllipseSC EllipseSC; /* 36 */
	pEllipseCB EllipseCB; /* 37 */
	uint32 paddingB[48 - 38]; /* 38 */

	/* internal */

	ORDER_INFO order_info;
	DSTBLT_ORDER dstblt;
	PATBLT_ORDER patblt;
	SCRBLT_ORDER scrblt;
	OPAQUE_RECT_ORDER opaque_rect;
	DRAW_NINE_GRID_ORDER draw_nine_grid;
	MULTI_DSTBLT_ORDER multi_dstblt;
	MULTI_PATBLT_ORDER multi_patblt;
	MULTI_SCRBLT_ORDER multi_scrblt;
	MULTI_OPAQUE_RECT_ORDER multi_opaque_rect;
	MULTI_DRAW_NINE_GRID_ORDER multi_draw_nine_grid;
	LINE_TO_ORDER line_to;
	POLYLINE_ORDER polyline;
	MEMBLT_ORDER memblt;
	MEM3BLT_ORDER mem3blt;
	SAVE_BITMAP_ORDER save_bitmap;
	GLYPH_INDEX_ORDER glyph_index;
	FAST_INDEX_ORDER fast_index;
	FAST_GLYPH_ORDER fast_glyph;
	POLYGON_SC_ORDER polygon_sc;
	POLYGON_CB_ORDER polygon_cb;
	ELLIPSE_SC_ORDER ellipse_sc;
	ELLIPSE_CB_ORDER ellipse_cb;
};
typedef struct rdp_primary_update rdpPrimaryUpdate;

#endif /* __UPDATE_PRIMARY_H */
