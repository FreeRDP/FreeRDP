/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Alternate Secondary Drawing Orders Interface API
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

#ifndef __UPDATE_ALTSEC_H
#define __UPDATE_ALTSEC_H

#include <freerdp/types.h>

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

struct _OFFSCREEN_DELETE_LIST
{
	uint32 sIndices;
	uint32 cIndices;
	uint16* indices;
};
typedef struct _OFFSCREEN_DELETE_LIST OFFSCREEN_DELETE_LIST;

struct _CREATE_OFFSCREEN_BITMAP_ORDER
{
	uint32 id;
	uint32 cx;
	uint32 cy;
	OFFSCREEN_DELETE_LIST deleteList;
};
typedef struct _CREATE_OFFSCREEN_BITMAP_ORDER CREATE_OFFSCREEN_BITMAP_ORDER;

struct _SWITCH_SURFACE_ORDER
{
	uint32 bitmapId;
};
typedef struct _SWITCH_SURFACE_ORDER SWITCH_SURFACE_ORDER;

struct _NINE_GRID_BITMAP_INFO
{
	uint32 flFlags;
	uint32 ulLeftWidth;
	uint32 ulRightWidth;
	uint32 ulTopHeight;
	uint32 ulBottomHeight;
	uint32 crTransparent;
};
typedef struct _NINE_GRID_BITMAP_INFO NINE_GRID_BITMAP_INFO;

struct _CREATE_NINE_GRID_BITMAP_ORDER
{
	uint32 bitmapBpp;
	uint32 bitmapId;
	uint32 cx;
	uint32 cy;
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
	uint32 bitmapFlags;
	uint32 bitmapBpp;
	uint32 bitmapType;
	uint32 bitmapWidth;
	uint32 bitmapHeight;
	uint32 bitmapSize;
	uint32 bitmapBlockSize;
	uint8* bitmapBlock;
};
typedef struct _STREAM_BITMAP_FIRST_ORDER STREAM_BITMAP_FIRST_ORDER;

struct _STREAM_BITMAP_NEXT_ORDER
{
	uint32 bitmapFlags;
	uint32 bitmapType;
	uint32 bitmapBlockSize;
	uint8* bitmapBlock;
};
typedef struct _STREAM_BITMAP_NEXT_ORDER STREAM_BITMAP_NEXT_ORDER;

struct _DRAW_GDIPLUS_FIRST_ORDER
{
	uint32 cbSize;
	uint32 cbTotalSize;
	uint32 cbTotalEmfSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_FIRST_ORDER DRAW_GDIPLUS_FIRST_ORDER;

struct _DRAW_GDIPLUS_NEXT_ORDER
{
	uint32 cbSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_NEXT_ORDER DRAW_GDIPLUS_NEXT_ORDER;

struct _DRAW_GDIPLUS_END_ORDER
{
	uint32 cbSize;
	uint32 cbTotalSize;
	uint32 cbTotalEmfSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_END_ORDER DRAW_GDIPLUS_END_ORDER;

struct _DRAW_GDIPLUS_CACHE_FIRST_ORDER
{
	uint32 flags;
	uint32 cacheType;
	uint32 cacheIndex;
	uint32 cbSize;
	uint32 cbTotalSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_CACHE_FIRST_ORDER DRAW_GDIPLUS_CACHE_FIRST_ORDER;

struct _DRAW_GDIPLUS_CACHE_NEXT_ORDER
{
	uint32 flags;
	uint32 cacheType;
	uint32 cacheIndex;
	uint32 cbSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_CACHE_NEXT_ORDER DRAW_GDIPLUS_CACHE_NEXT_ORDER;

struct _DRAW_GDIPLUS_CACHE_END_ORDER
{
	uint32 flags;
	uint32 cacheType;
	uint32 cacheIndex;
	uint32 cbSize;
	uint32 cbTotalSize;
	uint8* emfRecords;
};
typedef struct _DRAW_GDIPLUS_CACHE_END_ORDER DRAW_GDIPLUS_CACHE_END_ORDER;

typedef void (*pCreateOffscreenBitmap)(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap);
typedef void (*pSwitchSurface)(rdpContext* context, SWITCH_SURFACE_ORDER* switch_surface);
typedef void (*pCreateNineGridBitmap)(rdpContext* context, CREATE_NINE_GRID_BITMAP_ORDER* create_nine_grid_bitmap);
typedef void (*pFrameMarker)(rdpContext* context, FRAME_MARKER_ORDER* frame_marker);
typedef void (*pStreamBitmapFirst)(rdpContext* context, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_first);
typedef void (*pStreamBitmapNext)(rdpContext* context, STREAM_BITMAP_FIRST_ORDER* stream_bitmap_next);
typedef void (*pDrawGdiPlusFirst)(rdpContext* context, DRAW_GDIPLUS_FIRST_ORDER* draw_gdiplus_first);
typedef void (*pDrawGdiPlusNext)(rdpContext* context, DRAW_GDIPLUS_NEXT_ORDER* draw_gdiplus_next);
typedef void (*pDrawGdiPlusEnd)(rdpContext* context, DRAW_GDIPLUS_END_ORDER* draw_gdiplus_end);
typedef void (*pDrawGdiPlusCacheFirst)(rdpContext* context, DRAW_GDIPLUS_CACHE_FIRST_ORDER* draw_gdiplus_cache_first);
typedef void (*pDrawGdiPlusCacheNext)(rdpContext* context, DRAW_GDIPLUS_CACHE_NEXT_ORDER* draw_gdiplus_cache_next);
typedef void (*pDrawGdiPlusCacheEnd)(rdpContext* context, DRAW_GDIPLUS_CACHE_END_ORDER* draw_gdiplus_cache_end);

struct rdp_altsec_update
{
	rdpContext* context; /* 0 */
	uint32 paddingA[16 - 1]; /* 1 */

	pCreateOffscreenBitmap CreateOffscreenBitmap; /* 16 */
	pSwitchSurface SwitchSurface; /* 17 */
	pCreateNineGridBitmap CreateNineGridBitmap; /* 18 */
	pFrameMarker FrameMarker; /* 19 */
	pStreamBitmapFirst StreamBitmapFirst; /* 20 */
	pStreamBitmapNext StreamBitmapNext; /* 21 */
	pDrawGdiPlusFirst DrawGdiPlusFirst; /* 22 */
	pDrawGdiPlusNext DrawGdiPlusNext; /* 23 */
	pDrawGdiPlusEnd DrawGdiPlusEnd; /* 24 */
	pDrawGdiPlusCacheFirst DrawGdiPlusCacheFirst; /* 25 */
	pDrawGdiPlusCacheNext DrawGdiPlusCacheNext; /* 26 */
	pDrawGdiPlusCacheEnd DrawGdiPlusCacheEnd; /* 27 */
	uint32 paddingB[32 - 28]; /* 28 */

	/* internal */

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
typedef struct rdp_altsec_update rdpAltSecUpdate;

#endif /* __UPDATE_ALTSEC_H */
