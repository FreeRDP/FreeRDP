/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_UPDATE_H
#define FREERDP_UPDATE_H

typedef struct rdp_update rdpUpdate;

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/rail.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/graphics.h>
#include <freerdp/utils/pcap.h>

#include <freerdp/primary.h>
#include <freerdp/secondary.h>
#include <freerdp/altsec.h>
#include <freerdp/window.h>
#include <freerdp/pointer.h>

/* Bitmap Updates */

struct _BITMAP_DATA
{
	UINT32 destLeft;
	UINT32 destTop;
	UINT32 destRight;
	UINT32 destBottom;
	UINT32 width;
	UINT32 height;
	UINT32 bitsPerPixel;
	UINT32 flags;
	UINT32 bitmapLength;
	UINT32 cbCompFirstRowSize;
	UINT32 cbCompMainBodySize;
	UINT32 cbScanWidth;
	UINT32 cbUncompressedSize;
	BYTE* bitmapDataStream;
	BOOL compressed;
};
typedef struct _BITMAP_DATA BITMAP_DATA;

struct _BITMAP_UPDATE
{
	UINT32 count;
	UINT32 number;
	BITMAP_DATA* rectangles;
};
typedef struct _BITMAP_UPDATE BITMAP_UPDATE;

/* Palette Updates */

struct _PALETTE_ENTRY
{
	BYTE red;
	BYTE green;
	BYTE blue;
};
typedef struct _PALETTE_ENTRY PALETTE_ENTRY;

struct _PALETTE_UPDATE
{
	UINT32 number;
	PALETTE_ENTRY entries[256];
};
typedef struct _PALETTE_UPDATE PALETTE_UPDATE;

struct rdp_palette
{
	UINT32 count;
	PALETTE_ENTRY entries[256];
};
typedef struct rdp_palette rdpPalette;

/* Play Sound (System Beep) Updates */

struct _PLAY_SOUND_UPDATE
{
	UINT32 duration;
	UINT32 frequency;
};
typedef struct _PLAY_SOUND_UPDATE PLAY_SOUND_UPDATE;

/* Surface Command Updates */

struct _SURFACE_BITS_COMMAND
{
	UINT32 cmdType;
	UINT32 destLeft;
	UINT32 destTop;
	UINT32 destRight;
	UINT32 destBottom;
	UINT32 bpp;
	UINT32 codecID;
	UINT32 width;
	UINT32 height;
	UINT32 bitmapDataLength;
	BYTE* bitmapData;
};
typedef struct _SURFACE_BITS_COMMAND SURFACE_BITS_COMMAND;

struct _SURFACE_FRAME_MARKER
{
	UINT32 frameAction;
	UINT32 frameId;
};
typedef struct _SURFACE_FRAME_MARKER SURFACE_FRAME_MARKER;

enum SURFCMD_FRAMEACTION
{
	SURFACECMD_FRAMEACTION_BEGIN = 0x0000,
	SURFACECMD_FRAMEACTION_END = 0x0001
};

/* defined inside libfreerdp-core */
typedef struct rdp_update_proxy rdpUpdateProxy;

/* Update Interface */

typedef void (*pBeginPaint)(rdpContext* context);
typedef void (*pEndPaint)(rdpContext* context);
typedef void (*pSetBounds)(rdpContext* context, rdpBounds* bounds);

typedef void (*pSynchronize)(rdpContext* context);
typedef void (*pDesktopResize)(rdpContext* context);
typedef void (*pBitmapUpdate)(rdpContext* context, BITMAP_UPDATE* bitmap);
typedef void (*pPalette)(rdpContext* context, PALETTE_UPDATE* palette);
typedef void (*pPlaySound)(rdpContext* context, PLAY_SOUND_UPDATE* play_sound);

typedef void (*pRefreshRect)(rdpContext* context, BYTE count, RECTANGLE_16* areas);
typedef void (*pSuppressOutput)(rdpContext* context, BYTE allow, RECTANGLE_16* area);

typedef void (*pSurfaceCommand)(rdpContext* context, wStream* s);
typedef void (*pSurfaceBits)(rdpContext* context, SURFACE_BITS_COMMAND* surface_bits_command);
typedef void (*pSurfaceFrameMarker)(rdpContext* context, SURFACE_FRAME_MARKER* surface_frame_marker);
typedef void (*pSurfaceFrameAcknowledge)(rdpContext* context, UINT32 frameId);

struct rdp_update
{
	rdpContext* context; /* 0 */
	UINT32 paddingA[16 - 1]; /* 1 */

	pBeginPaint BeginPaint; /* 16 */
	pEndPaint EndPaint; /* 17 */
	pSetBounds SetBounds; /* 18 */
	pSynchronize Synchronize; /* 19 */
	pDesktopResize DesktopResize; /* 20 */
	pBitmapUpdate BitmapUpdate; /* 21 */
	pPalette Palette; /* 22 */
	pPlaySound PlaySound; /* 23 */
	UINT32 paddingB[32 - 24]; /* 24 */

	rdpPointerUpdate* pointer; /* 32 */
	rdpPrimaryUpdate* primary; /* 33 */
	rdpSecondaryUpdate* secondary; /* 34 */
	rdpAltSecUpdate* altsec; /* 35 */
	rdpWindowUpdate* window; /* 36 */
	UINT32 paddingC[48 - 37]; /* 37 */

	pRefreshRect RefreshRect; /* 48 */
	pSuppressOutput SuppressOutput; /* 49 */
	UINT32 paddingD[64 - 50]; /* 50 */

	pSurfaceCommand SurfaceCommand; /* 64 */
	pSurfaceBits SurfaceBits; /* 65 */
	pSurfaceFrameMarker SurfaceFrameMarker; /* 66 */
	pSurfaceFrameAcknowledge SurfaceFrameAcknowledge; /* 67 */
	UINT32 paddingE[80 - 68]; /* 68 */

	/* internal */

	BOOL dump_rfx;
	BOOL play_rfx;
	rdpPcap* pcap_rfx;
	BOOL initialState;

	BITMAP_UPDATE bitmap_update;
	PALETTE_UPDATE palette_update;
	PLAY_SOUND_UPDATE play_sound;

	SURFACE_BITS_COMMAND surface_bits_command;
	SURFACE_FRAME_MARKER surface_frame_marker;

	BOOL asynchronous;
	rdpUpdateProxy* proxy;
	wMessageQueue* queue;

	wStream* us;
	UINT16 numberOrders;
	BOOL combineUpdates;
	rdpBounds currentBounds;
	rdpBounds previousBounds;
};

#endif /* FREERDP_UPDATE_H */
