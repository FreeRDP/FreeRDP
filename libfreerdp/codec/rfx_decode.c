/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Decode
 *
 * Copyright 2011 Vic Lee
 * Copyright 2011 Norbert Federa <nfedera@thinstuff.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/utils/stream.h>
#include <freerdp/primitives.h>

#include "rfx_types.h"
#include "rfx_rlgr.h"
#include "rfx_differential.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"

#include "rfx_decode.h"

/* stride is bytes between rows in the output buffer. */
static void rfx_decode_format_rgb(INT16* r_buf, INT16* g_buf, INT16* b_buf,
	RDP_PIXEL_FORMAT pixel_format, BYTE* dst_buf, int stride)
{
	primitives_t *prims = primitives_get();
	INT16* r = r_buf;
	INT16* g = g_buf;
	INT16* b = b_buf;
	INT16* pSrc[3];
	static const prim_size_t roi_64x64 = { 64, 64 };
	BYTE* dst = dst_buf;
	int x, y;
	
	switch (pixel_format)
	{
		case RDP_PIXEL_FORMAT_B8G8R8A8:
			pSrc[0] = r;  pSrc[1] = g;  pSrc[2] = b;
			prims->RGBToRGB_16s8u_P3AC4R(
				(const INT16 **) pSrc, 64*sizeof(INT16),
				dst, stride, &roi_64x64);
			break;
		case RDP_PIXEL_FORMAT_R8G8B8A8:
			pSrc[0] = b;  pSrc[1] = g;  pSrc[2] = r;
			prims->RGBToRGB_16s8u_P3AC4R(
				(const INT16 **) pSrc, 64*sizeof(INT16),
				dst, stride, &roi_64x64);
			break;
		case RDP_PIXEL_FORMAT_B8G8R8:
			for (y=0; y<64; y++)
			{
				for (x=0; x<64; x++)
				{
					*dst++ = (BYTE) (*b++);
					*dst++ = (BYTE) (*g++);
					*dst++ = (BYTE) (*r++);
				}
				dst += stride - (64*3);
			}
			break;
		case RDP_PIXEL_FORMAT_R8G8B8:
			for (y=0; y<64; y++)
			{
				for (x=0; x<64; x++)
				{
					*dst++ = (BYTE) (*r++);
					*dst++ = (BYTE) (*g++);
					*dst++ = (BYTE) (*b++);
				}
				dst += stride - (64*3);
			}
			break;
		default:
			break;
	}
}

static void rfx_decode_component(RFX_CONTEXT* context, const UINT32* quantization_values,
	const BYTE* data, int size, INT16* buffer)
{
	PROFILER_ENTER(context->priv->prof_rfx_decode_component);

	PROFILER_ENTER(context->priv->prof_rfx_rlgr_decode);
		rfx_rlgr_decode(context->mode, data, size, buffer, 4096);
	PROFILER_EXIT(context->priv->prof_rfx_rlgr_decode);

	PROFILER_ENTER(context->priv->prof_rfx_differential_decode);
		rfx_differential_decode(buffer + 4032, 64);
	PROFILER_EXIT(context->priv->prof_rfx_differential_decode);

	PROFILER_ENTER(context->priv->prof_rfx_quantization_decode);
		context->quantization_decode(buffer, quantization_values);
	PROFILER_EXIT(context->priv->prof_rfx_quantization_decode);

	PROFILER_ENTER(context->priv->prof_rfx_dwt_2d_decode);
		context->dwt_2d_decode(buffer, context->priv->dwt_buffer);
	PROFILER_EXIT(context->priv->prof_rfx_dwt_2d_decode);

	PROFILER_EXIT(context->priv->prof_rfx_decode_component);
}

/* rfx_decode_ycbcr_to_rgb code now resides in the primitives library. */

/* stride is bytes between rows in the output buffer. */
void rfx_decode_rgb(RFX_CONTEXT* context, STREAM* data_in,
	int y_size, const UINT32 * y_quants,
	int cb_size, const UINT32 * cb_quants,
	int cr_size, const UINT32 * cr_quants, BYTE* rgb_buffer, int stride)
{
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t *prims = primitives_get();
	INT16 *pSrcDst[3];

	PROFILER_ENTER(context->priv->prof_rfx_decode_rgb);

	rfx_decode_component(context, y_quants, stream_get_tail(data_in), y_size, context->priv->y_r_buffer); /* YData */
	stream_seek(data_in, y_size);
	rfx_decode_component(context, cb_quants, stream_get_tail(data_in), cb_size, context->priv->cb_g_buffer); /* CbData */
	stream_seek(data_in, cb_size);
	rfx_decode_component(context, cr_quants, stream_get_tail(data_in), cr_size, context->priv->cr_b_buffer); /* CrData */
	stream_seek(data_in, cr_size);

	PROFILER_ENTER(context->priv->prof_rfx_decode_ycbcr_to_rgb);
		pSrcDst[0] = context->priv->y_r_buffer;
		pSrcDst[1] = context->priv->cb_g_buffer;
		pSrcDst[2] = context->priv->cr_b_buffer;
		prims->yCbCrToRGB_16s16s_P3P3((const INT16 **) pSrcDst, 64*sizeof(INT16),
			pSrcDst, 64*sizeof(INT16), &roi_64x64);
	PROFILER_EXIT(context->priv->prof_rfx_decode_ycbcr_to_rgb);

	PROFILER_ENTER(context->priv->prof_rfx_decode_format_rgb);
		rfx_decode_format_rgb(context->priv->y_r_buffer, context->priv->cb_g_buffer, context->priv->cr_b_buffer,
			context->pixel_format, rgb_buffer, stride);
	PROFILER_EXIT(context->priv->prof_rfx_decode_format_rgb);
	
	PROFILER_EXIT(context->priv->prof_rfx_decode_rgb);
}
