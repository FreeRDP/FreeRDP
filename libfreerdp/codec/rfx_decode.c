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

struct _RFX_COMPONENT_WORK_PARAM
{
	int size;
	INT16* buffer;
	const BYTE* data;
	RFX_CONTEXT* context;
	const UINT32* quantization_values;
};
typedef struct _RFX_COMPONENT_WORK_PARAM RFX_COMPONENT_WORK_PARAM;

void CALLBACK rfx_decode_component_work_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK work)
{
	RFX_COMPONENT_WORK_PARAM* param = (RFX_COMPONENT_WORK_PARAM*) context;
	rfx_decode_component(param->context, param->quantization_values, param->data, param->size, param->buffer);
}

/* stride is bytes between rows in the output buffer. */
BOOL rfx_decode_rgb(RFX_CONTEXT* context, wStream* data_in,
	int y_size, const UINT32* y_quants,
	int cb_size, const UINT32* cb_quants,
	int cr_size, const UINT32* cr_quants, BYTE* rgb_buffer, int stride)
{
	INT16* pSrcDst[3];
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t *prims = primitives_get();

	PROFILER_ENTER(context->priv->prof_rfx_decode_rgb);

	pSrcDst[0] = (INT16*)((BYTE*)BufferPool_Take(context->priv->BufferPool, -1) + 16); /* y_r_buffer */
	pSrcDst[1] = (INT16*)((BYTE*)BufferPool_Take(context->priv->BufferPool, -1) + 16); /* cb_g_buffer */
	pSrcDst[2] = (INT16*)((BYTE*)BufferPool_Take(context->priv->BufferPool, -1) + 16); /* cr_b_buffer */

#if 0
	if (context->priv->UseThreads)
	{
		PTP_WORK work_objects[3];
		RFX_COMPONENT_WORK_PARAM params[3];

		params[0].context = context;
		params[0].quantization_values = y_quants;
		params[0].buffer = Stream_Pointer(data_in);
		params[0].capacity = y_size;
		params[0].buffer = pSrcDst[0];
		Stream_Seek(data_in, y_size);

		params[1].context = context;
		params[1].quantization_values = cb_quants;
		params[1].buffer = Stream_Pointer(data_in);
		params[1].capacity = cb_size;
		params[1].buffer = pSrcDst[1];
		Stream_Seek(data_in, cb_size);

		params[2].context = context;
		params[2].quantization_values = cr_quants;
		params[2].buffer = Stream_Pointer(data_in);
		params[2].capacity = cr_size;
		params[2].buffer = pSrcDst[2];
		Stream_Seek(data_in, cr_size);

		work_objects[0] = CreateThreadpoolWork((PTP_WORK_CALLBACK) rfx_decode_component_work_callback,
				(void*) &params[0], &context->priv->ThreadPoolEnv);
		work_objects[1] = CreateThreadpoolWork((PTP_WORK_CALLBACK) rfx_decode_component_work_callback,
				(void*) &params[1], &context->priv->ThreadPoolEnv);
		work_objects[2] = CreateThreadpoolWork((PTP_WORK_CALLBACK) rfx_decode_component_work_callback,
				(void*) &params[2], &context->priv->ThreadPoolEnv);

		SubmitThreadpoolWork(work_objects[0]);
		SubmitThreadpoolWork(work_objects[1]);
		SubmitThreadpoolWork(work_objects[2]);

		WaitForThreadpoolWorkCallbacks(work_objects[0], FALSE);
		WaitForThreadpoolWorkCallbacks(work_objects[1], FALSE);
		WaitForThreadpoolWorkCallbacks(work_objects[2], FALSE);
	}
	else
#endif
	{
		if (Stream_GetRemainingLength(data_in) < y_size + cb_size + cr_size)
		{
			DEBUG_WARN("rfx_decode_rgb: packet too small for y_size+cb_size+cr_size");
			return FALSE;
		}

		rfx_decode_component(context, y_quants, Stream_Pointer(data_in), y_size, pSrcDst[0]); /* YData */
		Stream_Seek(data_in, y_size);

		rfx_decode_component(context, cb_quants, Stream_Pointer(data_in), cb_size, pSrcDst[1]); /* CbData */
		Stream_Seek(data_in, cb_size);

		rfx_decode_component(context, cr_quants, Stream_Pointer(data_in), cr_size, pSrcDst[2]); /* CrData */
		Stream_Seek(data_in, cr_size);
	}

	PROFILER_ENTER(context->priv->prof_rfx_ycbcr_to_rgb);
	prims->yCbCrToRGB_16s16s_P3P3((const INT16**) pSrcDst, 64 * sizeof(INT16),
			pSrcDst, 64 * sizeof(INT16), &roi_64x64);
	PROFILER_EXIT(context->priv->prof_rfx_ycbcr_to_rgb);

	PROFILER_ENTER(context->priv->prof_rfx_decode_format_rgb);
		rfx_decode_format_rgb(pSrcDst[0], pSrcDst[1], pSrcDst[2],
			context->pixel_format, rgb_buffer, stride);
	PROFILER_EXIT(context->priv->prof_rfx_decode_format_rgb);
	
	PROFILER_EXIT(context->priv->prof_rfx_decode_rgb);

	BufferPool_Return(context->priv->BufferPool, (BYTE*)pSrcDst[0] - 16);
	BufferPool_Return(context->priv->BufferPool, (BYTE*)pSrcDst[1] - 16);
	BufferPool_Return(context->priv->BufferPool, (BYTE*)pSrcDst[2] - 16);

	return TRUE;
}
