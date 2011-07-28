/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Update Interface API
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

#ifndef __UPDATE_API_H
#define __UPDATE_API_H

#include <freerdp/types.h>

/* Bitmap Updates */

typedef struct
{
	uint16 left;
	uint16 top;
	uint16 right;
	uint16 bottom;
	uint16 width;
	uint16 height;
	uint16 bpp;
	uint16 flags;
	uint16 length;
	uint8* data;
} BITMAP_DATA;

typedef struct
{
	uint16 number;
	BITMAP_DATA* bitmaps;
} BITMAP_UPDATE;

/* Palette Updates */

typedef struct
{
	uint32 number;
	uint32 entries[256];
} PALETTE_UPDATE;

/* Orders Updates */

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

/* Update Interface */

typedef struct rdp_update rdpUpdate;

typedef int (*pcBeginPaint)(rdpUpdate* update);
typedef int (*pcEndPaint)(rdpUpdate* update);
typedef int (*pcSynchronize)(rdpUpdate* update);
typedef int (*pcBitmap)(rdpUpdate* update, BITMAP_UPDATE* bitmap);
typedef int (*pcPalette)(rdpUpdate* update, PALETTE_UPDATE* palette);
typedef int (*pcDstBlt)(rdpUpdate* update, DSTBLT_ORDER* dstblt);
typedef int (*pcPatBlt)(rdpUpdate* update, PATBLT_ORDER* patblt);
typedef int (*pcScrBlt)(rdpUpdate* update, SCRBLT_ORDER* scrblt);
typedef int (*pcDrawNineGrid)(rdpUpdate* update, DRAW_NINE_GRID_ORDER* draw_nine_grid);
typedef int (*pcMultiDrawNineGrid)(rdpUpdate* update, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid);
typedef int (*pcLineTo)(rdpUpdate* update, LINE_TO_ORDER* line_to);
typedef int (*pcOpaqueRect)(rdpUpdate* update, OPAQUE_RECT_ORDER* opaque_rect);
typedef int (*pcSaveBitmap)(rdpUpdate* update, SAVE_BITMAP_ORDER* save_bitmap);
typedef int (*pcMemBlt)(rdpUpdate* update, MEMBLT_ORDER* memblt);
typedef int (*pcMem3Blt)(rdpUpdate* update, MEM3BLT_ORDER* memblt);
typedef int (*pcMultiDstBlt)(rdpUpdate* update, MULTI_DSTBLT_ORDER* multi_dstblt);
typedef int (*pcMultiPatBlt)(rdpUpdate* update, MULTI_PATBLT_ORDER* multi_patblt);
typedef int (*pcMultiScrBlt)(rdpUpdate* update, MULTI_SCRBLT_ORDER* multi_scrblt);
typedef int (*pcMultiOpaqueRect)(rdpUpdate* update, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect);
typedef int (*pcFastIndex)(rdpUpdate* update, FAST_INDEX_ORDER* fast_index);
typedef int (*pcPolygonSC)(rdpUpdate* update, POLYGON_SC_ORDER* polygon_sc);
typedef int (*pcPolygonCB)(rdpUpdate* update, POLYGON_CB_ORDER* polygon_cb);
typedef int (*pcPolyline)(rdpUpdate* update, POLYLINE_ORDER* polyline);
typedef int (*pcFastGlyph)(rdpUpdate* update, FAST_GLYPH_ORDER* fast_glyph);
typedef int (*pcEllipseSC)(rdpUpdate* update, ELLIPSE_SC_ORDER* ellipse_sc);
typedef int (*pcEllipseCB)(rdpUpdate* update, ELLIPSE_CB_ORDER* ellipse_cb);
typedef int (*pcGlyphIndex)(rdpUpdate* update, GLYPH_INDEX_ORDER* glyph_index);

struct rdp_update
{
	void* rdp;
	void* gdi;
	void* param1;
	void* param2;

	pcBeginPaint BeginPaint;
	pcEndPaint EndPaint;
	pcSynchronize Synchronize;
	pcBitmap Bitmap;
	pcPalette Palette;
	pcDstBlt DstBlt;
	pcPatBlt PatBlt;
	pcScrBlt ScrBlt;
	pcDrawNineGrid DrawNineGrid;
	pcMultiDrawNineGrid MultiDrawNineGrid;
	pcLineTo LineTo;
	pcOpaqueRect OpaqueRect;
	pcSaveBitmap SaveBitmap;
	pcMemBlt MemBlt;
	pcMem3Blt Mem3Blt;
	pcMultiDstBlt MultiDstBlt;
	pcMultiPatBlt MultiPatBlt;
	pcMultiScrBlt MultiScrBlt;
	pcMultiOpaqueRect MultiOpaqueRect;
	pcFastIndex FastIndex;
	pcPolygonSC PolygonSC;
	pcPolygonCB PolygonCB;
	pcPolyline Polyline;
	pcFastGlyph FastGlyph;
	pcEllipseSC EllipseSC;
	pcEllipseCB EllipseCB;
	pcGlyphIndex GlyphIndex;

	BITMAP_UPDATE bitmap_update;
	PALETTE_UPDATE palette_update;
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

#endif /* __UPDATE_API_H */

