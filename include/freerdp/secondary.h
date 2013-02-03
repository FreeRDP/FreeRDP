/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_UPDATE_SECONDARY_H
#define FREERDP_UPDATE_SECONDARY_H

#include <freerdp/types.h>
#include <freerdp/primary.h>

#define GLYPH_FRAGMENT_NOP			0x00
#define GLYPH_FRAGMENT_USE			0xFE
#define GLYPH_FRAGMENT_ADD			0xFF

#define CBR2_HEIGHT_SAME_AS_WIDTH		0x01
#define CBR2_PERSISTENT_KEY_PRESENT		0x02
#define CBR2_NO_BITMAP_COMPRESSION_HDR		0x08
#define CBR2_DO_NOT_CACHE			0x10

#define SCREEN_BITMAP_SURFACE			0xFFFF
#define BITMAP_CACHE_WAITING_LIST_INDEX 	0x7FFF

#define CACHED_BRUSH				0x80

#define BMF_1BPP				0x1
#define BMF_8BPP				0x3
#define BMF_16BPP				0x4
#define BMF_24BPP				0x5
#define BMF_32BPP				0x6

#ifndef _WIN32
#define BS_SOLID				0x00
#define BS_NULL					0x01
#define BS_HATCHED				0x02
#define BS_PATTERN				0x03
#endif

#ifndef _WIN32
#define HS_HORIZONTAL				0x00
#define HS_VERTICAL				0x01
#define HS_FDIAGONAL				0x02
#define HS_BDIAGONAL				0x03
#define HS_CROSS				0x04
#define HS_DIAGCROSS				0x05
#endif

#define SO_FLAG_DEFAULT_PLACEMENT		0x01
#define SO_HORIZONTAL				0x02
#define SO_VERTICAL				0x04
#define SO_REVERSED				0x08
#define SO_ZERO_BEARINGS			0x10
#define SO_CHAR_INC_EQUAL_BM_BASE		0x20
#define SO_MAXEXT_EQUAL_BM_SIDE			0x40

struct _CACHE_BITMAP_ORDER
{
	UINT32 cacheId;
	UINT32 bitmapBpp;
	UINT32 bitmapWidth;
	UINT32 bitmapHeight;
	UINT32 bitmapLength;
	UINT32 cacheIndex;
	BOOL compressed;
	BYTE bitmapComprHdr[8];
	BYTE* bitmapDataStream;
};
typedef struct _CACHE_BITMAP_ORDER CACHE_BITMAP_ORDER;

struct _CACHE_BITMAP_V2_ORDER
{
	UINT32 cacheId;
	UINT32 flags;
	UINT32 key1;
	UINT32 key2;
	UINT32 bitmapBpp;
	UINT32 bitmapWidth;
	UINT32 bitmapHeight;
	UINT32 bitmapLength;
	UINT32 cacheIndex;
	BOOL compressed;
	UINT32 cbCompFirstRowSize;
	UINT32 cbCompMainBodySize;
	UINT32 cbScanWidth;
	UINT32 cbUncompressedSize;
	BYTE* bitmapDataStream;
};
typedef struct _CACHE_BITMAP_V2_ORDER CACHE_BITMAP_V2_ORDER;

struct _BITMAP_DATA_EX
{
	UINT32 bpp;
	UINT32 codecID;
	UINT32 width;
	UINT32 height;
	UINT32 length;
	BYTE* data;
};
typedef struct _BITMAP_DATA_EX BITMAP_DATA_EX;

struct _CACHE_BITMAP_V3_ORDER
{
	UINT32 cacheId;
	UINT32 bpp;
	UINT32 flags;
	UINT32 cacheIndex;
	UINT32 key1;
	UINT32 key2;
	BITMAP_DATA_EX bitmapData;
};
typedef struct _CACHE_BITMAP_V3_ORDER CACHE_BITMAP_V3_ORDER;

struct _CACHE_COLOR_TABLE_ORDER
{
	UINT32 cacheIndex;
	UINT32 numberColors;
	UINT32 colorTable[256];
};
typedef struct _CACHE_COLOR_TABLE_ORDER CACHE_COLOR_TABLE_ORDER;

struct _CACHE_GLYPH_ORDER
{
	UINT32 cacheId;
	UINT32 cGlyphs;
	GLYPH_DATA glyphData[256];
	BYTE* unicodeCharacters;
};
typedef struct _CACHE_GLYPH_ORDER CACHE_GLYPH_ORDER;

struct _CACHE_GLYPH_V2_ORDER
{
	UINT32 cacheId;
	UINT32 flags;
	UINT32 cGlyphs;
	GLYPH_DATA_V2 glyphData[256];
	BYTE* unicodeCharacters;
};
typedef struct _CACHE_GLYPH_V2_ORDER CACHE_GLYPH_V2_ORDER;

struct _CACHE_BRUSH_ORDER
{
	UINT32 index;
	UINT32 bpp;
	UINT32 cx;
	UINT32 cy;
	UINT32 style;
	UINT32 length;
	BYTE data[256];
};
typedef struct _CACHE_BRUSH_ORDER CACHE_BRUSH_ORDER;

typedef void (*pCacheBitmap)(rdpContext* context, CACHE_BITMAP_ORDER* cache_bitmap_order);
typedef void (*pCacheBitmapV2)(rdpContext* context, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2_order);
typedef void (*pCacheBitmapV3)(rdpContext* context, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3_order);
typedef void (*pCacheColorTable)(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cache_color_table_order);
typedef void (*pCacheGlyph)(rdpContext* context, CACHE_GLYPH_ORDER* cache_glyph_order);
typedef void (*pCacheGlyphV2)(rdpContext* context, CACHE_GLYPH_V2_ORDER* cache_glyph_v2_order);
typedef void (*pCacheBrush)(rdpContext* context, CACHE_BRUSH_ORDER* cache_brush_order);

struct rdp_secondary_update
{
	rdpContext* context; /* 0 */
	UINT32 paddingA[16 - 1]; /* 1 */

	pCacheBitmap CacheBitmap; /* 16 */
	pCacheBitmapV2 CacheBitmapV2; /* 17 */
	pCacheBitmapV3 CacheBitmapV3; /* 18 */
	pCacheColorTable CacheColorTable; /* 19 */
	pCacheGlyph CacheGlyph; /* 20 */
	pCacheGlyphV2 CacheGlyphV2; /* 21 */
	pCacheBrush CacheBrush; /* 22 */
	UINT32 paddingE[32 - 23]; /* 23 */

	/* internal */

	BOOL glyph_v2;
	CACHE_BITMAP_ORDER cache_bitmap_order;
	CACHE_BITMAP_V2_ORDER cache_bitmap_v2_order;
	CACHE_BITMAP_V3_ORDER cache_bitmap_v3_order;
	CACHE_COLOR_TABLE_ORDER cache_color_table_order;
	CACHE_GLYPH_ORDER cache_glyph_order;
	CACHE_GLYPH_V2_ORDER cache_glyph_v2_order;
	CACHE_BRUSH_ORDER cache_brush_order;
};
typedef struct rdp_secondary_update rdpSecondaryUpdate;

#endif /* FREERDP_UPDATE_SECONDARY_H */
