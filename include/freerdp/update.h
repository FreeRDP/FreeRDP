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

/* Common */

typedef struct
{
	uint16 left;
	uint16 top;
	uint16 right;
	uint16 bottom;
} BOUNDS;

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

/* Primary Drawing Orders */

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

/* Secondary Drawing Orders */

typedef struct
{
	uint8 cacheId;
	uint8 bitmapBpp;
	uint8 bitmapWidth;
	uint8 bitmapHeight;
	uint16 bitmapLength;
	uint16 cacheIndex;
	uint8* bitmapComprHdr;
	uint8* bitmapDataStream;
} CACHE_BITMAP_ORDER;

typedef struct
{
	uint8 cacheId;
	uint16 flags;
	uint32 key1;
	uint32 key2;
	uint8 bitmapBpp;
	uint8 bitmapWidth;
	uint8 bitmapHeight;
	uint16 bitmapLength;
	uint16 cacheIndex;
	uint8* bitmapComprHdr;
	uint8* bitmapDataStream;
} CACHE_BITMAP_V2_ORDER;

typedef struct
{
	uint8 cacheId;
	uint8 bpp;
	uint16 flags;
	uint16 cacheIndex;
	uint32 key1;
	uint32 key2;
	uint8* bitmapData;
} CACHE_BITMAP_V3_ORDER;

typedef struct
{
	uint8 cacheIndex;
	uint16 numberColors;
	uint32* colorTable;
} CACHE_COLOR_TABLE_ORDER;

typedef struct
{
	uint16 cacheIndex;
	uint16 x;
	uint16 y;
	uint16 cx;
	uint16 cy;
	uint8* aj;
} GLYPH_DATA;

typedef struct
{
	uint8 cacheId;
	uint8 cGlyphs;
	GLYPH_DATA* glyphData;
	uint8* unicodeCharacters;
} CACHE_GLYPH_ORDER;

typedef struct
{
	uint8 cacheIndex;
	sint16 x;
	sint16 y;
	uint16 cx;
	uint16 cy;
	uint8* aj;
} GLYPH_DATA_V2;

typedef struct
{
	uint8 cacheId;
	uint8 flags;
	uint8 cGlyphs;
	GLYPH_DATA_V2* glyphData;
	uint8* unicodeCharacters;
} CACHE_GLYPH_V2_ORDER;

typedef struct
{
	uint8 cacheEntry;
	uint8 iBitmapFormat;
	uint8 cx;
	uint8 cy;
	uint8 style;
	uint8 iBytes;
	uint8* brushData;
} CACHE_BRUSH_ORDER;

/* Alternate Secondary Drawing Orders */

typedef struct
{
	uint16 cIndices;
	uint16* indices;
} OFFSCREEN_DELETE_LIST;

typedef struct
{
	uint16 offscreenBitmapId;
	uint16 cx;
	uint16 cy;
	OFFSCREEN_DELETE_LIST deleteList;
} CREATE_OFFSCREEN_BITMAP_ORDER;

typedef struct
{
	uint16 bitmapId;
} SWITCH_SURFACE_ORDER;

typedef struct
{
	uint32 flFlags;
	uint16 ulLeftWidth;
	uint16 ulRightWidth;
	uint16 ulTopHeight;
	uint16 ulBottomHeight;
	uint32 crTransparent;
} NINE_GRID_BITMAP_INFO;

typedef struct
{
	uint8 bitmapBpp;
	uint16 bitmapId;
	uint16 cx;
	uint16 cy;
	NINE_GRID_BITMAP_INFO nineGridInfo;
} CREATE_NINE_GRID_BITMAP_ORDER;

typedef struct
{
	uint32 action;
} FRAME_MARKER_ORDER;

typedef struct
{

} STREAM_BITMAP_FIRST_ORDER;

typedef struct
{

} STREAM_BITMAP_NEXT_ORDER;

typedef struct
{

} DRAW_GDIPLUS_FIRST_ORDER;

typedef struct
{

} DRAW_GDIPLUS_NEXT_ORDER;

typedef struct
{

} DRAW_GDIPLUS_END_ORDER;

typedef struct
{

} DRAW_GDIPLUS_CACHE_FIRST_ORDER;

typedef struct
{

} DRAW_GDIPLUS_CACHE_NEXT_ORDER;

typedef struct
{

} DRAW_GDIPLUS_CACHE_END_ORDER;

/* Constants */

#define CACHED_BRUSH	0x80

#define BMF_1BPP	0x1
#define BMF_8BPP	0x3
#define BMF_16BPP	0x4
#define BMF_24BPP	0x5
#define BMF_32BPP	0x6

#define BS_SOLID	0x00
#define BS_NULL		0x01
#define BS_HATCHED	0x02
#define BS_PATTERN	0x03

#define HS_HORIZONTAL	0x00
#define HS_VERTICAL	0x01
#define HS_FDIAGONAL	0x02
#define HS_BDIAGONAL	0x03
#define HS_CROSS	0x04
#define HS_DIAGCROSS	0x05

#define DSDNG_STRETCH 		0x00000001
#define DSDNG_TILE 		0x00000002
#define DSDNG_PERPIXELALPHA 	0x00000004
#define DSDNG_TRANSPARENT 	0x00000008
#define DSDNG_MUSTFLIP 		0x00000010
#define DSDNG_TRUESIZE 		0x00000020

#define FRAME_START		0x00000000
#define FRAME_END		0x00000001

/* Update Interface */

typedef struct rdp_update rdpUpdate;

typedef void (*pcBeginPaint)(rdpUpdate* update);
typedef void (*pcEndPaint)(rdpUpdate* update);
typedef void (*pcSetBounds)(rdpUpdate* update, BOUNDS* bounds);
typedef void (*pcSynchronize)(rdpUpdate* update);
typedef void (*pcBitmap)(rdpUpdate* update, BITMAP_UPDATE* bitmap);
typedef void (*pcPalette)(rdpUpdate* update, PALETTE_UPDATE* palette);

typedef void (*pcDstBlt)(rdpUpdate* update, DSTBLT_ORDER* dstblt);
typedef void (*pcPatBlt)(rdpUpdate* update, PATBLT_ORDER* patblt);
typedef void (*pcScrBlt)(rdpUpdate* update, SCRBLT_ORDER* scrblt);
typedef void (*pcOpaqueRect)(rdpUpdate* update, OPAQUE_RECT_ORDER* opaque_rect);
typedef void (*pcDrawNineGrid)(rdpUpdate* update, DRAW_NINE_GRID_ORDER* draw_nine_grid);
typedef void (*pcMultiDstBlt)(rdpUpdate* update, MULTI_DSTBLT_ORDER* multi_dstblt);
typedef void (*pcMultiPatBlt)(rdpUpdate* update, MULTI_PATBLT_ORDER* multi_patblt);
typedef void (*pcMultiScrBlt)(rdpUpdate* update, MULTI_SCRBLT_ORDER* multi_scrblt);
typedef void (*pcMultiOpaqueRect)(rdpUpdate* update, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect);
typedef void (*pcMultiDrawNineGrid)(rdpUpdate* update, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid);
typedef void (*pcLineTo)(rdpUpdate* update, LINE_TO_ORDER* line_to);
typedef void (*pcPolyline)(rdpUpdate* update, POLYLINE_ORDER* polyline);
typedef void (*pcMemBlt)(rdpUpdate* update, MEMBLT_ORDER* memblt);
typedef void (*pcMem3Blt)(rdpUpdate* update, MEM3BLT_ORDER* memblt);
typedef void (*pcSaveBitmap)(rdpUpdate* update, SAVE_BITMAP_ORDER* save_bitmap);
typedef void (*pcFastIndex)(rdpUpdate* update, FAST_INDEX_ORDER* fast_index);
typedef void (*pcFastGlyph)(rdpUpdate* update, FAST_GLYPH_ORDER* fast_glyph);
typedef void (*pcGlyphIndex)(rdpUpdate* update, GLYPH_INDEX_ORDER* glyph_index);
typedef void (*pcPolygonSC)(rdpUpdate* update, POLYGON_SC_ORDER* polygon_sc);
typedef void (*pcPolygonCB)(rdpUpdate* update, POLYGON_CB_ORDER* polygon_cb);
typedef void (*pcEllipseSC)(rdpUpdate* update, ELLIPSE_SC_ORDER* ellipse_sc);
typedef void (*pcEllipseCB)(rdpUpdate* update, ELLIPSE_CB_ORDER* ellipse_cb);

typedef void (*pcCacheBitmap)(rdpUpdate* update, CACHE_BITMAP_ORDER* cache_bitmap_order);
typedef void (*pcCacheBitmapV2)(rdpUpdate* update, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2_order);
typedef void (*pcCacheBitmapV3)(rdpUpdate* update, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3_order);
typedef void (*pcCacheColorTable)(rdpUpdate* update, CACHE_COLOR_TABLE_ORDER* cache_color_table_order);
typedef void (*pcCacheGlyph)(rdpUpdate* update, CACHE_GLYPH_ORDER* cache_glyph_order);
typedef void (*pcCacheGlyphV2)(rdpUpdate* update, CACHE_GLYPH_V2_ORDER* cache_glyph_v2_order);
typedef void (*pcCacheBrush)(rdpUpdate* update, CACHE_BRUSH_ORDER* cache_brush_order);

typedef void (*pcCreateOffscreenBitmap)(rdpUpdate* update, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap);
typedef void (*pcSwitchSurface)(rdpUpdate* update, SWITCH_SURFACE_ORDER* switch_surface);
typedef void (*pcCreateNineGridBitmap)(rdpUpdate* update, CREATE_NINE_GRID_BITMAP_ORDER* create_nine_grid_bitmap);
typedef void (*pcFrameMarker)(rdpUpdate* update, FRAME_MARKER_ORDER* frame_marker);
typedef void (*pcStreamBitmapFirst)(rdpUpdate* update, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_first);
typedef void (*pcStreamBitmapNext)(rdpUpdate* update, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_next);
typedef void (*pcDrawGdiPlusFirst)(rdpUpdate* update, DRAW_GDIPLUS_FIRST_ORDER* draw_gdiplus_first);
typedef void (*pcDrawGdiPlusNext)(rdpUpdate* update, DRAW_GDIPLUS_NEXT_ORDER* draw_gdiplus_next);
typedef void (*pcDrawGdiPlusEnd)(rdpUpdate* update, DRAW_GDIPLUS_END_ORDER* draw_gdiplus_end);
typedef void (*pcDrawGdiPlusCacheFirst)(rdpUpdate* update, DRAW_GDIPLUS_CACHE_FIRST_ORDER* draw_gdiplus_cache_first);
typedef void (*pcDrawGdiPlusCacheNext)(rdpUpdate* update, DRAW_GDIPLUS_CACHE_NEXT_ORDER* draw_gdiplus_cache_next);
typedef void (*pcDrawGdiPlusCacheEnd)(rdpUpdate* update, DRAW_GDIPLUS_CACHE_END_ORDER* draw_gdiplus_cache_end);

struct rdp_update
{
	void* rdp;
	void* gdi;
	void* param1;
	void* param2;

	pcBeginPaint BeginPaint;
	pcEndPaint EndPaint;
	pcSetBounds SetBounds;
	pcSynchronize Synchronize;
	pcBitmap Bitmap;
	pcPalette Palette;

	pcDstBlt DstBlt;
	pcPatBlt PatBlt;
	pcScrBlt ScrBlt;
	pcOpaqueRect OpaqueRect;
	pcDrawNineGrid DrawNineGrid;
	pcMultiDstBlt MultiDstBlt;
	pcMultiPatBlt MultiPatBlt;
	pcMultiScrBlt MultiScrBlt;
	pcMultiOpaqueRect MultiOpaqueRect;
	pcMultiDrawNineGrid MultiDrawNineGrid;
	pcLineTo LineTo;
	pcPolyline Polyline;
	pcMemBlt MemBlt;
	pcMem3Blt Mem3Blt;
	pcSaveBitmap SaveBitmap;
	pcFastIndex FastIndex;
	pcFastGlyph FastGlyph;
	pcGlyphIndex GlyphIndex;
	pcPolygonSC PolygonSC;
	pcPolygonCB PolygonCB;
	pcEllipseSC EllipseSC;
	pcEllipseCB EllipseCB;

	boolean glyph_v2;
	pcCacheBitmap CacheBitmap;
	pcCacheBitmapV2 CacheBitmapV2;
	pcCacheBitmapV3 CacheBitmapV3;
	pcCacheColorTable CacheColorTable;
	pcCacheGlyph CacheGlyph;
	pcCacheGlyphV2 CacheGlyphV2;
	pcCacheBrush CacheBrush;

	pcCreateOffscreenBitmap CreateOffscreenBitmap;
	pcSwitchSurface SwitchSurface;
	pcCreateNineGridBitmap CreateNineGridBitmap;
	pcFrameMarker FrameMarker;
	pcStreamBitmapFirst StreamBitmapFirst;
	pcStreamBitmapNext StreamBitmapNext;
	pcDrawGdiPlusFirst DrawGdiPlusFirst;
	pcDrawGdiPlusNext DrawGdiPlusNext;
	pcDrawGdiPlusEnd DrawGdiPlusEnd;
	pcDrawGdiPlusCacheFirst DrawGdiPlusCacheFirst;
	pcDrawGdiPlusCacheNext DrawGdiPlusCacheNext;
	pcDrawGdiPlusCacheEnd DrawGdiPlusCacheEnd;

	BITMAP_UPDATE bitmap_update;
	PALETTE_UPDATE palette_update;
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
	FAST_INDEX_ORDER fast_index;
	FAST_GLYPH_ORDER fast_glyph;
	GLYPH_INDEX_ORDER glyph_index;
	POLYGON_SC_ORDER polygon_sc;
	POLYGON_CB_ORDER polygon_cb;
	ELLIPSE_SC_ORDER ellipse_sc;
	ELLIPSE_CB_ORDER ellipse_cb;

	CACHE_BITMAP_ORDER cache_bitmap_order;
	CACHE_BITMAP_V2_ORDER cache_bitmap_v2_order;
	CACHE_BITMAP_V3_ORDER cache_bitmap_v3_order;
	CACHE_COLOR_TABLE_ORDER cache_color_table_order;
	CACHE_GLYPH_ORDER cache_glyph_order;
	CACHE_GLYPH_V2_ORDER cache_glyph_v2_order;
	CACHE_BRUSH_ORDER cache_brush_order;

	CREATE_OFFSCREEN_BITMAP_ORDER create_offscreen_bitmap;
	SWITCH_SURFACE_ORDER switch_surface;
	CREATE_NINE_GRID_BITMAP_ORDER create_nine_grid_bitmap;
	FRAME_MARKER_ORDER frame_marker;
	STREAM_BITMAP_FIRST_ORDER stream_bitmap_first;
	STREAM_BITMAP_FIRST_ORDER stream_bitmap_next;
	DRAW_GDIPLUS_CACHE_FIRST_ORDER draw_gdiplus_cache_first;
	DRAW_GDIPLUS_CACHE_NEXT_ORDER draw_gdiplus_cache_next;
	DRAW_GDIPLUS_CACHE_END_ORDER draw_gdiplus_cache_end;
	DRAW_GDIPLUS_FIRST_ORDER draw_gdiplus_first;
	DRAW_GDIPLUS_NEXT_ORDER draw_gdiplus_next;
	DRAW_GDIPLUS_END_ORDER draw_gdiplus_end;
};

#endif /* __UPDATE_API_H */

