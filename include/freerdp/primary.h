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

struct _BOUNDS
{
	sint16 left;
	sint16 top;
	sint16 right;
	sint16 bottom;
};
typedef struct _BOUNDS BOUNDS;

struct rdp_brush
{
	uint8 x;
	uint8 y;
	uint8 bpp;
	uint8 style;
	uint8 hatch;
	uint8 index;
	uint8* data;
	uint8 p8x8[8];
};
typedef struct rdp_brush rdpBrush;

struct _ORDER_INFO
{
	uint8 orderType;
	uint32 fieldFlags;
	BOUNDS bounds;
	sint8 deltaBoundLeft;
	sint8 deltaBoundTop;
	sint8 deltaBoundRight;
	sint8 deltaBoundBottom;
	boolean deltaCoordinates;
};
typedef struct _ORDER_INFO ORDER_INFO;

struct _DSTBLT_ORDER
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
};
typedef struct _DSTBLT_ORDER DSTBLT_ORDER;

struct _PATBLT_ORDER
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
};
typedef struct _PATBLT_ORDER PATBLT_ORDER;

struct _SCRBLT_ORDER
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
};
typedef struct _SCRBLT_ORDER SCRBLT_ORDER;

struct _OPAQUE_RECT_ORDER
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint32 color;
};
typedef struct _OPAQUE_RECT_ORDER OPAQUE_RECT_ORDER;

struct _DRAW_NINE_GRID_ORDER
{
	sint16 srcLeft;
	sint16 srcTop;
	sint16 srcRight;
	sint16 srcBottom;
	uint16 bitmapId;
};
typedef struct _DRAW_NINE_GRID_ORDER DRAW_NINE_GRID_ORDER;

struct _DELTA_RECT
{
	sint16 left;
	sint16 top;
	sint16 width;
	sint16 height;
};
typedef struct _DELTA_RECT DELTA_RECT;

struct _MULTI_DSTBLT_ORDER
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	uint8 numRectangles;
	uint16 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_DSTBLT_ORDER MULTI_DSTBLT_ORDER;

struct _MULTI_PATBLT_ORDER
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
	uint8 numRectangles;
	uint16 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_PATBLT_ORDER MULTI_PATBLT_ORDER;

struct _MULTI_SCRBLT_ORDER
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
	uint8 numRectangles;
	uint16 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_SCRBLT_ORDER MULTI_SCRBLT_ORDER;

struct _MULTI_OPAQUE_RECT_ORDER
{
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint32 color;
	uint8 numRectangles;
	uint16 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_OPAQUE_RECT_ORDER MULTI_OPAQUE_RECT_ORDER;

struct _MULTI_DRAW_NINE_GRID_ORDER
{
	sint16 srcLeft;
	sint16 srcTop;
	sint16 srcRight;
	sint16 srcBottom;
	uint16 bitmapId;
	uint8 nDeltaEntries;
	uint16 cbData;
	uint8* codeDeltaList;
};
typedef struct _MULTI_DRAW_NINE_GRID_ORDER MULTI_DRAW_NINE_GRID_ORDER;

struct _LINE_TO_ORDER
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
};
typedef struct _LINE_TO_ORDER LINE_TO_ORDER;

struct _DELTA_POINT
{
	sint16 x;
	sint16 y;
};
typedef struct _DELTA_POINT DELTA_POINT;

struct _POLYLINE_ORDER
{
	sint16 xStart;
	sint16 yStart;
	uint8 bRop2;
	uint32 penColor;
	uint8 numPoints;
	uint8 cbData;
	DELTA_POINT* points;
};
typedef struct _POLYLINE_ORDER POLYLINE_ORDER;

struct _MEMBLT_ORDER
{
	uint16 cacheId;
	uint8 colorIndex;
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
	uint16 cacheIndex;
	rdpBitmap* bitmap;
};
typedef struct _MEMBLT_ORDER MEMBLT_ORDER;

struct _MEM3BLT_ORDER
{
	uint16 cacheId;
	uint8 colorIndex;
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
	uint16 cacheIndex;
	rdpBitmap* bitmap;
};
typedef struct _MEM3BLT_ORDER MEM3BLT_ORDER;

struct _SAVE_BITMAP_ORDER
{
	uint32 savedBitmapPosition;
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nRightRect;
	sint16 nBottomRect;
	uint8 operation;
};
typedef struct _SAVE_BITMAP_ORDER SAVE_BITMAP_ORDER;

struct _GLYPH_FRAGMENT_INDEX
{
	uint8 index;
	uint16 delta;
};
typedef struct _GLYPH_FRAGMENT_INDEX GLYPH_FRAGMENT_INDEX;

struct _GLYPH_FRAGMENT
{
	uint8 operation;
	uint8 index;
	uint8 size;
	uint8 nindices;
	GLYPH_FRAGMENT_INDEX* indices;
};
typedef struct _GLYPH_FRAGMENT GLYPH_FRAGMENT;

struct _GLYPH_INDEX_ORDER
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
	rdpBrush brush;
	sint16 x;
	sint16 y;
	uint8 cbData;
	uint8* data;
};
typedef struct _GLYPH_INDEX_ORDER GLYPH_INDEX_ORDER;

struct _FAST_INDEX_ORDER
{
	uint8 cacheId;
	uint8 flAccel;
	uint8 ulCharInc;
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
	boolean opaqueRect;
	sint16 x;
	sint16 y;
	uint8 cbData;
	uint8* data;
};
typedef struct _FAST_INDEX_ORDER FAST_INDEX_ORDER;

struct _FAST_GLYPH_ORDER
{
	uint8 cacheId;
	uint8 flAccel;
	uint8 ulCharInc;
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
	sint16 x;
	sint16 y;
	uint8 cbData;
	uint8* data;
};
typedef struct _FAST_GLYPH_ORDER FAST_GLYPH_ORDER;

struct _POLYGON_SC_ORDER
{
	sint16 xStart;
	sint16 yStart;
	uint8 bRop2;
	uint8 fillMode;
	uint32 brushColor;
	uint8 nDeltaEntries;
	uint8 cbData;
	uint8* codeDeltaList;
};
typedef struct _POLYGON_SC_ORDER POLYGON_SC_ORDER;

struct _POLYGON_CB_ORDER
{
	sint16 xStart;
	sint16 yStart;
	uint8 bRop2;
	uint8 fillMode;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
	uint8 nDeltaEntries;
	uint8 cbData;
	uint8* codeDeltaList;
};
typedef struct _POLYGON_CB_ORDER POLYGON_CB_ORDER;

struct _ELLIPSE_SC_ORDER
{
	sint16 leftRect;
	sint16 topRect;
	sint16 rightRect;
	sint16 bottomRect;
	uint8 bRop2;
	uint8 fillMode;
	uint32 color;
};
typedef struct _ELLIPSE_SC_ORDER ELLIPSE_SC_ORDER;

struct _ELLIPSE_CB_ORDER
{
	sint16 leftRect;
	sint16 topRect;
	sint16 rightRect;
	sint16 bottomRect;
	uint8 bRop2;
	uint8 fillMode;
	uint32 backColor;
	uint32 foreColor;
	rdpBrush brush;
};
typedef struct _ELLIPSE_CB_ORDER ELLIPSE_CB_ORDER;

typedef void (*pDstBlt)(rdpUpdate* update, DSTBLT_ORDER* dstblt);
typedef void (*pPatBlt)(rdpUpdate* update, PATBLT_ORDER* patblt);
typedef void (*pScrBlt)(rdpUpdate* update, SCRBLT_ORDER* scrblt);
typedef void (*pOpaqueRect)(rdpUpdate* update, OPAQUE_RECT_ORDER* opaque_rect);
typedef void (*pDrawNineGrid)(rdpUpdate* update, DRAW_NINE_GRID_ORDER* draw_nine_grid);
typedef void (*pMultiDstBlt)(rdpUpdate* update, MULTI_DSTBLT_ORDER* multi_dstblt);
typedef void (*pMultiPatBlt)(rdpUpdate* update, MULTI_PATBLT_ORDER* multi_patblt);
typedef void (*pMultiScrBlt)(rdpUpdate* update, MULTI_SCRBLT_ORDER* multi_scrblt);
typedef void (*pMultiOpaqueRect)(rdpUpdate* update, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect);
typedef void (*pMultiDrawNineGrid)(rdpUpdate* update, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid);
typedef void (*pLineTo)(rdpUpdate* update, LINE_TO_ORDER* line_to);
typedef void (*pPolyline)(rdpUpdate* update, POLYLINE_ORDER* polyline);
typedef void (*pMemBlt)(rdpUpdate* update, MEMBLT_ORDER* memblt);
typedef void (*pMem3Blt)(rdpUpdate* update, MEM3BLT_ORDER* memblt);
typedef void (*pSaveBitmap)(rdpUpdate* update, SAVE_BITMAP_ORDER* save_bitmap);
typedef void (*pGlyphIndex)(rdpUpdate* update, GLYPH_INDEX_ORDER* glyph_index);
typedef void (*pFastIndex)(rdpUpdate* update, FAST_INDEX_ORDER* fast_index);
typedef void (*pFastGlyph)(rdpUpdate* update, FAST_GLYPH_ORDER* fast_glyph);
typedef void (*pPolygonSC)(rdpUpdate* update, POLYGON_SC_ORDER* polygon_sc);
typedef void (*pPolygonCB)(rdpUpdate* update, POLYGON_CB_ORDER* polygon_cb);
typedef void (*pEllipseSC)(rdpUpdate* update, ELLIPSE_SC_ORDER* ellipse_sc);
typedef void (*pEllipseCB)(rdpUpdate* update, ELLIPSE_CB_ORDER* ellipse_cb);

struct rdp_primary
{
	rdpContext* context; /* 0 */
	uint32 paddingA[16 - 1]; /* 1 */
};

#endif /* __UPDATE_PRIMARY_H */
