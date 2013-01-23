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
	INT16* dwt_buffer;

	dwt_buffer = BufferPool_Take(context->priv->BufferPool, -1); /* dwt_buffer */

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
		context->dwt_2d_decode(buffer, dwt_buffer);
	PROFILER_EXIT(context->priv->prof_rfx_dwt_2d_decode);

	PROFILER_EXIT(context->priv->prof_rfx_decode_component);

	BufferPool_Return(context->priv->BufferPool, dwt_buffer);
}

/* rfx_decode_ycbcr_to_rgb code now resides in the primitives library. */

/* stride is bytes between rows in the output buffer. */
void rfx_decode_rgb(RFX_CONTEXT* context, STREAM* data_in,
	int y_size, const UINT32* y_quants,
	int cb_size, const UINT32* cb_quants,
	int cr_size, const UINT32* cr_quants, BYTE* rgb_buffer, int stride)
{
	INT16* pSrcDst[3];
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t *prims = primitives_get();

	PROFILER_ENTER(context->priv->prof_rfx_decode_rgb);

	pSrcDst[0] = BufferPool_Take(context->priv->BufferPool, -1); /* y_r_buffer */
	pSrcDst[1] = BufferPool_Take(context->priv->BufferPool, -1); /* cb_g_buffer */
	pSrcDst[2] = BufferPool_Take(context->priv->BufferPool, -1); /* cr_b_buffer */

	rfx_decode_component(context, y_quants, stream_get_tail(data_in), y_size, pSrcDst[0]); /* YData */
	stream_seek(data_in, y_size);
	rfx_decode_component(context, cb_quants, stream_get_tail(data_in), cb_size, pSrcDst[1]); /* CbData */
	stream_seek(data_in, cb_size);
	rfx_decode_component(context, cr_quants, stream_get_tail(data_in), cr_size, pSrcDst[2]); /* CrData */
	stream_seek(data_in, cr_size);

	prims->yCbCrToRGB_16s16s_P3P3((const INT16**) pSrcDst, 64 * sizeof(INT16),
			pSrcDst, 64 * sizeof(INT16), &roi_64x64);

	PROFILER_ENTER(context->priv->prof_rfx_decode_format_rgb);
		rfx_decode_format_rgb(pSrcDst[0], pSrcDst[1], pSrcDst[2],
			context->pixel_format, rgb_buffer, stride);
	PROFILER_EXIT(context->priv->prof_rfx_decode_format_rgb);
	
	PROFILER_EXIT(context->priv->prof_rfx_decode_rgb);

	BufferPool_Return(context->priv->BufferPool, pSrcDst[0]);
	BufferPool_Return(context->priv->BufferPool, pSrcDst[1]);
	BufferPool_Return(context->priv->BufferPool, pSrcDst[2]);
}
