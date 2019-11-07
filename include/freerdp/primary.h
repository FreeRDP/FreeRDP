/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_UPDATE_PRIMARY_H
#define FREERDP_UPDATE_PRIMARY_H

#include <freerdp/types.h>

struct _GLYPH_DATA
{
	UINT32 cacheIndex;
	INT16 x;
	INT16 y;
	UINT32 cx;
	UINT32 cy;
	UINT32 cb;
	BYTE* aj;
};
typedef struct _GLYPH_DATA GLYPH_DATA;

struct _GLYPH_DATA_V2
{
	UINT32 cacheIndex;
	INT32 x;
	INT32 y;
	UINT32 cx;
	UINT32 cy;
	UINT32 cb;
	BYTE* aj;
};
typedef struct _GLYPH_DATA_V2 GLYPH_DATA_V2;

#define BACKMODE_TRANSPARENT 0x0001
#define BACKMODE_OPAQUE 0x0002

struct rdp_bounds
{
	INT32 left;
	INT32 top;
	INT32 right;
	INT32 bottom;
};
typedef struct rdp_bounds rdpBounds;

struct rdp_brush
{
	UINT32 x;
	UINT32 y;
	UINT32 bpp;
	UINT32 style;
	UINT32 hatch;
	UINT32 index;
	BYTE* data;
	BYTE p8x8[8];
};
typedef struct rdp_brush rdpBrush;

struct _ORDER_INFO
{
	UINT32 controlFlags;
	UINT32 orderType;
	UINT32 fieldFlags;
	UINT32 boundsFlags;
	rdpBounds bounds;
	BOOL deltaCoordinates;
};
typedef struct _ORDER_INFO ORDER_INFO;

struct _DSTBLT_ORDER
{
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 bRop;
};
typedef struct _DSTBLT_ORDER DSTBLT_ORDER;

struct _PATBLT_ORDER
{
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 bRop;
	UINT32 backColor;
	UINT32 foreColor;
	rdpBrush brush;
};
typedef struct _PATBLT_ORDER PATBLT_ORDER;

struct _SCRBLT_ORDER
{
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 bRop;
	INT32 nXSrc;
	INT32 nYSrc;
};
typedef struct _SCRBLT_ORDER SCRBLT_ORDER;

struct _OPAQUE_RECT_ORDER
{
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 color;
};
typedef struct _OPAQUE_RECT_ORDER OPAQUE_RECT_ORDER;

struct _DRAW_NINE_GRID_ORDER
{
	INT32 srcLeft;
	INT32 srcTop;
	INT32 srcRight;
	INT32 srcBottom;
	UINT32 bitmapId;
};
typedef struct _DRAW_NINE_GRID_ORDER DRAW_NINE_GRID_ORDER;

struct _DELTA_RECT
{
	INT32 left;
	INT32 top;
	INT32 width;
	INT32 height;
};
typedef struct _DELTA_RECT DELTA_RECT;

struct _MULTI_DSTBLT_ORDER
{
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 bRop;
	UINT32 numRectangles;
	UINT32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_DSTBLT_ORDER MULTI_DSTBLT_ORDER;

struct _MULTI_PATBLT_ORDER
{
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 bRop;
	UINT32 backColor;
	UINT32 foreColor;
	rdpBrush brush;
	UINT32 numRectangles;
	UINT32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_PATBLT_ORDER MULTI_PATBLT_ORDER;

struct _MULTI_SCRBLT_ORDER
{
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 bRop;
	INT32 nXSrc;
	INT32 nYSrc;
	UINT32 numRectangles;
	UINT32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_SCRBLT_ORDER MULTI_SCRBLT_ORDER;

struct _MULTI_OPAQUE_RECT_ORDER
{
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 color;
	UINT32 numRectangles;
	UINT32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_OPAQUE_RECT_ORDER MULTI_OPAQUE_RECT_ORDER;

struct _MULTI_DRAW_NINE_GRID_ORDER
{
	INT32 srcLeft;
	INT32 srcTop;
	INT32 srcRight;
	INT32 srcBottom;
	UINT32 bitmapId;
	UINT32 nDeltaEntries;
	UINT32 cbData;
	DELTA_RECT rectangles[45];
};
typedef struct _MULTI_DRAW_NINE_GRID_ORDER MULTI_DRAW_NINE_GRID_ORDER;

struct _LINE_TO_ORDER
{
	UINT32 backMode;
	INT32 nXStart;
	INT32 nYStart;
	INT32 nXEnd;
	INT32 nYEnd;
	UINT32 backColor;
	UINT32 bRop2;
	UINT32 penStyle;
	UINT32 penWidth;
	UINT32 penColor;
};
typedef struct _LINE_TO_ORDER LINE_TO_ORDER;

struct _DELTA_POINT
{
	INT32 x;
	INT32 y;
};
typedef struct _DELTA_POINT DELTA_POINT;

struct _POLYLINE_ORDER
{
	INT32 xStart;
	INT32 yStart;
	UINT32 bRop2;
	UINT32 penColor;
	UINT32 numDeltaEntries;
	UINT32 cbData;
	DELTA_POINT* points;
};
typedef struct _POLYLINE_ORDER POLYLINE_ORDER;

struct _MEMBLT_ORDER
{
	UINT32 cacheId;
	UINT32 colorIndex;
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 bRop;
	INT32 nXSrc;
	INT32 nYSrc;
	UINT32 cacheIndex;
	rdpBitmap* bitmap;
};
typedef struct _MEMBLT_ORDER MEMBLT_ORDER;

struct _MEM3BLT_ORDER
{
	UINT32 cacheId;
	UINT32 colorIndex;
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nWidth;
	INT32 nHeight;
	UINT32 bRop;
	INT32 nXSrc;
	INT32 nYSrc;
	UINT32 backColor;
	UINT32 foreColor;
	rdpBrush brush;
	UINT32 cacheIndex;
	rdpBitmap* bitmap;
};
typedef struct _MEM3BLT_ORDER MEM3BLT_ORDER;

struct _SAVE_BITMAP_ORDER
{
	UINT32 savedBitmapPosition;
	INT32 nLeftRect;
	INT32 nTopRect;
	INT32 nRightRect;
	INT32 nBottomRect;
	UINT32 operation;
};
typedef struct _SAVE_BITMAP_ORDER SAVE_BITMAP_ORDER;

struct _GLYPH_FRAGMENT_INDEX
{
	UINT32 index;
	UINT32 delta;
};
typedef struct _GLYPH_FRAGMENT_INDEX GLYPH_FRAGMENT_INDEX;

struct _GLYPH_FRAGMENT
{
	UINT32 operation;
	UINT32 index;
	UINT32 size;
	UINT32 nindices;
	GLYPH_FRAGMENT_INDEX* indices;
};
typedef struct _GLYPH_FRAGMENT GLYPH_FRAGMENT;

struct _GLYPH_INDEX_ORDER
{
	UINT32 cacheId;
	UINT32 flAccel;
	UINT32 ulCharInc;
	UINT32 fOpRedundant;
	UINT32 backColor;
	UINT32 foreColor;
	INT32 bkLeft;
	INT32 bkTop;
	INT32 bkRight;
	INT32 bkBottom;
	INT32 opLeft;
	INT32 opTop;
	INT32 opRight;
	INT32 opBottom;
	rdpBrush brush;
	INT32 x;
	INT32 y;
	UINT32 cbData;
	BYTE data[256];
};
typedef struct _GLYPH_INDEX_ORDER GLYPH_INDEX_ORDER;

struct _FAST_INDEX_ORDER
{
	UINT32 cacheId;
	UINT32 flAccel;
	UINT32 ulCharInc;
	UINT32 backColor;
	UINT32 foreColor;
	INT32 bkLeft;
	INT32 bkTop;
	INT32 bkRight;
	INT32 bkBottom;
	INT32 opLeft;
	INT32 opTop;
	INT32 opRight;
	INT32 opBottom;
	BOOL opaqueRect;
	INT32 x;
	INT32 y;
	UINT32 cbData;
	BYTE data[256];
};
typedef struct _FAST_INDEX_ORDER FAST_INDEX_ORDER;

struct _FAST_GLYPH_ORDER
{
	UINT32 cacheId;
	UINT32 flAccel;
	UINT32 ulCharInc;
	UINT32 backColor;
	UINT32 foreColor;
	INT32 bkLeft;
	INT32 bkTop;
	INT32 bkRight;
	INT32 bkBottom;
	INT32 opLeft;
	INT32 opTop;
	INT32 opRight;
	INT32 opBottom;
	INT32 x;
	INT32 y;
	UINT32 cbData;
	BYTE data[256];
	GLYPH_DATA_V2 glyphData;
};
typedef struct _FAST_GLYPH_ORDER FAST_GLYPH_ORDER;

struct _POLYGON_SC_ORDER
{
	INT32 xStart;
	INT32 yStart;
	UINT32 bRop2;
	UINT32 fillMode;
	UINT32 brushColor;
	UINT32 numPoints;
	UINT32 cbData;
	DELTA_POINT* points;
};
typedef struct _POLYGON_SC_ORDER POLYGON_SC_ORDER;

struct _POLYGON_CB_ORDER
{
	INT32 xStart;
	INT32 yStart;
	UINT32 bRop2;
	UINT32 backMode;
	UINT32 fillMode;
	UINT32 backColor;
	UINT32 foreColor;
	rdpBrush brush;
	UINT32 numPoints;
	UINT32 cbData;
	DELTA_POINT* points;
};
typedef struct _POLYGON_CB_ORDER POLYGON_CB_ORDER;

struct _ELLIPSE_SC_ORDER
{
	INT32 leftRect;
	INT32 topRect;
	INT32 rightRect;
	INT32 bottomRect;
	UINT32 bRop2;
	UINT32 fillMode;
	UINT32 color;
};
typedef struct _ELLIPSE_SC_ORDER ELLIPSE_SC_ORDER;

struct _ELLIPSE_CB_ORDER
{
	INT32 leftRect;
	INT32 topRect;
	INT32 rightRect;
	INT32 bottomRect;
	UINT32 bRop2;
	UINT32 fillMode;
	UINT32 backColor;
	UINT32 foreColor;
	rdpBrush brush;
};
typedef struct _ELLIPSE_CB_ORDER ELLIPSE_CB_ORDER;

typedef BOOL (*pDstBlt)(rdpContext* context, const DSTBLT_ORDER* dstblt);
typedef BOOL (*pPatBlt)(rdpContext* context, PATBLT_ORDER* patblt);
typedef BOOL (*pScrBlt)(rdpContext* context, const SCRBLT_ORDER* scrblt);
typedef BOOL (*pOpaqueRect)(rdpContext* context, const OPAQUE_RECT_ORDER* opaque_rect);
typedef BOOL (*pDrawNineGrid)(rdpContext* context, const DRAW_NINE_GRID_ORDER* draw_nine_grid);
typedef BOOL (*pMultiDstBlt)(rdpContext* context, const MULTI_DSTBLT_ORDER* multi_dstblt);
typedef BOOL (*pMultiPatBlt)(rdpContext* context, const MULTI_PATBLT_ORDER* multi_patblt);
typedef BOOL (*pMultiScrBlt)(rdpContext* context, const MULTI_SCRBLT_ORDER* multi_scrblt);
typedef BOOL (*pMultiOpaqueRect)(rdpContext* context,
                                 const MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect);
typedef BOOL (*pMultiDrawNineGrid)(rdpContext* context,
                                   const MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid);
typedef BOOL (*pLineTo)(rdpContext* context, const LINE_TO_ORDER* line_to);
typedef BOOL (*pPolyline)(rdpContext* context, const POLYLINE_ORDER* polyline);
typedef BOOL (*pMemBlt)(rdpContext* context, MEMBLT_ORDER* memblt);
typedef BOOL (*pMem3Blt)(rdpContext* context, MEM3BLT_ORDER* memblt);
typedef BOOL (*pSaveBitmap)(rdpContext* context, const SAVE_BITMAP_ORDER* save_bitmap);
typedef BOOL (*pGlyphIndex)(rdpContext* context, GLYPH_INDEX_ORDER* glyph_index);
typedef BOOL (*pFastIndex)(rdpContext* context, const FAST_INDEX_ORDER* fast_index);
typedef BOOL (*pFastGlyph)(rdpContext* context, const FAST_GLYPH_ORDER* fast_glyph);
typedef BOOL (*pPolygonSC)(rdpContext* context, const POLYGON_SC_ORDER* polygon_sc);
typedef BOOL (*pPolygonCB)(rdpContext* context, POLYGON_CB_ORDER* polygon_cb);
typedef BOOL (*pEllipseSC)(rdpContext* context, const ELLIPSE_SC_ORDER* ellipse_sc);
typedef BOOL (*pEllipseCB)(rdpContext* context, const ELLIPSE_CB_ORDER* ellipse_cb);

struct rdp_primary_update
{
	rdpContext* context;     /* 0 */
	UINT32 paddingA[16 - 1]; /* 1 */

	pDstBlt DstBlt;                       /* 16 */
	pPatBlt PatBlt;                       /* 17 */
	pScrBlt ScrBlt;                       /* 18 */
	pOpaqueRect OpaqueRect;               /* 19 */
	pDrawNineGrid DrawNineGrid;           /* 20 */
	pMultiDstBlt MultiDstBlt;             /* 21 */
	pMultiPatBlt MultiPatBlt;             /* 22 */
	pMultiScrBlt MultiScrBlt;             /* 23 */
	pMultiOpaqueRect MultiOpaqueRect;     /* 24 */
	pMultiDrawNineGrid MultiDrawNineGrid; /* 25 */
	pLineTo LineTo;                       /* 26 */
	pPolyline Polyline;                   /* 27 */
	pMemBlt MemBlt;                       /* 28 */
	pMem3Blt Mem3Blt;                     /* 29 */
	pSaveBitmap SaveBitmap;               /* 30 */
	pGlyphIndex GlyphIndex;               /* 31 */
	pFastIndex FastIndex;                 /* 32 */
	pFastGlyph FastGlyph;                 /* 33 */
	pPolygonSC PolygonSC;                 /* 34 */
	pPolygonCB PolygonCB;                 /* 35 */
	pEllipseSC EllipseSC;                 /* 36 */
	pEllipseCB EllipseCB;                 /* 37 */
	UINT32 paddingB[48 - 38];             /* 38 */

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

#endif /* FREERDP_UPDATE_PRIMARY_H */
