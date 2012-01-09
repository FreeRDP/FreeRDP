/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#ifndef __RFX_H
#define __RFX_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

enum _RLGR_MODE
{
	RLGR1,
	RLGR3
};
typedef enum _RLGR_MODE RLGR_MODE;

enum _RFX_PIXEL_FORMAT
{
	RFX_PIXEL_FORMAT_BGRA,
	RFX_PIXEL_FORMAT_RGBA,
	RFX_PIXEL_FORMAT_BGR,
	RFX_PIXEL_FORMAT_RGB,
	RFX_PIXEL_FORMAT_BGR565_LE,
	RFX_PIXEL_FORMAT_RGB565_LE,
	RFX_PIXEL_FORMAT_PALETTE4_PLANER,
	RFX_PIXEL_FORMAT_PALETTE8
};
typedef enum _RFX_PIXEL_FORMAT RFX_PIXEL_FORMAT;

struct _RFX_RECT
{
	uint16 x;
	uint16 y;
	uint16 width;
	uint16 height;
};
typedef struct _RFX_RECT RFX_RECT;

struct _RFX_TILE
{
	uint16 x;
	uint16 y;
	uint8* data;
};
typedef struct _RFX_TILE RFX_TILE;

struct _RFX_MESSAGE
{
	/**
	 * The rects array represents the updated region of the frame. The UI
	 * requires to clip drawing destination base on the union of the rects.
	 */
	uint16 num_rects;
	RFX_RECT* rects;

	/**
	 * The tiles array represents the actual frame data. Each tile is always
	 * 64x64. Note that only pixels inside the updated region (represented as
	 * rects described above) are valid. Pixels outside of the region may
	 * contain arbitrary data.
	 */
	uint16 num_tiles;
	RFX_TILE** tiles;
};
typedef struct _RFX_MESSAGE RFX_MESSAGE;

typedef struct _RFX_CONTEXT_PRIV RFX_CONTEXT_PRIV;

struct _RFX_CONTEXT
{
	uint16 flags;
	uint16 properties;
	uint16 width;
	uint16 height;
	RLGR_MODE mode;
	uint32 version;
	uint32 codec_id;
	uint32 codec_version;
	RFX_PIXEL_FORMAT pixel_format;
	uint8 bits_per_pixel;

	/* color palette allocated by the application */
	const uint8* palette;

	/* temporary data within a frame */
	uint32 frame_idx;
	boolean header_processed;
	uint8 num_quants;
	uint32* quants;
	uint8 quant_idx_y;
	uint8 quant_idx_cb;
	uint8 quant_idx_cr;

	/* routines */
	void (*decode_ycbcr_to_rgb)(sint16* y_r_buf, sint16* cb_g_buf, sint16* cr_b_buf);
	void (*encode_rgb_to_ycbcr)(sint16* y_r_buf, sint16* cb_g_buf, sint16* cr_b_buf);
	void (*quantization_decode)(sint16* buffer, const uint32* quantization_values);
	void (*quantization_encode)(sint16* buffer, const uint32* quantization_values);
	void (*dwt_2d_decode)(sint16* buffer, sint16* dwt_buffer);
	void (*dwt_2d_encode)(sint16* buffer, sint16* dwt_buffer);

	/* private definitions */
	RFX_CONTEXT_PRIV* priv;
};
typedef struct _RFX_CONTEXT RFX_CONTEXT;

FREERDP_API RFX_CONTEXT* rfx_context_new(void);
FREERDP_API void rfx_context_free(RFX_CONTEXT* context);
FREERDP_API void rfx_context_set_cpu_opt(RFX_CONTEXT* context, uint32 cpu_opt);
FREERDP_API void rfx_context_set_pixel_format(RFX_CONTEXT* context, RFX_PIXEL_FORMAT pixel_format);
FREERDP_API void rfx_context_reset(RFX_CONTEXT* context);

FREERDP_API RFX_MESSAGE* rfx_process_message(RFX_CONTEXT* context, uint8* data, uint32 length);
FREERDP_API uint16 rfx_message_get_tile_count(RFX_MESSAGE* message);
FREERDP_API RFX_TILE* rfx_message_get_tile(RFX_MESSAGE* message, int index);
FREERDP_API uint16 rfx_message_get_rect_count(RFX_MESSAGE* message);
FREERDP_API RFX_RECT* rfx_message_get_rect(RFX_MESSAGE* message, int index);
FREERDP_API void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message);

FREERDP_API void rfx_compose_message_header(RFX_CONTEXT* context, STREAM* s);
FREERDP_API void rfx_compose_message(RFX_CONTEXT* context, STREAM* s,
	const RFX_RECT* rects, int num_rects, uint8* image_data, int width, int height, int rowstride);

#ifdef __cplusplus
}
#endif

#endif /* __RFX_H */
