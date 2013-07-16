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

#include <freerdp/api.h>
#include <freerdp/types.h>
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
typedef struct _RFX_RECT RFX_RECT;

struct _RFX_TILE
{
	UINT16 x;
	UINT16 y;
	BYTE* data;
};
typedef struct _RFX_TILE RFX_TILE;

struct _RFX_MESSAGE
{
	/**
	 * The rects array represents the updated region of the frame. The UI
	 * requires to clip drawing destination base on the union of the rects.
	 */
	UINT16 num_rects;
	RFX_RECT* rects;

	/**
	 * The tiles array represents the actual frame data. Each tile is always
	 * 64x64. Note that only pixels inside the updated region (represented as
	 * rects described above) are valid. Pixels outside of the region may
	 * contain arbitrary data.
	 */
	UINT16 num_tiles;
	RFX_TILE** tiles;
};
typedef struct _RFX_MESSAGE RFX_MESSAGE;

typedef struct _RFX_CONTEXT_PRIV RFX_CONTEXT_PRIV;

struct _RFX_CONTEXT
{
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
	UINT32 frame_idx;
	BOOL header_processed;
	BYTE num_quants;
	UINT32* quants;
	BYTE quant_idx_y;
	BYTE quant_idx_cb;
	BYTE quant_idx_cr;

	/* routines */
	void (*quantization_decode)(INT16* buffer, const UINT32* quantization_values);
	void (*quantization_encode)(INT16* buffer, const UINT32* quantization_values);
	void (*dwt_2d_decode)(INT16* buffer, INT16* dwt_buffer);
	void (*dwt_2d_encode)(INT16* buffer, INT16* dwt_buffer);
	int (*rlgr_decode)(RLGR_MODE mode, const BYTE* data, int data_size, INT16* buffer, int buffer_size);
	int (*rlgr_encode)(RLGR_MODE mode, const INT16* data, int data_size, BYTE* buffer, int buffer_size);

	/* private definitions */
	RFX_CONTEXT_PRIV* priv;
};
typedef struct _RFX_CONTEXT RFX_CONTEXT;

FREERDP_API RFX_CONTEXT* rfx_context_new(void);
FREERDP_API void rfx_context_free(RFX_CONTEXT* context);
FREERDP_API void rfx_context_set_pixel_format(RFX_CONTEXT* context, RDP_PIXEL_FORMAT pixel_format);
FREERDP_API void rfx_context_reset(RFX_CONTEXT* context);

FREERDP_API RFX_MESSAGE* rfx_process_message(RFX_CONTEXT* context, BYTE* data, UINT32 length);
FREERDP_API UINT16 rfx_message_get_tile_count(RFX_MESSAGE* message);
FREERDP_API RFX_TILE* rfx_message_get_tile(RFX_MESSAGE* message, int index);
FREERDP_API UINT16 rfx_message_get_rect_count(RFX_MESSAGE* message);
FREERDP_API RFX_RECT* rfx_message_get_rect(RFX_MESSAGE* message, int index);
FREERDP_API void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message);

FREERDP_API void rfx_compose_message_header(RFX_CONTEXT* context, wStream* s);
FREERDP_API void rfx_compose_message(RFX_CONTEXT* context, wStream* s,
	const RFX_RECT* rects, int num_rects, BYTE* image_data, int width, int height, int rowstride);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_REMOTEFX_H */
