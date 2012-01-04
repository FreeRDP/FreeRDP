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
	uint32 destLeft;
	uint32 destTop;
	uint32 destRight;
	uint32 destBottom;
	uint32 width;
	uint32 height;
	uint32 bitsPerPixel;
	uint32 flags;
	uint32 bitmapLength;
	uint32 cbCompFirstRowSize;
	uint32 cbCompMainBodySize;
	uint32 cbScanWidth;
	uint32 cbUncompressedSize;
	uint8* bitmapDataStream;
	boolean compressed;
};
typedef struct _BITMAP_DATA BITMAP_DATA;

struct _BITMAP_UPDATE
{
	uint32 count;
	uint32 number;
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
	uint32 count;
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
	uint32 cmdType;
	uint32 destLeft;
	uint32 destTop;
	uint32 destRight;
	uint32 destBottom;
	uint32 bpp;
	uint32 codecID;
	uint32 width;
	uint32 height;
	uint32 bitmapDataLength;
	uint8* bitmapData;
};
typedef struct _SURFACE_BITS_COMMAND SURFACE_BITS_COMMAND;

struct _SURFACE_FRAME_MARKER
{
	uint32 frameAction;
	uint32 frameId;
};
typedef struct _SURFACE_FRAME_MARKER SURFACE_FRAME_MARKER;

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
typedef void (*pSurfaceFrameMarker)(rdpContext* context, SURFACE_FRAME_MARKER* surface_frame_marker);

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
	rdpAltSecUpdate* altsec; /* 35 */
	rdpWindowUpdate* window; /* 36 */
	uint32 paddingC[48 - 37]; /* 37 */

	pRefreshRect RefreshRect; /* 48 */
	pSuppressOutput SuppressOutput; /* 49 */
	uint32 paddingD[64 - 50]; /* 50 */

	pSurfaceCommand SurfaceCommand; /* 64 */
	pSurfaceBits SurfaceBits; /* 65 */
	pSurfaceFrameMarker SurfaceFrameMarker; /* 66 */
	uint32 paddingE[80 - 67]; /* 67 */

	/* internal */

	boolean dump_rfx;
	boolean play_rfx;
	rdpPcap* pcap_rfx;

	BITMAP_UPDATE bitmap_update;
	PALETTE_UPDATE palette_update;
	PLAY_SOUND_UPDATE play_sound;

	SURFACE_BITS_COMMAND surface_bits_command;
	SURFACE_FRAME_MARKER surface_frame_marker;
};

#endif /* __UPDATE_API_H */

