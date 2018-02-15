/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Codec
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 * Copyright 2012 Vic Lee
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
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

#include <winpr/crt.h>

#include <freerdp/codec/nsc.h>
#include <freerdp/codec/color.h>

#include "nsc_types.h"
#include "nsc_encode.h"

#include "nsc_sse2.h"

#ifndef NSC_INIT_SIMD
#define NSC_INIT_SIMD(_nsc_context) do { } while (0)
#endif

static void nsc_decode(NSC_CONTEXT* context)
{
	UINT16 x;
	UINT16 y;
	UINT16 rw = ROUND_UP_TO(context->width, 8);
	BYTE shift = context->ColorLossLevel - 1; /* colorloss recovery + YCoCg shift */
	BYTE* bmpdata = context->BitmapData;

	for (y = 0; y < context->height; y++)
	{
		const BYTE* yplane;
		const BYTE* coplane;
		const BYTE* cgplane;
		const BYTE* aplane = context->priv->PlaneBuffers[3] + y * context->width; /* A */

		if (context->ChromaSubsamplingLevel)
		{
			yplane = context->priv->PlaneBuffers[0] + y * rw; /* Y */
			coplane = context->priv->PlaneBuffers[1] + (y >> 1) * (rw >>
			          1); /* Co, supersampled */
			cgplane = context->priv->PlaneBuffers[2] + (y >> 1) * (rw >>
			          1); /* Cg, supersampled */
		}
		else
		{
			yplane = context->priv->PlaneBuffers[0] + y * context->width; /* Y */
			coplane = context->priv->PlaneBuffers[1] + y * context->width; /* Co */
			cgplane = context->priv->PlaneBuffers[2] + y * context->width; /* Cg */
		}

		for (x = 0; x < context->width; x++)
		{
			INT16 y_val = (INT16) * yplane;
			INT16 co_val = (INT16)(INT8)(*coplane << shift);
			INT16 cg_val = (INT16)(INT8)(*cgplane << shift);
			INT16 r_val = y_val + co_val - cg_val;
			INT16 g_val = y_val + cg_val;
			INT16 b_val = y_val - co_val - cg_val;
			*bmpdata++ = MINMAX(b_val, 0, 0xFF);
			*bmpdata++ = MINMAX(g_val, 0, 0xFF);
			*bmpdata++ = MINMAX(r_val, 0, 0xFF);
			*bmpdata++ = *aplane;
			yplane++;
			coplane += (context->ChromaSubsamplingLevel ? x % 2 : 1);
			cgplane += (context->ChromaSubsamplingLevel ? x % 2 : 1);
			aplane++;
		}
	}
}

static void nsc_rle_decode(BYTE* in, BYTE* out, UINT32 originalSize)
{
	UINT32 len;
	UINT32 left;
	BYTE value;
	left = originalSize;

	while (left > 4)
	{
		value = *in++;

		if (left == 5)
		{
			*out++ = value;
			left--;
		}
		else if (value == *in)
		{
			in++;

			if (*in < 0xFF)
			{
				len = (UINT32) * in++;
				len += 2;
			}
			else
			{
				in++;
				len = *((UINT32*) in);
				in += 4;
			}

			FillMemory(out, len, value);
			out += len;
			left -= len;
		}
		else
		{
			*out++ = value;
			left--;
		}
	}

	*((UINT32*)out) = *((UINT32*)in);
}

static void nsc_rle_decompress_data(NSC_CONTEXT* context)
{
	UINT16 i;
	BYTE* rle;
	UINT32 planeSize;
	UINT32 originalSize;
	rle = context->Planes;

	for (i = 0; i < 4; i++)
	{
		originalSize = context->OrgByteCount[i];
		planeSize = context->PlaneByteCount[i];

		if (planeSize == 0)
			FillMemory(context->priv->PlaneBuffers[i], originalSize, 0xFF);
		else if (planeSize < originalSize)
			nsc_rle_decode(rle, context->priv->PlaneBuffers[i], originalSize);
		else
			CopyMemory(context->priv->PlaneBuffers[i], rle, originalSize);

		rle += planeSize;
	}
}

static BOOL nsc_stream_initialize(NSC_CONTEXT* context, wStream* s)
{
	int i;

	if (Stream_GetRemainingLength(s) < 20)
		return FALSE;

	for (i = 0; i < 4; i++)
		Stream_Read_UINT32(s, context->PlaneByteCount[i]);

	Stream_Read_UINT8(s, context->ColorLossLevel); /* ColorLossLevel (1 byte) */
	Stream_Read_UINT8(s,
	                  context->ChromaSubsamplingLevel); /* ChromaSubsamplingLevel (1 byte) */
	Stream_Seek(s, 2); /* Reserved (2 bytes) */
	context->Planes = Stream_Pointer(s);
	return TRUE;
}

static BOOL nsc_context_initialize(NSC_CONTEXT* context, wStream* s)
{
	int i;
	UINT32 length;
	UINT32 tempWidth;
	UINT32 tempHeight;

	if (!nsc_stream_initialize(context, s))
		return FALSE;

	length = context->width * context->height * 4;

	if (!context->BitmapData)
	{
		context->BitmapData = calloc(1, length + 16);

		if (!context->BitmapData)
			return FALSE;

		context->BitmapDataLength = length;
	}
	else if (length > context->BitmapDataLength)
	{
		void* tmp;
		tmp = realloc(context->BitmapData, length + 16);

		if (!tmp)
			return FALSE;

		context->BitmapData = tmp;
		context->BitmapDataLength = length;
	}

	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);
	/* The maximum length a decoded plane can reach in all cases */
	length = tempWidth * tempHeight;

	if (length > context->priv->PlaneBuffersLength)
	{
		for (i = 0; i < 4; i++)
		{
			void* tmp = (BYTE*) realloc(context->priv->PlaneBuffers[i], length);

			if (!tmp)
				return FALSE;

			context->priv->PlaneBuffers[i] = tmp;
		}

		context->priv->PlaneBuffersLength = length;
	}

	for (i = 0; i < 4; i++)
	{
		context->OrgByteCount[i] = context->width * context->height;
	}

	if (context->ChromaSubsamplingLevel)
	{
		context->OrgByteCount[0] = tempWidth * context->height;
		context->OrgByteCount[1] = (tempWidth >> 1) * (tempHeight >> 1);
		context->OrgByteCount[2] = context->OrgByteCount[1];
	}

	return TRUE;
}

static void nsc_profiler_print(NSC_CONTEXT* context)
{
	PROFILER_PRINT_HEADER
	PROFILER_PRINT(context->priv->prof_nsc_rle_decompress_data)
	PROFILER_PRINT(context->priv->prof_nsc_decode)
	PROFILER_PRINT(context->priv->prof_nsc_rle_compress_data)
	PROFILER_PRINT(context->priv->prof_nsc_encode)
	PROFILER_PRINT_FOOTER
}

BOOL nsc_context_reset(NSC_CONTEXT* context, UINT32 width, UINT32 height)
{
	if (!context)
		return FALSE;

	context->width = width;
	context->height = height;
	return TRUE;
}

NSC_CONTEXT* nsc_context_new(void)
{
	NSC_CONTEXT* context;
	context = (NSC_CONTEXT*) calloc(1, sizeof(NSC_CONTEXT));

	if (!context)
		return NULL;

	context->priv = (NSC_CONTEXT_PRIV*) calloc(1, sizeof(NSC_CONTEXT_PRIV));

	if (!context->priv)
		goto error_priv;

	WLog_Init();
	context->priv->log = WLog_Get("com.freerdp.codec.nsc");
	WLog_OpenAppender(context->priv->log);
	context->BitmapData = NULL;
	context->decode = nsc_decode;
	context->encode = nsc_encode;
	context->priv->PlanePool = BufferPool_New(TRUE, 0, 16);

	if (!context->priv->PlanePool)
		goto error_PlanePool;

	PROFILER_CREATE(context->priv->prof_nsc_rle_decompress_data,
	                "nsc_rle_decompress_data")
	PROFILER_CREATE(context->priv->prof_nsc_decode, "nsc_decode")
	PROFILER_CREATE(context->priv->prof_nsc_rle_compress_data,
	                "nsc_rle_compress_data")
	PROFILER_CREATE(context->priv->prof_nsc_encode, "nsc_encode")
	/* Default encoding parameters */
	context->ColorLossLevel = 3;
	context->ChromaSubsamplingLevel = 1;
	/* init optimized methods */
	NSC_INIT_SIMD(context);
	return context;
error_PlanePool:
	free(context->priv);
error_priv:
	free(context);
	return NULL;
}

void nsc_context_free(NSC_CONTEXT* context)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		if (context->priv->PlaneBuffers[i])
		{
			free(context->priv->PlaneBuffers[i]);
			context->priv->PlaneBuffers[i] = NULL;
		}
	}

	free(context->BitmapData);
	BufferPool_Free(context->priv->PlanePool);
	nsc_profiler_print(context);
	PROFILER_FREE(context->priv->prof_nsc_rle_decompress_data)
	PROFILER_FREE(context->priv->prof_nsc_decode)
	PROFILER_FREE(context->priv->prof_nsc_rle_compress_data)
	PROFILER_FREE(context->priv->prof_nsc_encode)
	free(context->priv);
	free(context);
}

BOOL nsc_context_set_pixel_format(NSC_CONTEXT* context, UINT32 pixel_format)
{
	if (!context)
		return FALSE;

	context->format = pixel_format;
	return TRUE;
}

BOOL nsc_process_message(NSC_CONTEXT* context, UINT16 bpp,
                         UINT32 width, UINT32 height,
                         const BYTE* data, UINT32 length,
                         BYTE* pDstData, UINT32 DstFormat,
                         UINT32 nDstStride,
                         UINT32 nXDst, UINT32 nYDst, UINT32 nWidth,
                         UINT32 nHeight, UINT32 flip)
{
	wStream* s;
	BOOL ret;
	s = Stream_New((BYTE*)data, length);

	if (!s)
		return FALSE;

	if (nDstStride == 0)
		nDstStride = nWidth * GetBytesPerPixel(DstFormat);

	switch (bpp)
	{
		case 32:
			context->format = PIXEL_FORMAT_BGRA32;
			break;

		case 24:
			context->format = PIXEL_FORMAT_BGR24;
			break;

		case 16:
			context->format = PIXEL_FORMAT_BGR16;
			break;

		case 8:
			context->format = PIXEL_FORMAT_RGB8;
			break;

		case 4:
			context->format = PIXEL_FORMAT_A4;
			break;

		default:
			return FALSE;
	}

	context->width = width;
	context->height = height;
	ret = nsc_context_initialize(context, s);
	Stream_Free(s, FALSE);

	if (!ret)
		return FALSE;

	/* RLE decode */
	PROFILER_ENTER(context->priv->prof_nsc_rle_decompress_data)
	nsc_rle_decompress_data(context);
	PROFILER_EXIT(context->priv->prof_nsc_rle_decompress_data)
	/* Colorloss recover, Chroma supersample and AYCoCg to ARGB Conversion in one step */
	PROFILER_ENTER(context->priv->prof_nsc_decode)
	context->decode(context);
	PROFILER_EXIT(context->priv->prof_nsc_decode)

	if (!freerdp_image_copy(pDstData, DstFormat, nDstStride, nXDst, nYDst,
	                        width, height, context->BitmapData,
	                        PIXEL_FORMAT_BGRA32, 0, 0, 0, NULL, flip))
		return FALSE;

	return TRUE;
}
