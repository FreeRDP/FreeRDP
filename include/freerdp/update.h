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

typedef struct rdp_update rdpUpdate;

#include <freerdp/rail.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/graphics.h>
#include <freerdp/utils/pcap.h>
#include <freerdp/utils/stream.h>

#include <freerdp/primary.h>
#include <freerdp/secondary.h>
#include <freerdp/altsec.h>
#include <freerdp/window.h>
#include <freerdp/pointer.h>

/* Bitmap Updates */

struct _BITMAP_DATA
{
	uint16 destLeft;
	uint16 destTop;
	uint16 destRight;
	uint16 destBottom;
	uint16 width;
	uint16 height;
	uint16 bitsPerPixel;
	uint16 flags;
	uint16 bitmapLength;
	uint8 bitmapComprHdr[8];
	uint8* bitmapDataStream;
	boolean compressed;
};
typedef struct _BITMAP_DATA BITMAP_DATA;

struct _BITMAP_UPDATE
{
	uint16 count;
	uint16 number;
	BITMAP_DATA* rectangles;
};
typedef struct _BITMAP_UPDATE BITMAP_UPDATE;

/* Palette Updates */

struct _PALETTE_ENTRY
{
	uint8 red;
	uint8 green;
	uint8 blue;
};
typedef struct _PALETTE_ENTRY PALETTE_ENTRY;

struct _PALETTE_UPDATE
{
	uint32 number;
	PALETTE_ENTRY entries[256];
};
typedef struct _PALETTE_UPDATE PALETTE_UPDATE;

struct rdp_palette
{
	uint16 count;
	PALETTE_ENTRY* entries;
};
typedef struct rdp_palette rdpPalette;

/* Play Sound (System Beep) Updates */

struct _PLAY_SOUND_UPDATE
{
	uint32 duration;
	uint32 frequency;
};
typedef struct _PLAY_SOUND_UPDATE PLAY_SOUND_UPDATE;

/* Surface Command Updates */

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

/* Update Interface */

typedef void (*pBeginPaint)(rdpUpdate* update);
typedef void (*pEndPaint)(rdpUpdate* update);
typedef void (*pSetBounds)(rdpUpdate* update, BOUNDS* bounds);
typedef void (*pSynchronize)(rdpUpdate* update);
typedef void (*pDesktopResize)(rdpUpdate* update);
typedef void (*pBitmapUpdate)(rdpUpdate* update, BITMAP_UPDATE* bitmap);
typedef void (*pPalette)(rdpUpdate* update, PALETTE_UPDATE* palette);
typedef void (*pPlaySound)(rdpUpdate* update, PLAY_SOUND_UPDATE* play_sound);

typedef void (*pRefreshRect)(rdpUpdate* update, uint8 count, RECTANGLE_16* areas);
typedef void (*pSuppressOutput)(rdpUpdate* update, uint8 allow, RECTANGLE_16* area);

typedef void (*pSurfaceCommand)(rdpUpdate* update, STREAM* s);
typedef void (*pSurfaceBits)(rdpUpdate* update, SURFACE_BITS_COMMAND* surface_bits_command);

struct rdp_update
{
	rdpContext* context; /* 0 */
	uint32 paddingA[16 - 1]; /* 1 */

	pBeginPaint BeginPaint; /* 16 */
	pEndPaint EndPaint; /* 17 */
	pSetBounds SetBounds; /* 18 */
	pSynchronize Synchronize; /* 19 */
	pDesktopResize DesktopResize; /* 20 */
	pBitmapUpdate BitmapUpdate; /* 21 */
	pPalette Palette; /* 22 */
	pPlaySound PlaySound; /* 23 */
	uint32 paddingB[32 - 24]; /* 24 */

	rdpPointerUpdate* pointer; /* 32 */
	uint32 paddingC[48 - 33]; /* 33 */

	rdpPrimaryUpdate* primary; /* 48 */
	uint32 paddingD[80 - 49]; /* 49 */

	pCacheBitmap CacheBitmap; /* 80 */
	pCacheBitmapV2 CacheBitmapV2; /* 81 */
	pCacheBitmapV3 CacheBitmapV3; /* 82 */
	pCacheColorTable CacheColorTable; /* 83 */
	pCacheGlyph CacheGlyph; /* 84 */
	pCacheGlyphV2 CacheGlyphV2; /* 85 */
	pCacheBrush CacheBrush; /* 86 */
	uint32 paddingE[112 - 87]; /* 87 */

	pCreateOffscreenBitmap CreateOffscreenBitmap; /* 112 */
	pSwitchSurface SwitchSurface; /* 113 */
	pCreateNineGridBitmap CreateNineGridBitmap; /* 114 */
	pFrameMarker FrameMarker; /* 115 */
	pStreamBitmapFirst StreamBitmapFirst; /* 116 */
	pStreamBitmapNext StreamBitmapNext; /* 117 */
	pDrawGdiPlusFirst DrawGdiPlusFirst; /* 118 */
	pDrawGdiPlusNext DrawGdiPlusNext; /* 119 */
	pDrawGdiPlusEnd DrawGdiPlusEnd; /* 120 */
	pDrawGdiPlusCacheFirst DrawGdiPlusCacheFirst; /* 121 */
	pDrawGdiPlusCacheNext DrawGdiPlusCacheNext; /* 122 */
	pDrawGdiPlusCacheEnd DrawGdiPlusCacheEnd; /* 123 */
	uint32 paddingF[144 - 124]; /* 124 */

	pWindowCreate WindowCreate; /* 144 */
	pWindowUpdate WindowUpdate; /* 145 */
	pWindowIcon WindowIcon; /* 146 */
	pWindowCachedIcon WindowCachedIcon; /* 147 */
	pWindowDelete WindowDelete; /* 148 */
	pNotifyIconCreate NotifyIconCreate; /* 149 */
	pNotifyIconUpdate NotifyIconUpdate; /* 150 */
	pNotifyIconDelete NotifyIconDelete; /* 151 */
	pMonitoredDesktop MonitoredDesktop; /* 152 */
	pNonMonitoredDesktop NonMonitoredDesktop; /* 153 */
	uint32 paddingG[176 - 154]; /* 154 */

	pRefreshRect RefreshRect; /* 176 */
	pSuppressOutput SuppressOutput; /* 177 */
	uint32 paddingH[192 - 178]; /* 178 */

	pSurfaceCommand SurfaceCommand; /* 192 */
	pSurfaceBits SurfaceBits; /* 193 */
	uint32 paddingI[208 - 194]; /* 194 */

	/* everything below is internal, and should not be directly accessed */

	boolean glyph_v2;

	boolean dump_rfx;
	boolean play_rfx;
	rdpPcap* pcap_rfx;

	BITMAP_UPDATE bitmap_update;
	PALETTE_UPDATE palette_update;
	PLAY_SOUND_UPDATE play_sound;

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

