/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec
 *
 * Copyright 2011 Vic Lee
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

#ifndef FREERDP_CODEC_REMOTEFX_H
#define FREERDP_CODEC_REMOTEFX_H

typedef struct _RFX_RECT RFX_RECT;
typedef struct _RFX_TILE RFX_TILE;
typedef struct _RFX_MESSAGE RFX_MESSAGE;
typedef struct _RFX_CONTEXT RFX_CONTEXT;

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>

#include <winpr/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

enum _RLGR_MODE
{
	RLGR1,
	RLGR3
};
typedef enum _RLGR_MODE RLGR_MODE;

struct _RFX_RECT
{
	UINT16 x;
	UINT16 y;
	UINT16 width;
	UINT16 height;
};

struct _RFX_TILE
{
	UINT16 x;
	UINT16 y;
	int width;
	int height;
	BYTE* data;
	int scanline;
	BOOL allocated;
	BYTE quantIdxY;
	BYTE quantIdxCb;
	BYTE quantIdxCr;
	UINT16 xIdx;
	UINT16 yIdx;
	UINT16 YLen;
	UINT16 CbLen;
	UINT16 CrLen;
	BYTE* YData;
	BYTE* CbData;
	BYTE* CrData;
	BYTE* YCbCrData;
};

struct _RFX_MESSAGE
{
	UINT32 frameIdx;

	/**
	 * The rects array represents the updated region of the frame. The UI
	 * requires to clip drawing destination base on the union of the rects.
	 */
	UINT16 numRects;
	RFX_RECT* rects;
	BOOL freeRects;

	/**
	 * The tiles array represents the actual frame data. Each tile is always
	 * 64x64. Note that only pixels inside the updated region (represented as
	 * rects described above) are valid. Pixels outside of the region may
	 * contain arbitrary data.
	 */
	UINT16 numTiles;
	RFX_TILE** tiles;

	UINT16 numQuant;
	UINT32* quantVals;

	UINT32 tilesDataSize;

	BOOL freeArray;
};

typedef struct _RFX_CONTEXT_PRIV RFX_CONTEXT_PRIV;

enum _RFX_STATE
{
	RFX_STATE_INITIAL,
	RFX_STATE_SERVER_UNINITIALIZED,
	RFX_STATE_SEND_HEADERS,
	RFX_STATE_SEND_FRAME_DATA,
	RFX_STATE_FRAME_DATA_SENT,
	RFX_STATE_FINAL
};
typedef enum _RFX_STATE RFX_STATE;

#define _RFX_DECODED_SYNC       0x00000001
#define _RFX_DECODED_CONTEXT    0x00000002
#define _RFX_DECODED_VERSIONS   0x00000004
#define _RFX_DECODED_CHANNELS   0x00000008
#define _RFX_DECODED_HEADERS    0x0000000F


struct _RFX_CONTEXT
{
	RFX_STATE state;

	BOOL encoder;
	UINT16 flags;
	UINT16 properties;
	UINT16 width;
	UINT16 height;
	RLGR_MODE mode;
	UINT32 version;
	UINT32 codec_id;
	UINT32 codec_version;
	RDP_PIXEL_FORMAT pixel_format;
	BYTE bits_per_pixel;

	/* color palette allocated by the application */
	const BYTE* palette;

	/* temporary data within a frame */
	UINT32 frameIdx;
	BYTE numQuant;
	UINT32* quants;
	BYTE quantIdxY;
	BYTE quantIdxCb;
	BYTE quantIdxCr;

	/* decoded header blocks */
	UINT32 decodedHeaderBlocks;

	/* routines */
	void (*quantization_decode)(INT16* buffer, const UINT32* quantization_values);
	void (*quantization_encode)(INT16* buffer, const UINT32* quantization_values);
	void (*dwt_2d_decode)(INT16* buffer, INT16* dwt_buffer);
	void (*dwt_2d_encode)(INT16* buffer, INT16* dwt_buffer);

	/* private definitions */
	RFX_CONTEXT_PRIV* priv;
};

FREERDP_API void rfx_context_set_pixel_format(RFX_CONTEXT* context, RDP_PIXEL_FORMAT pixel_format);

FREERDP_API int rfx_rlgr_decode(const BYTE* pSrcData, UINT32 SrcSize, INT16* pDstData, UINT32 DstSize, int mode);

FREERDP_API RFX_MESSAGE* rfx_process_message(RFX_CONTEXT* context, BYTE* data, UINT32 length);
FREERDP_API UINT16 rfx_message_get_tile_count(RFX_MESSAGE* message);
FREERDP_API RFX_TILE* rfx_message_get_tile(RFX_MESSAGE* message, int index);
FREERDP_API UINT16 rfx_message_get_rect_count(RFX_MESSAGE* message);
FREERDP_API RFX_RECT* rfx_message_get_rect(RFX_MESSAGE* message, int index);
FREERDP_API void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message);

FREERDP_API BOOL rfx_compose_message(RFX_CONTEXT* context, wStream* s,
	const RFX_RECT* rects, int num_rects, BYTE* image_data, int width, int height, int rowstride);

FREERDP_API RFX_MESSAGE* rfx_encode_message(RFX_CONTEXT* context, const RFX_RECT* rects,
		int numRects, BYTE* data, int width, int height, int scanline);
FREERDP_API RFX_MESSAGE* rfx_encode_messages(RFX_CONTEXT* context, const RFX_RECT* rects, int numRects,
		BYTE* data, int width, int height, int scanline, int* numMessages, int maxDataSize);
FREERDP_API BOOL rfx_write_message(RFX_CONTEXT* context, wStream* s, RFX_MESSAGE* message);

FREERDP_API BOOL rfx_context_reset(RFX_CONTEXT* context, UINT32 width, UINT32 height);

FREERDP_API RFX_CONTEXT* rfx_context_new(BOOL encoder);
FREERDP_API void rfx_context_free(RFX_CONTEXT* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_REMOTEFX_H */
