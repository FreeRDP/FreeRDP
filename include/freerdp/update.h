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

typedef void (*pBeginPaint)(rdpContext* context);
typedef void (*pEndPaint)(rdpContext* context);
typedef void (*pSetBounds)(rdpContext* context, rdpBounds* bounds);

typedef void (*pSynchronize)(rdpContext* context);
typedef void (*pDesktopResize)(rdpContext* context);
typedef void (*pBitmapUpdate)(rdpContext* context, BITMAP_UPDATE* bitmap);
typedef void (*pPalette)(rdpContext* context, PALETTE_UPDATE* palette);
typedef void (*pPlaySound)(rdpContext* context, PLAY_SOUND_UPDATE* play_sound);

typedef void (*pRefreshRect)(rdpContext* context, uint8 count, RECTANGLE_16* areas);
typedef void (*pSuppressOutput)(rdpContext* context, uint8 allow, RECTANGLE_16* area);

typedef void (*pSurfaceCommand)(rdpContext* context, STREAM* s);
typedef void (*pSurfaceBits)(rdpContext* context, SURFACE_BITS_COMMAND* surface_bits_command);

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
	rdpPrimaryUpdate* primary; /* 33 */
	rdpSecondaryUpdate* secondary; /* 34 */

	pCreateOffscreenBitmap CreateOffscreenBitmap;
	pSwitchSurface SwitchSurface;
	pCreateNineGridBitmap CreateNineGridBitmap;
	pFrameMarker FrameMarker;
	pStreamBitmapFirst StreamBitmapFirst;
	pStreamBitmapNext StreamBitmapNext;
	pDrawGdiPlusFirst DrawGdiPlusFirst;
	pDrawGdiPlusNext DrawGdiPlusNext;
	pDrawGdiPlusEnd DrawGdiPlusEnd;
	pDrawGdiPlusCacheFirst DrawGdiPlusCacheFirst;
	pDrawGdiPlusCacheNext DrawGdiPlusCacheNext;
	pDrawGdiPlusCacheEnd DrawGdiPlusCacheEnd;

	pWindowCreate WindowCreate;
	pWindowUpdate WindowUpdate;
	pWindowIcon WindowIcon;
	pWindowCachedIcon WindowCachedIcon;
	pWindowDelete WindowDelete;
	pNotifyIconCreate NotifyIconCreate;
	pNotifyIconUpdate NotifyIconUpdate;
	pNotifyIconDelete NotifyIconDelete;
	pMonitoredDesktop MonitoredDesktop;
	pNonMonitoredDesktop NonMonitoredDesktop;

	pRefreshRect RefreshRect;
	pSuppressOutput SuppressOutput;

	pSurfaceCommand SurfaceCommand;
	pSurfaceBits SurfaceBits;

	/* internal */

	boolean dump_rfx;
	boolean play_rfx;
	rdpPcap* pcap_rfx;

	BITMAP_UPDATE bitmap_update;
	PALETTE_UPDATE palette_update;
	PLAY_SOUND_UPDATE play_sound;

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

