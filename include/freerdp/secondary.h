/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Secondary Drawing Orders Interface API
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

#ifndef __UPDATE_SECONDARY_H
#define __UPDATE_SECONDARY_H

#include <freerdp/types.h>

#define GLYPH_FRAGMENT_NOP		0x00
#define GLYPH_FRAGMENT_USE		0xFE
#define GLYPH_FRAGMENT_ADD		0xFF

#define CBR2_HEIGHT_SAME_AS_WIDTH	0x01
#define CBR2_PERSISTENT_KEY_PRESENT	0x02
#define CBR2_NO_BITMAP_COMPRESSION_HDR	0x08
#define CBR2_DO_NOT_CACHE		0x10

#define SCREEN_BITMAP_SURFACE		0xFFFF
#define BITMAP_CACHE_WAITING_LIST_INDEX 0x7FFF

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

#define SO_FLAG_DEFAULT_PLACEMENT	0x01
#define SO_HORIZONTAL			0x02
#define SO_VERTICAL			0x04
#define SO_REVERSED			0x08
#define SO_ZERO_BEARINGS		0x10
#define SO_CHAR_INC_EQUAL_BM_BASE	0x20
#define SO_MAXEXT_EQUAL_BM_SIDE		0x40

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
	boolean compressed;
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
	sint16 x;
	sint16 y;
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

typedef void (*pCacheBitmap)(rdpUpdate* update, CACHE_BITMAP_ORDER* cache_bitmap_order);
typedef void (*pCacheBitmapV2)(rdpUpdate* update, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2_order);
typedef void (*pCacheBitmapV3)(rdpUpdate* update, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3_order);
typedef void (*pCacheColorTable)(rdpUpdate* update, CACHE_COLOR_TABLE_ORDER* cache_color_table_order);
typedef void (*pCacheGlyph)(rdpUpdate* update, CACHE_GLYPH_ORDER* cache_glyph_order);
typedef void (*pCacheGlyphV2)(rdpUpdate* update, CACHE_GLYPH_V2_ORDER* cache_glyph_v2_order);
typedef void (*pCacheBrush)(rdpUpdate* update, CACHE_BRUSH_ORDER* cache_brush_order);

#endif /* __UPDATE_SECONDARY_H */
