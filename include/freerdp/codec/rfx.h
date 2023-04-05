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

	typedef struct S_RFX_MESSAGE_LIST RFX_MESSAGE_LIST;
	typedef struct S_RFX_MESSAGE RFX_MESSAGE;
	typedef struct S_RFX_CONTEXT RFX_CONTEXT;

	FREERDP_API BOOL rfx_process_message(RFX_CONTEXT* context, const BYTE* data, UINT32 length,
	                                     UINT32 left, UINT32 top, BYTE* dst, UINT32 dstFormat,
	                                     UINT32 dstStride, UINT32 dstHeight,
	                                     REGION16* invalidRegion);

	FREERDP_API UINT32 rfx_message_get_frame_idx(const RFX_MESSAGE* message);
	FREERDP_API const UINT32* rfx_message_get_quants(const RFX_MESSAGE* message,
	                                                 UINT16* numQuantVals);

	FREERDP_API const RFX_TILE** rfx_message_get_tiles(const RFX_MESSAGE* message,
	                                                   UINT16* numTiles);
	FREERDP_API UINT16 rfx_message_get_tile_count(const RFX_MESSAGE* message);

	FREERDP_API const RFX_RECT* rfx_message_get_rects(const RFX_MESSAGE* message, UINT16* numRects);
	FREERDP_API UINT16 rfx_message_get_rect_count(const RFX_MESSAGE* message);

	FREERDP_API void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message);

	FREERDP_API BOOL rfx_compose_message(RFX_CONTEXT* context, wStream* s, const RFX_RECT* rects,
	                                     size_t num_rects, const BYTE* image_data, UINT32 width,
	                                     UINT32 height, UINT32 rowstride);

	FREERDP_API RFX_MESSAGE* rfx_encode_message(RFX_CONTEXT* context, const RFX_RECT* rects,
	                                            size_t numRects, const BYTE* data, UINT32 width,
	                                            UINT32 height, size_t scanline);

	FREERDP_API RFX_MESSAGE_LIST* rfx_encode_messages(RFX_CONTEXT* context, const RFX_RECT* rects,
	                                                  size_t numRects, const BYTE* data,
	                                                  UINT32 width, UINT32 height, UINT32 scanline,
	                                                  size_t* numMessages, size_t maxDataSize);
	FREERDP_API void rfx_message_list_free(RFX_MESSAGE_LIST* messages);

	FREERDP_API const RFX_MESSAGE* rfx_message_list_get(const RFX_MESSAGE_LIST* messages,
	                                                    size_t idx);

	FREERDP_API BOOL rfx_write_message(RFX_CONTEXT* context, wStream* s,
	                                   const RFX_MESSAGE* message);

	FREERDP_API RFX_CONTEXT* rfx_context_new_ex(BOOL encoder, UINT32 ThreadingFlags);
	FREERDP_API RFX_CONTEXT* rfx_context_new(BOOL encoder);
	FREERDP_API void rfx_context_free(RFX_CONTEXT* context);

	FREERDP_API BOOL rfx_context_reset(RFX_CONTEXT* context, UINT32 width, UINT32 height);
	FREERDP_API BOOL rfx_context_set_mode(RFX_CONTEXT* context, RLGR_MODE mode);
	FREERDP_API void rfx_context_set_pixel_format(RFX_CONTEXT* context, UINT32 pixel_format);
	FREERDP_API UINT32 rfx_context_get_frame_idx(const RFX_CONTEXT* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_REMOTEFX_H */
