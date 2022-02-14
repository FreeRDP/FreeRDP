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
#include <winpr/wlog.h>
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
#define EX_COMPRESSED_BITMAP_HEADER_PRESENT 0x01

typedef struct
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
} BITMAP_DATA;

typedef struct
{
	UINT32 number;
	BITMAP_DATA* rectangles;
	BOOL skipCompression;
} BITMAP_UPDATE;

/* Palette Updates */

typedef struct
{
	UINT32 number;
	PALETTE_ENTRY entries[256];
} PALETTE_UPDATE;

/* Play Sound (System Beep) Updates */

typedef struct
{
	UINT32 duration;
	UINT32 frequency;
} PLAY_SOUND_UPDATE;

/* Surface Command Updates */
typedef struct
{
	UINT32 highUniqueId;
	UINT32 lowUniqueId;
	UINT64 tmMilliseconds;
	UINT64 tmSeconds;
} TS_COMPRESSED_BITMAP_HEADER_EX;

typedef struct
{
	BYTE bpp;
	BYTE flags;
	UINT16 codecID;
	UINT16 width;
	UINT16 height;
	UINT32 bitmapDataLength;
	TS_COMPRESSED_BITMAP_HEADER_EX exBitmapDataHeader;
	BYTE* bitmapData;
} TS_BITMAP_DATA_EX;

enum SURFCMD_CMDTYPE
{
	CMDTYPE_SET_SURFACE_BITS = 0x0001,
	CMDTYPE_FRAME_MARKER = 0x0004,
	CMDTYPE_STREAM_SURFACE_BITS = 0x0006
};

typedef struct
{
	UINT32 cmdType;
	UINT32 destLeft;
	UINT32 destTop;
	UINT32 destRight;
	UINT32 destBottom;
	TS_BITMAP_DATA_EX bmp;
	BOOL skipCompression;
} SURFACE_BITS_COMMAND;

typedef struct
{
	UINT32 frameAction;
	UINT32 frameId;
} SURFACE_FRAME_MARKER;

enum SURFCMD_FRAMEACTION
{
	SURFACECMD_FRAMEACTION_BEGIN = 0x0000,
	SURFACECMD_FRAMEACTION_END = 0x0001
};

/** @brief status code as in 2.2.5.2 Server Status Info PDU */
enum
{
	TS_STATUS_FINDING_DESTINATION = 0x00000401,
	TS_STATUS_LOADING_DESTINATION = 0x00000402,
	TS_STATUS_BRINGING_SESSION_ONLINE = 0x00000403,
	TS_STATUS_REDIRECTING_TO_DESTINATION = 0x00000404,
	TS_STATUS_VM_LOADING = 0x00000501,
	TS_STATUS_VM_WAKING = 0x00000502,
	TS_STATUS_VM_STARTING = 0x00000503,
	TS_STATUS_VM_STARTING_MONITORING = 0x00000504,
	TS_STATUS_VM_RETRYING_MONITORING = 0x00000505
};

typedef struct
{
	UINT32 frameId;
	UINT32 commandCount;
	SURFACE_BITS_COMMAND* commands;
} SURFACE_FRAME;

/* defined inside libfreerdp-core */
typedef struct rdp_update_proxy rdpUpdateProxy;

/* Update Interface */

typedef BOOL (*pBeginPaint)(rdpContext* context);
typedef BOOL (*pEndPaint)(rdpContext* context);
typedef BOOL (*pSetBounds)(rdpContext* context, const rdpBounds* bounds);

typedef BOOL (*pSynchronize)(rdpContext* context);
typedef BOOL (*pDesktopResize)(rdpContext* context);
typedef BOOL (*pBitmapUpdate)(rdpContext* context, const BITMAP_UPDATE* bitmap);
typedef BOOL (*pPalette)(rdpContext* context, const PALETTE_UPDATE* palette);
typedef BOOL (*pPlaySound)(rdpContext* context, const PLAY_SOUND_UPDATE* play_sound);
typedef BOOL (*pSetKeyboardIndicators)(rdpContext* context, UINT16 led_flags);

typedef BOOL (*pRefreshRect)(rdpContext* context, BYTE count, const RECTANGLE_16* areas);
typedef BOOL (*pSuppressOutput)(rdpContext* context, BYTE allow, const RECTANGLE_16* area);
typedef BOOL (*pRemoteMonitors)(rdpContext* context, UINT32 count, const MONITOR_DEF* monitors);

typedef BOOL (*pSurfaceCommand)(rdpContext* context, wStream* s);
typedef BOOL (*pSurfaceBits)(rdpContext* context, const SURFACE_BITS_COMMAND* surfaceBitsCommand);
typedef BOOL (*pSurfaceFrameMarker)(rdpContext* context,
                                    const SURFACE_FRAME_MARKER* surfaceFrameMarker);
typedef BOOL (*pSurfaceFrameBits)(rdpContext* context, const SURFACE_BITS_COMMAND* cmd, BOOL first,
                                  BOOL last, UINT32 frameId);
typedef BOOL (*pSurfaceFrameAcknowledge)(rdpContext* context, UINT32 frameId);

typedef BOOL (*pSaveSessionInfo)(rdpContext* context, UINT32 type, void* data);
typedef BOOL (*pSetKeyboardImeStatus)(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                      UINT32 imeConvMode);
typedef BOOL (*pServerStatusInfo)(rdpContext* context, UINT32 status);

struct rdp_update
{
	rdpContext* context;     /* 0 */
	UINT32 paddingA[16 - 1]; /* 1 */

	pBeginPaint BeginPaint;                       /* 16 */
	pEndPaint EndPaint;                           /* 17 */
	pSetBounds SetBounds;                         /* 18 */
	pSynchronize Synchronize;                     /* 19 */
	pDesktopResize DesktopResize;                 /* 20 */
	pBitmapUpdate BitmapUpdate;                   /* 21 */
	pPalette Palette;                             /* 22 */
	pPlaySound PlaySound;                         /* 23 */
	pSetKeyboardIndicators SetKeyboardIndicators; /* 24 */
	pSetKeyboardImeStatus SetKeyboardImeStatus;   /* 25 */
	UINT32 paddingB[32 - 26];                     /* 26 */

	rdpPointerUpdate* pointer;     /* 32 */
	rdpPrimaryUpdate* primary;     /* 33 */
	rdpSecondaryUpdate* secondary; /* 34 */
	rdpAltSecUpdate* altsec;       /* 35 */
	rdpWindowUpdate* window;       /* 36 */
	UINT32 paddingC[48 - 37];      /* 37 */

	pRefreshRect RefreshRect;       /* 48 */
	pSuppressOutput SuppressOutput; /* 49 */
	pRemoteMonitors RemoteMonitors; /* 50 */
	UINT32 paddingD[64 - 51];       /* 51 */

	pSurfaceCommand SurfaceCommand;                   /* 64 */
	pSurfaceBits SurfaceBits;                         /* 65 */
	pSurfaceFrameMarker SurfaceFrameMarker;           /* 66 */
	pSurfaceFrameBits SurfaceFrameBits;               /* 67 */
	pSurfaceFrameAcknowledge SurfaceFrameAcknowledge; /* 68 */
	pSaveSessionInfo SaveSessionInfo;                 /* 69 */
	pServerStatusInfo ServerStatusInfo;               /* 70 */
	/* if autoCalculateBitmapData is set to TRUE, the server automatically
	 * fills BITMAP_DATA struct members: flags, cbCompMainBodySize and cbCompFirstRowSize.
	 */
	BOOL autoCalculateBitmapData; /* 71 */
	UINT32 paddingE[80 - 72];     /* 72 */
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API void rdp_update_lock(rdpUpdate* update);
	FREERDP_API void rdp_update_unlock(rdpUpdate* update);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UPDATE_H */
