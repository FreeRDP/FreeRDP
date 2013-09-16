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

#include <winpr/stream.h>
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
		context->rlgr_decode(context->mode, data, size, buffer, 4096);
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
BOOL rfx_decode_rgb(RFX_CONTEXT* context, RFX_TILE* tile, BYTE* rgb_buffer, int stride)
{
	BYTE* pBuffer;
	INT16* pSrcDst[3];
	UINT32 *y_quants, *cb_quants, *cr_quants;
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t *prims = primitives_get();

	PROFILER_ENTER(context->priv->prof_rfx_decode_rgb);

	y_quants = context->quants + (tile->quantIdxY * 10);
	cb_quants = context->quants + (tile->quantIdxCb * 10);
	cr_quants = context->quants + (tile->quantIdxCr * 10);

	pBuffer = (BYTE*) BufferPool_Take(context->priv->BufferPool, -1);
	pSrcDst[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* y_r_buffer */
	pSrcDst[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* cb_g_buffer */
	pSrcDst[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* cr_b_buffer */

	rfx_decode_component(context, y_quants, tile->YData, tile->YLen, pSrcDst[0]); /* YData */

	rfx_decode_component(context, cb_quants, tile->CbData, tile->CbLen, pSrcDst[1]); /* CbData */

	rfx_decode_component(context, cr_quants, tile->CrData, tile->CrLen, pSrcDst[2]); /* CrData */

	PROFILER_ENTER(context->priv->prof_rfx_ycbcr_to_rgb);
	prims->yCbCrToRGB_16s16s_P3P3((const INT16**) pSrcDst, 64 * sizeof(INT16),
			pSrcDst, 64 * sizeof(INT16), &roi_64x64);
	PROFILER_EXIT(context->priv->prof_rfx_ycbcr_to_rgb);

	PROFILER_ENTER(context->priv->prof_rfx_decode_format_rgb);
		rfx_decode_format_rgb(pSrcDst[0], pSrcDst[1], pSrcDst[2],
			context->pixel_format, rgb_buffer, stride);
	PROFILER_EXIT(context->priv->prof_rfx_decode_format_rgb);
	
	PROFILER_EXIT(context->priv->prof_rfx_decode_rgb);

	BufferPool_Return(context->priv->BufferPool, pBuffer);

	return TRUE;
}
