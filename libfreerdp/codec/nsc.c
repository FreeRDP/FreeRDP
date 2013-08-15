/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Codec
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 * Copyright 2012 Vic Lee
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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <winpr/crt.h>

#include <freerdp/codec/nsc.h>

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
	UINT16 rw;
	BYTE shift;
	BYTE* yplane;
	BYTE* coplane;
	BYTE* cgplane;
	BYTE* aplane;
	INT16 y_val;
	INT16 co_val;
	INT16 cg_val;
	INT16 r_val;
	INT16 g_val;
	INT16 b_val;
	BYTE* bmpdata;

	bmpdata = context->BitmapData;
	rw = ROUND_UP_TO(context->width, 8);
	shift = context->nsc_stream.ColorLossLevel - 1; /* colorloss recovery + YCoCg shift */

	for (y = 0; y < context->height; y++)
	{
		if (context->nsc_stream.ChromaSubSamplingLevel > 0)
		{
			yplane = context->priv->PlaneBuffers[0] + y * rw; /* Y */
			coplane = context->priv->PlaneBuffers[1] + (y >> 1) * (rw >> 1); /* Co, supersampled */
			cgplane = context->priv->PlaneBuffers[2] + (y >> 1) * (rw >> 1); /* Cg, supersampled */
		}
		else
		{
			yplane = context->priv->PlaneBuffers[0] + y * context->width; /* Y */
			coplane = context->priv->PlaneBuffers[1] + y * context->width; /* Co */
			cgplane = context->priv->PlaneBuffers[2] + y * context->width; /* Cg */
		}

		aplane = context->priv->PlaneBuffers[3] + y * context->width; /* A */

		for (x = 0; x < context->width; x++)
		{
			y_val = (INT16) *yplane;
			co_val = (INT16) (INT8) (*coplane << shift);
			cg_val = (INT16) (INT8) (*cgplane << shift);
			r_val = y_val + co_val - cg_val;
			g_val = y_val + cg_val;
			b_val = y_val - co_val - cg_val;
			*bmpdata++ = MINMAX(b_val, 0, 0xFF);
			*bmpdata++ = MINMAX(g_val, 0, 0xFF);
			*bmpdata++ = MINMAX(r_val, 0, 0xFF);
			*bmpdata++ = *aplane;
			yplane++;
			coplane += (context->nsc_stream.ChromaSubSamplingLevel > 0 ? x % 2 : 1);
			cgplane += (context->nsc_stream.ChromaSubSamplingLevel > 0 ? x % 2 : 1);
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
				len = (UINT32) *in++;
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

	rle = context->nsc_stream.Planes;

	for (i = 0; i < 4; i++)
	{
		originalSize = context->OrgByteCount[i];
		planeSize = context->nsc_stream.PlaneByteCount[i];

		if (planeSize == 0)
			FillMemory(context->priv->PlaneBuffers[i], originalSize, 0xFF);
		else if (planeSize < originalSize)
			nsc_rle_decode(rle, context->priv->PlaneBuffers[i], originalSize);
		else
			CopyMemory(context->priv->PlaneBuffers[i], rle, originalSize);

		rle += planeSize;
	}
}

static void nsc_stream_initialize(NSC_CONTEXT* context, wStream* s)
{
	int i;

	for (i = 0; i < 4; i++)
		Stream_Read_UINT32(s, context->nsc_stream.PlaneByteCount[i]);

	Stream_Read_UINT8(s, context->nsc_stream.ColorLossLevel);
	Stream_Read_UINT8(s, context->nsc_stream.ChromaSubSamplingLevel);
	Stream_Seek(s, 2);

	context->nsc_stream.Planes = Stream_Pointer(s);
}

static void nsc_context_initialize(NSC_CONTEXT* context, wStream* s)
{
	int i;
	UINT32 length;
	UINT32 tempWidth;
	UINT32 tempHeight;

	nsc_stream_initialize(context, s);
	length = context->width * context->height * 4;

	if (!context->BitmapData)
	{
		context->BitmapData = malloc(length + 16);
		ZeroMemory(context->BitmapData, length + 16);
		context->BitmapDataLength = length;
	}
	else if (length > context->BitmapDataLength)
	{
		context->BitmapData = realloc(context->BitmapData, length + 16);
		context->BitmapDataLength = length;
	}

	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);

	/* The maximum length a decoded plane can reach in all cases */
	length = tempWidth * tempHeight;

	if (length > context->priv->PlaneBuffersLength)
	{
		for (i = 0; i < 4; i++)
			context->priv->PlaneBuffers[i] = (BYTE*) realloc(context->priv->PlaneBuffers[i], length);

		context->priv->PlaneBuffersLength = length;
	}

	for (i = 0; i < 4; i++)
	{
		context->OrgByteCount[i] = context->width * context->height;
	}

	if (context->nsc_stream.ChromaSubSamplingLevel > 0)	/* [MS-RDPNSC] 2.2 */
	{
		context->OrgByteCount[0] = tempWidth * context->height;
		context->OrgByteCount[1] = (tempWidth >> 1) * (tempHeight >> 1);
		context->OrgByteCount[2] = context->OrgByteCount[1];
	}
}

static void nsc_profiler_print(NSC_CONTEXT* context)
{
	PROFILER_PRINT_HEADER;

	PROFILER_PRINT(context->priv->prof_nsc_rle_decompress_data);
	PROFILER_PRINT(context->priv->prof_nsc_decode);
	PROFILER_PRINT(context->priv->prof_nsc_rle_compress_data);
	PROFILER_PRINT(context->priv->prof_nsc_encode);

	PROFILER_PRINT_FOOTER;
}

NSC_CONTEXT* nsc_context_new(void)
{
	UINT8 i;
	NSC_CONTEXT* context;

	context = (NSC_CONTEXT*) calloc(1, sizeof(NSC_CONTEXT));
	context->priv = (NSC_CONTEXT_PRIV*) calloc(1, sizeof(NSC_CONTEXT_PRIV));

	for (i = 0; i < 5; ++i)
	{
		context->priv->PlaneBuffers[i] = NULL;
	}

	context->BitmapData = NULL;

	context->decode = nsc_decode;
	context->encode = nsc_encode;

	context->priv->PlanePool = BufferPool_New(TRUE, 0, 16);

	PROFILER_CREATE(context->priv->prof_nsc_rle_decompress_data, "nsc_rle_decompress_data");
	PROFILER_CREATE(context->priv->prof_nsc_decode, "nsc_decode");
	PROFILER_CREATE(context->priv->prof_nsc_rle_compress_data, "nsc_rle_compress_data");
	PROFILER_CREATE(context->priv->prof_nsc_encode, "nsc_encode");

	/* Default encoding parameters */
	context->nsc_stream.ColorLossLevel = 3;
	context->nsc_stream.ChromaSubSamplingLevel = 1;

	/* init optimized methods */
	NSC_INIT_SIMD(context);

	return context;
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

	if (context->BitmapData)
		free(context->BitmapData);

	BufferPool_Free(context->priv->PlanePool);

	nsc_profiler_print(context);
	PROFILER_FREE(context->priv->prof_nsc_rle_decompress_data);
	PROFILER_FREE(context->priv->prof_nsc_decode);
	PROFILER_FREE(context->priv->prof_nsc_rle_compress_data);
	PROFILER_FREE(context->priv->prof_nsc_encode);

	free(context->priv);
	free(context);
}

void nsc_context_set_pixel_format(NSC_CONTEXT* context, RDP_PIXEL_FORMAT pixel_format)
{
	context->pixel_format = pixel_format;

	switch (pixel_format)
	{
		case RDP_PIXEL_FORMAT_B8G8R8A8:
		case RDP_PIXEL_FORMAT_R8G8B8A8:
			context->bpp = 32;
			break;

		case RDP_PIXEL_FORMAT_B8G8R8:
		case RDP_PIXEL_FORMAT_R8G8B8:
			context->bpp = 24;
			break;

		case RDP_PIXEL_FORMAT_B5G6R5_LE:
		case RDP_PIXEL_FORMAT_R5G6B5_LE:
			context->bpp = 16;
			break;

		case RDP_PIXEL_FORMAT_P4_PLANER:
			context->bpp = 4;
			break;

		case RDP_PIXEL_FORMAT_P8:
			context->bpp = 8;
			break;

		default:
			context->bpp = 0;
			break;
	}
}

void nsc_process_message(NSC_CONTEXT* context, UINT16 bpp, UINT16 width, UINT16 height, BYTE* data, UINT32 length)
{
	wStream* s;

	s = Stream_New(data, length);
	context->bpp = bpp;
	context->width = width;
	context->height = height;
	nsc_context_initialize(context, s);
	Stream_Free(s, FALSE);

	/* RLE decode */
	PROFILER_ENTER(context->priv->prof_nsc_rle_decompress_data);
	nsc_rle_decompress_data(context);
	PROFILER_EXIT(context->priv->prof_nsc_rle_decompress_data);

	/* Colorloss recover, Chroma supersample and AYCoCg to ARGB Conversion in one step */
	PROFILER_ENTER(context->priv->prof_nsc_decode);
	context->decode(context);
	PROFILER_EXIT(context->priv->prof_nsc_decode);
}
