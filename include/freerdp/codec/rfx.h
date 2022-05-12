/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec
 *
 * Copyright 2011 Vic Lee
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/codec/region.h>

#include <winpr/stream.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		RLGR1,
		RLGR3
	} RLGR_MODE;

	typedef struct
	{
		UINT16 x;
		UINT16 y;
		UINT16 width;
		UINT16 height;
	} RFX_RECT;

	typedef struct
	{
		UINT16 x;
		UINT16 y;
		UINT32 width;
		UINT32 height;
		BYTE* data;
		UINT32 scanline;
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
	} RFX_TILE;

	typedef struct
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
	} RFX_MESSAGE;

	typedef struct S_RFX_CONTEXT_PRIV RFX_CONTEXT_PRIV;

	typedef enum
	{
		RFX_STATE_INITIAL,
		RFX_STATE_SERVER_UNINITIALIZED,
		RFX_STATE_SEND_HEADERS,
		RFX_STATE_SEND_FRAME_DATA,
		RFX_STATE_FRAME_DATA_SENT,
		RFX_STATE_FINAL
	} RFX_STATE;

#define RFX_DECODED_SYNC 0x00000001
#define RFX_DECODED_CONTEXT 0x00000002
#define RFX_DECODED_VERSIONS 0x00000004
#define RFX_DECODED_CHANNELS 0x00000008
#define RFX_DECODED_HEADERS 0x0000000F

	typedef struct
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
		UINT32 pixel_format;
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
		UINT16 expectedDataBlockType;
		RFX_MESSAGE currentMessage;

		/* routines */
		void (*quantization_decode)(INT16* buffer, const UINT32* quantization_values);
		void (*quantization_encode)(INT16* buffer, const UINT32* quantization_values);
		void (*dwt_2d_decode)(INT16* buffer, INT16* dwt_buffer);
		void (*dwt_2d_encode)(INT16* buffer, INT16* dwt_buffer);
		int (*rlgr_decode)(RLGR_MODE mode, const BYTE* data, UINT32 data_size, INT16* buffer,
		                   UINT32 buffer_size);
		int (*rlgr_encode)(RLGR_MODE mode, const INT16* data, UINT32 data_size, BYTE* buffer,
		                   UINT32 buffer_size);

		/* private definitions */
		RFX_CONTEXT_PRIV* priv;
	} RFX_CONTEXT;

	FREERDP_API void rfx_context_set_pixel_format(RFX_CONTEXT* context, UINT32 pixel_format);

	FREERDP_API BOOL rfx_process_message(RFX_CONTEXT* context, const BYTE* data, UINT32 length,
	                                     UINT32 left, UINT32 top, BYTE* dst, UINT32 dstFormat,
	                                     UINT32 dstStride, UINT32 dstHeight,
	                                     REGION16* invalidRegion);
	FREERDP_API UINT16 rfx_message_get_tile_count(RFX_MESSAGE* message);
	FREERDP_API UINT16 rfx_message_get_rect_count(RFX_MESSAGE* message);
	FREERDP_API void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message);

	FREERDP_API BOOL rfx_compose_message(RFX_CONTEXT* context, wStream* s, const RFX_RECT* rects,
	                                     size_t num_rects, const BYTE* image_data, UINT32 width,
	                                     UINT32 height, UINT32 rowstride);

	FREERDP_API RFX_MESSAGE* rfx_encode_message(RFX_CONTEXT* context, const RFX_RECT* rects,
	                                            size_t numRects, const BYTE* data, UINT32 width,
	                                            UINT32 height, size_t scanline);

	FREERDP_API RFX_MESSAGE* rfx_encode_messages(RFX_CONTEXT* context, const RFX_RECT* rects,
	                                             size_t numRects, const BYTE* data, UINT32 width,
	                                             UINT32 height, UINT32 scanline,
	                                             size_t* numMessages, size_t maxDataSize);
	FREERDP_API BOOL rfx_write_message(RFX_CONTEXT* context, wStream* s,
	                                   const RFX_MESSAGE* message);

	FREERDP_API BOOL rfx_context_reset(RFX_CONTEXT* context, UINT32 width, UINT32 height);

	FREERDP_API RFX_CONTEXT* rfx_context_new_ex(BOOL encoder, UINT32 ThreadingFlags);
	FREERDP_API RFX_CONTEXT* rfx_context_new(BOOL encoder);
	FREERDP_API void rfx_context_free(RFX_CONTEXT* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_REMOTEFX_H */
