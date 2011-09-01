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

#include <freerdp/rail.h>
#include <freerdp/types.h>
#include <freerdp/utils/pcap.h>
#include <freerdp/utils/stream.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Common */

struct _BOUNDS
{
	sint16 left;
	sint16 top;
	sint16 right;
	sint16 bottom;
};
typedef struct _BOUNDS BOUNDS;

struct _BRUSH
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
typedef struct _BRUSH BRUSH;

/* Bitmap Updates */

struct _BITMAP_DATA
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
};
typedef struct _BITMAP_DATA BITMAP_DATA;

struct _BITMAP_UPDATE
{
	uint16 number;
	BITMAP_DATA* bitmaps;
};
typedef struct _BITMAP_UPDATE BITMAP_UPDATE;

/* Palette Updates */

struct _PALETTE_UPDATE
{
	uint32 number;
	uint32 entries[256];
};
typedef struct _PALETTE_UPDATE PALETTE_UPDATE;

/* Pointer Updates */

struct _POINTER_POSITION_UPDATE
{
	uint16 xPos;
	uint16 yPos;
};
typedef struct _POINTER_POSITION_UPDATE POINTER_POSITION_UPDATE;

struct _POINTER_SYSTEM_UPDATE
{
	uint32 type;
};
typedef struct _POINTER_SYSTEM_UPDATE POINTER_SYSTEM_UPDATE;

struct _POINTER_COLOR_UPDATE
{
	uint16 cacheIndex;
	uint32 hotSpot;
	uint16 width;
	uint16 height;
	uint16 lengthAndMask;
	uint16 lengthXorMask;
	uint8* xorMaskData;
	uint8* andMaskData;
};
typedef struct _POINTER_COLOR_UPDATE POINTER_COLOR_UPDATE;

struct _POINTER_NEW_UPDATE
{
	uint16 xorBpp;
	POINTER_COLOR_UPDATE colorPtrAttr;
};
typedef struct _POINTER_NEW_UPDATE POINTER_NEW_UPDATE;

struct _POINTER_CACHED_UPDATE
{
	uint16 cacheIndex;
};
typedef struct _POINTER_CACHED_UPDATE POINTER_CACHED_UPDATE;

/* Play Sound (System Beep) Updates */

struct _PLAY_SOUND_UPDATE
{
	uint32 duration;
	uint32 frequency;
};
typedef struct _PLAY_SOUND_UPDATE PLAY_SOUND_UPDATE;

/* Orders Updates */

/* Primary Drawing Orders */

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
	BRUSH brush;
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
	BRUSH brush;
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
	sint16 nLeftRect;
	sint16 nTopRect;
	sint16 nWidth;
	sint16 nHeight;
	uint8 bRop;
	sint16 nXSrc;
	sint16 nYSrc;
	uint16 cacheIndex;
};
typedef struct _MEMBLT_ORDER MEMBLT_ORDER;

struct _MEM3BLT_ORDER
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
	BRUSH brush;
	uint16 cacheIndex;
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
	BRUSH brush;
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
	BRUSH brush;
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
	BRUSH brush;
};
typedef struct _ELLIPSE_CB_ORDER ELLIPSE_CB_ORDER;

/* Secondary Drawing Orders */

struct _CACHE_BITMAP_ORDER
{
	uint8 cacheId;
	uint8 bitmapBpp;
	uint8 bitmapWidth;
	uint8 bitmapHeight;
	uint16 bitmapLength;
	uint16 cacheIndex;
	uint8 bitmapComprHdr[8];
	uint8* bitmapDataStream;
};
typedef struct _CACHE_BITMAP_ORDER CACHE_BITMAP_ORDER;

struct _CACHE_BITMAP_V2_ORDER
{
	uint8 cacheId;
	uint16 flags;
	uint32 key1;
	uint32 key2;
	uint8 bitmapBpp;
	uint16 bitmapWidth;
	uint16 bitmapHeight;
	uint32 bitmapLength;
	uint16 cacheIndex;
	uint8 bitmapComprHdr[8];
	uint8* bitmapDataStream;
};
typedef struct _CACHE_BITMAP_V2_ORDER CACHE_BITMAP_V2_ORDER;

struct _BITMAP_DATA_EX
{
	uint8 bpp;
	uint8 codecID;
	uint16 width;
	uint16 height;
	uint32 length;
	uint8* data;
};
typedef struct _BITMAP_DATA_EX BITMAP_DATA_EX;

struct _CACHE_BITMAP_V3_ORDER
{
	uint8 cacheId;
	uint8 bpp;
	uint16 flags;
	uint16 cacheIndex;
	uint32 key1;
	uint32 key2;
	BITMAP_DATA_EX bitmapData;
};
typedef struct _CACHE_BITMAP_V3_ORDER CACHE_BITMAP_V3_ORDER;

struct _CACHE_COLOR_TABLE_ORDER
{
	uint8 cacheIndex;
	uint16 numberColors;
	uint32* colorTable;
};
typedef struct _CACHE_COLOR_TABLE_ORDER CACHE_COLOR_TABLE_ORDER;

struct _GLYPH_DATA
{
	uint16 cacheIndex;
	uint16 x;
	uint16 y;
	uint16 cx;
	uint16 cy;
	uint16 cb;
	uint8* aj;
};
typedef struct _GLYPH_DATA GLYPH_DATA;

struct _CACHE_GLYPH_ORDER
{
	uint8 cacheId;
	uint8 cGlyphs;
	GLYPH_DATA* glyphData[255];
	uint8* unicodeCharacters;
};
typedef struct _CACHE_GLYPH_ORDER CACHE_GLYPH_ORDER;

struct _GLYPH_DATA_V2
{
	uint8 cacheIndex;
	sint16 x;
	sint16 y;
	uint16 cx;
	uint16 cy;
	uint16 cb;
	uint8* aj;
};
typedef struct _GLYPH_DATA_V2 GLYPH_DATA_V2;

struct _CACHE_GLYPH_V2_ORDER
{
	uint8 cacheId;
	uint8 flags;
	uint8 cGlyphs;
	GLYPH_DATA_V2* glyphData[255];
	uint8* unicodeCharacters;
};
typedef struct _CACHE_GLYPH_V2_ORDER CACHE_GLYPH_V2_ORDER;

struct _CACHE_BRUSH_ORDER
{
	uint8 index;
	uint8 bpp;
	uint8 cx;
	uint8 cy;
	uint8 style;
	uint8 length;
	uint8* data;
};
typedef struct _CACHE_BRUSH_ORDER CACHE_BRUSH_ORDER;

/* Alternate Secondary Drawing Orders */

struct _OFFSCREEN_DELETE_LIST
{
	uint16 cIndices;
	uint16* indices;
};
typedef struct _OFFSCREEN_DELETE_LIST OFFSCREEN_DELETE_LIST;

struct _CREATE_OFFSCREEN_BITMAP_ORDER
{
	uint16 id;
	uint16 cx;
	uint16 cy;
	OFFSCREEN_DELETE_LIST deleteList;
};
typedef struct _CREATE_OFFSCREEN_BITMAP_ORDER CREATE_OFFSCREEN_BITMAP_ORDER;

struct _SWITCH_SURFACE_ORDER
{
	uint16 bitmapId;
};
typedef struct _SWITCH_SURFACE_ORDER SWITCH_SURFACE_ORDER;

struct _NINE_GRID_BITMAP_INFO
{
	uint32 flFlags;
	uint16 ulLeftWidth;
	uint16 ulRightWidth;
	uint16 ulTopHeight;
	uint16 ulBottomHeight;
	uint32 crTransparent;
};
typedef struct _NINE_GRID_BITMAP_INFO NINE_GRID_BITMAP_INFO;

struct _CREATE_NINE_GRID_BITMAP_ORDER
{
	uint8 bitmapBpp;
	uint16 bitmapId;
	uint16 cx;
	uint16 cy;
	NINE_GRID_BITMAP_INFO nineGridInfo;
};
typedef struct _CREATE_NINE_GRID_BITMAP_ORDER CREATE_NINE_GRID_BITMAP_ORDER;

struct _FRAME_MARKER_ORDER
{
	uint32 action;
};
typedef struct _FRAME_MARKER_ORDER FRAME_MARKER_ORDER;

struct _STREAM_BITMAP_FIRST_ORDER
{
	uint8 bitmapFlags;
	uint8 bitmapBpp;
	uint16 bitmapType;
	uint16 bitmapWidth;
	uint16 bitmapHeight;
	uint32 bitmapSize;
	uint16 bitmapBlockSize;
	uint8* bitmapBlock;
};
typedef struct _STREAM_BITMAP_FIRST_ORDER STREAM_BITMAP_FIRST_ORDER;

struct _STREAM_BITMAP_NEXT_ORDER
{
	uint8 bitmapFlags;
	uint16 bitmapType;
	uint16 bitmapBlockSize;
	uint8* bitmapBlock;
};
typedef struct _STREAM_BITMAP_NEXT_ORDER STREAM_BITMAP_NEXT_ORDER;

struct _DRAW_GDIPLUS_FIRST_ORDER
{
	uint16 cbSize;
	uint32 cbTotalSize;
	uint32 cbTotalEmfSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_FIRST_ORDER DRAW_GDIPLUS_FIRST_ORDER;

struct _DRAW_GDIPLUS_NEXT_ORDER
{
	uint16 cbSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_NEXT_ORDER DRAW_GDIPLUS_NEXT_ORDER;

struct _DRAW_GDIPLUS_END_ORDER
{
	uint16 cbSize;
	uint32 cbTotalSize;
	uint32 cbTotalEmfSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_END_ORDER DRAW_GDIPLUS_END_ORDER;

struct _DRAW_GDIPLUS_CACHE_FIRST_ORDER
{
	uint8 flags;
	uint16 cacheType;
	uint16 cacheIndex;
	uint16 cbSize;
	uint32 cbTotalSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_CACHE_FIRST_ORDER DRAW_GDIPLUS_CACHE_FIRST_ORDER;

struct _DRAW_GDIPLUS_CACHE_NEXT_ORDER
{
	uint8 flags;
	uint16 cacheType;
	uint16 cacheIndex;
	uint16 cbSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_CACHE_NEXT_ORDER DRAW_GDIPLUS_CACHE_NEXT_ORDER;

struct _DRAW_GDIPLUS_CACHE_END_ORDER
{
	uint8 flags;
	uint16 cacheType;
	uint16 cacheIndex;
	uint16 cbSize;
	uint32 cbTotalSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_CACHE_END_ORDER DRAW_GDIPLUS_CACHE_END_ORDER;

/* Window Alternate Secondary Drawing Orders */

struct _WINDOW_ORDER_INFO
{
	uint32 windowId;
	uint32 fieldFlags;
	uint32 notifyIconId;
};
typedef struct _WINDOW_ORDER_INFO WINDOW_ORDER_INFO;

struct _ICON_INFO
{
	uint16 cacheEntry;
	uint8 cacheId;
	uint8 bpp;
	uint16 width;
	uint16 height;
	uint16 cbColorTable;
	uint16 cbBitsMask;
	uint16 cbBitsColor;
	uint8* bitsMask;
	uint8* colorTable;
	uint8* bitsColor;
};
typedef struct _ICON_INFO ICON_INFO;

struct _CACHED_ICON_INFO
{
	uint16 cacheEntry;
	uint8 cacheId;
};
typedef struct _CACHED_ICON_INFO CACHED_ICON_INFO;

struct _NOTIFY_ICON_INFOTIP
{
	uint32 timeout;
	uint32 flags;
	UNICODE_STRING text;
	UNICODE_STRING title;
};
typedef struct _NOTIFY_ICON_INFOTIP NOTIFY_ICON_INFOTIP;

struct _WINDOW_STATE_ORDER
{
	uint32 ownerWindowId;
	uint32 style;
	uint32 extendedStyle;
	uint8 showState;
	UNICODE_STRING titleInfo;
	uint32 clientOffsetX;
	uint32 clientOffsetY;
	uint32 clientAreaWidth;
	uint32 clientAreaHeight;
	uint8 RPContent;
	uint32 rootParentHandle;
	uint32 windowOffsetX;
	uint32 windowOffsetY;
	uint32 windowClientDeltaX;
	uint32 windowClientDeltaY;
	uint32 windowWidth;
	uint32 windowHeight;
	uint16 numWindowRects;
	RECTANGLE_16* windowRects;
	uint32 visibleOffsetX;
	uint32 visibleOffsetY;
	uint16 numVisibilityRects;
	RECTANGLE_16* visibilityRects;
};
typedef struct _WINDOW_STATE_ORDER WINDOW_STATE_ORDER;

struct _WINDOW_ICON_ORDER
{
	ICON_INFO* iconInfo;
};
typedef struct _WINDOW_ICON_ORDER WINDOW_ICON_ORDER;

struct _WINDOW_CACHED_ICON_ORDER
{
	CACHED_ICON_INFO cachedIcon;
};
typedef struct _WINDOW_CACHED_ICON_ORDER WINDOW_CACHED_ICON_ORDER;

struct _NOTIFY_ICON_STATE_ORDER
{
	uint32 version;
	UNICODE_STRING toolTip;
	NOTIFY_ICON_INFOTIP infoTip;
	uint32 state;
	ICON_INFO icon;
	CACHED_ICON_INFO cachedIcon;
};
typedef struct _NOTIFY_ICON_STATE_ORDER NOTIFY_ICON_STATE_ORDER;

struct _MONITORED_DESKTOP_ORDER
{
	uint32 activeWindowId;
	uint8 numWindowIds;
	uint32* windowIds;
};
typedef struct _MONITORED_DESKTOP_ORDER MONITORED_DESKTOP_ORDER;

struct _SURFACE_BITS_COMMAND
{
	uint16 cmdType;
	uint16 destLeft;
	uint16 destTop;
	uint16 destRight;
	uint16 destBottom;
	uint8 bpp;
	uint8 codecID;
	uint16 width;
	uint16 height;
	uint32 bitmapDataLength;
	uint8* bitmapData;
};
typedef struct _SURFACE_BITS_COMMAND SURFACE_BITS_COMMAND;

/* Constants */

#define PTR_MSG_TYPE_SYSTEM		0x0001
#define PTR_MSG_TYPE_POSITION		0x0003
#define PTR_MSG_TYPE_COLOR		0x0006
#define PTR_MSG_TYPE_CACHED		0x0007
#define PTR_MSG_TYPE_POINTER		0x0008

#define SYSPTR_NULL			0x00000000
#define SYSPTR_DEFAULT			0x00007F00

#define CACHED_BRUSH			0x80

#define BMF_1BPP			0x1
#define BMF_8BPP			0x3
#define BMF_16BPP			0x4
#define BMF_24BPP			0x5
#define BMF_32BPP			0x6

#ifndef _WIN32
#define BS_SOLID			0x00
#define BS_NULL				0x01
#define BS_HATCHED			0x02
#define BS_PATTERN			0x03
#endif

#define HS_HORIZONTAL			0x00
#define HS_VERTICAL			0x01
#define HS_FDIAGONAL			0x02
#define HS_BDIAGONAL			0x03
#define HS_CROSS			0x04
#define HS_DIAGCROSS			0x05

#define DSDNG_STRETCH 			0x00000001
#define DSDNG_TILE 			0x00000002
#define DSDNG_PERPIXELALPHA 		0x00000004
#define DSDNG_TRANSPARENT 		0x00000008
#define DSDNG_MUSTFLIP 			0x00000010
#define DSDNG_TRUESIZE 			0x00000020

#define FRAME_START			0x00000000
#define FRAME_END			0x00000001

#define STREAM_BITMAP_END		0x01
#define STREAM_BITMAP_COMPRESSED	0x02
#define STREAM_BITMAP_V2		0x04

#define SCREEN_BITMAP_SURFACE		0xFFFF

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

/* Window Show States */
#define WINDOW_HIDE					0x00
#define WINDOW_SHOW_MINIMIZED				0x02
#define WINDOW_SHOW_MAXIMIZED				0x03
#define WINDOW_SHOW					0x05

/* Window Styles */
#ifndef _WIN32
#define WS_BORDER			0x00800000
#define WS_CAPTION			0x00C00000
#define WS_CHILD			0x40000000
#define WS_CLIPCHILDREN			0x02000000
#define WS_CLIPSIBLINGS			0x04000000
#define WS_DISABLED			0x08000000
#define WS_DLGFRAME			0x00400000
#define WS_GROUP			0x00020000
#define WS_HSCROLL			0x00100000
#define WS_ICONIC			0x20000000
#define WS_MAXIMIZE			0x01000000
#define WS_MAXIMIZEBOX			0x00010000
#define WS_MINIMIZE			0x20000000
#define WS_MINIMIZEBOX			0x00020000
#define WS_OVERLAPPED			0x00000000
#define WS_OVERLAPPEDWINDOW		(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUP			0x80000000
#define WS_POPUPWINDOW			(WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_SIZEBOX			0x00040000
#define WS_SYSMENU			0x00080000
#define WS_TABSTOP			0x00010000
#define WS_THICKFRAME			0x00040000
#define WS_VISIBLE			0x10000000
#define WS_VSCROLL			0x00200000
#endif

/* Extended Window Styles */
#ifndef _WIN32
#define WS_EX_ACCEPTFILES		0x00000010
#define WS_EX_APPWINDOW			0x00040000
#define WS_EX_CLIENTEDGE		0x00000200
#define WS_EX_COMPOSITED		0x02000000
#define WS_EX_CONTEXTHELP		0x00000400
#define WS_EX_CONTROLPARENT		0x00010000
#define WS_EX_DLGMODALFRAME		0x00000001
#define WS_EX_LAYERED			0x00080000
#define WS_EX_LAYOUTRTL			0x00400000
#define WS_EX_LEFT			0x00000000
#define WS_EX_LEFTSCROLLBAR		0x00004000
#define WS_EX_LTRREADING		0x00000000
#define WS_EX_MDICHILD			0x00000040
#define WS_EX_NOACTIVATE		0x08000000
#define WS_EX_NOINHERITLAYOUT		0x00100000
#define WS_EX_NOPARENTNOTIFY		0x00000004
#define WS_EX_OVERLAPPEDWINDOW		(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)
#define WS_EX_PALETTEWINDOW		(WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST)
#define WS_EX_RIGHT			0x00001000
#define WS_EX_RIGHTSCROLLBAR		0x00000000
#define WS_EX_RTLREADING		0x00002000
#define WS_EX_STATICEDGE		0x00020000
#define WS_EX_TOOLWINDOW		0x00000080
#define WS_EX_TOPMOST			0x00000008
#define WS_EX_TRANSPARENT		0x00000020
#define WS_EX_WINDOWEDGE		0x00000100
#endif

/* Update Interface */

typedef struct rdp_update rdpUpdate;

typedef void (*pcBeginPaint)(rdpUpdate* update);
typedef void (*pcEndPaint)(rdpUpdate* update);
typedef void (*pcSetBounds)(rdpUpdate* update, BOUNDS* bounds);
typedef void (*pcSynchronize)(rdpUpdate* update);
typedef void (*pcBitmap)(rdpUpdate* update, BITMAP_UPDATE* bitmap);
typedef void (*pcPalette)(rdpUpdate* update, PALETTE_UPDATE* palette);
typedef void (*pcPlaySound)(rdpUpdate* update, PLAY_SOUND_UPDATE* play_sound);
typedef void (*pcPointerPosition)(rdpUpdate* update, POINTER_POSITION_UPDATE* pointer_position);
typedef void (*pcPointerSystem)(rdpUpdate* update, POINTER_SYSTEM_UPDATE* pointer_system);
typedef void (*pcPointerColor)(rdpUpdate* update, POINTER_COLOR_UPDATE* pointer_color);
typedef void (*pcPointerNew)(rdpUpdate* update, POINTER_NEW_UPDATE* pointer_new);
typedef void (*pcPointerCached)(rdpUpdate* update, POINTER_CACHED_UPDATE* pointer_cached);

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
typedef void (*pcGlyphIndex)(rdpUpdate* update, GLYPH_INDEX_ORDER* glyph_index);
typedef void (*pcFastIndex)(rdpUpdate* update, FAST_INDEX_ORDER* fast_index);
typedef void (*pcFastGlyph)(rdpUpdate* update, FAST_GLYPH_ORDER* fast_glyph);
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

typedef void (*pcWindowCreate)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);
typedef void (*pcWindowUpdate)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state);
typedef void (*pcWindowIcon)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* window_icon);
typedef void (*pcWindowCachedIcon)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* window_cached_icon);
typedef void (*pcWindowDelete)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo);
typedef void (*pcNotifyIconCreate)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state);
typedef void (*pcNotifyIconUpdate)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notify_icon_state);
typedef void (*pcNotifyIconDelete)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo);
typedef void (*pcMonitoredDesktop)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitored_desktop);
typedef void (*pcNonMonitoredDesktop)(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo);

typedef void (*pcSurfaceBits)(rdpUpdate* update, SURFACE_BITS_COMMAND* surface_bits_command);
typedef void (*pcSurfaceCommand)(rdpUpdate* update, STREAM* s);

struct rdp_update
{
	void* rdp;
	void* gdi;
	void* rail;
	void* param1;
	void* param2;

	boolean dump_rfx;
	boolean play_rfx;
	rdpPcap* pcap_rfx;

	pcBeginPaint BeginPaint;
	pcEndPaint EndPaint;
	pcSetBounds SetBounds;
	pcSynchronize Synchronize;
	pcBitmap Bitmap;
	pcPalette Palette;
	pcPlaySound PlaySound;
	pcPointerPosition PointerPosition;
	pcPointerSystem PointerSystem;
	pcPointerColor PointerColor;
	pcPointerNew PointerNew;
	pcPointerCached PointerCached;

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
	pcGlyphIndex GlyphIndex;
	pcFastIndex FastIndex;
	pcFastGlyph FastGlyph;
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

	pcWindowCreate WindowCreate;
	pcWindowUpdate WindowUpdate;
	pcWindowIcon WindowIcon;
	pcWindowCachedIcon WindowCachedIcon;
	pcWindowDelete WindowDelete;
	pcNotifyIconCreate NotifyIconCreate;
	pcNotifyIconUpdate NotifyIconUpdate;
	pcNotifyIconDelete NotifyIconDelete;
	pcMonitoredDesktop MonitoredDesktop;
	pcNonMonitoredDesktop NonMonitoredDesktop;

	pcSurfaceBits SurfaceBits;
	pcSurfaceCommand SurfaceCommand;

	BITMAP_UPDATE bitmap_update;
	PALETTE_UPDATE palette_update;
	PLAY_SOUND_UPDATE play_sound;
	POINTER_POSITION_UPDATE pointer_position;
	POINTER_SYSTEM_UPDATE pointer_system;
	POINTER_COLOR_UPDATE pointer_color;
	POINTER_NEW_UPDATE pointer_new;
	POINTER_CACHED_UPDATE pointer_cached;

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

	WINDOW_ORDER_INFO orderInfo;
	WINDOW_STATE_ORDER window_state;
	WINDOW_ICON_ORDER window_icon;
	WINDOW_CACHED_ICON_ORDER window_cached_icon;
	NOTIFY_ICON_STATE_ORDER notify_icon_state;
	MONITORED_DESKTOP_ORDER monitored_desktop;

	SURFACE_BITS_COMMAND surface_bits_command;
};

#endif /* __UPDATE_API_H */

