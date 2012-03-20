/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/utils/memory.h>

#include "nsc_types.h"
#include "nsc_encode.h"

#ifdef WITH_SSE2
#include "nsc_sse2.h"
#endif

#ifndef NSC_INIT_SIMD
#define NSC_INIT_SIMD(_nsc_context) do { } while (0)
#endif

static void nsc_decode(NSC_CONTEXT* context)
{
	uint16 x;
	uint16 y;
	uint16 rw;
	uint8 shift;
	uint8* yplane;
	uint8* coplane;
	uint8* cgplane;
	uint8* aplane;
	sint16 y_val;
	sint16 co_val;
	sint16 cg_val;
	sint16 r_val;
	sint16 g_val;
	sint16 b_val;
	uint8* bmpdata;

	bmpdata = context->bmpdata;
	rw = ROUND_UP_TO(context->width, 8);
	shift = context->nsc_stream.ColorLossLevel - 1; /* colorloss recovery + YCoCg shift */

	for (y = 0; y < context->height; y++)
	{
		if (context->nsc_stream.ChromaSubSamplingLevel > 0)
		{
			yplane = context->priv->plane_buf[0] + y * rw; /* Y */
			coplane = context->priv->plane_buf[1] + (y >> 1) * (rw >> 1); /* Co, supersampled */
			cgplane = context->priv->plane_buf[2] + (y >> 1) * (rw >> 1); /* Cg, supersampled */
		}
		else
		{
			yplane = context->priv->plane_buf[0] + y * context->width; /* Y */
			coplane = context->priv->plane_buf[1] + y * context->width; /* Co */
			cgplane = context->priv->plane_buf[2] + y * context->width; /* Cg */
		}
		aplane = context->priv->plane_buf[3] + y * context->width; /* A */
		for (x = 0; x < context->width; x++)
		{
			y_val = (sint16) *yplane;
			co_val = (sint16) (sint8) (*coplane << shift);
			cg_val = (sint16) (sint8) (*cgplane << shift);
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

static void nsc_rle_decode(uint8* in, uint8* out, uint32 origsz)
{
	uint32 len;
	uint32 left;
	uint8 value;

	left = origsz;
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
				len = (uint32) *in++;
				len += 2;
			}
			else
			{
				in++;
				len = *((uint32*) in);
				in += 4;
			}
			memset(out, value, len);
			out += len;
			left -= len;
		}
		else
		{
			*out++ = value;
			left--;
		}
	}

	*((uint32*)out) = *((uint32*)in);
}

static void nsc_rle_decompress_data(NSC_CONTEXT* context)
{
	uint16 i;
	uint8* rle;
	uint32 origsize;
	uint32 planesize;

	rle = context->nsc_stream.Planes;

	for (i = 0; i < 4; i++)
	{
		origsize = context->OrgByteCount[i];
		planesize = context->nsc_stream.PlaneByteCount[i];

		if (planesize == 0)
			memset(context->priv->plane_buf[i], 0xff, origsize);
		else if (planesize < origsize)
			nsc_rle_decode(rle, context->priv->plane_buf[i], origsize);
		else
			memcpy(context->priv->plane_buf[i], rle, origsize);

		rle += planesize;
	}
}

static void nsc_stream_initialize(NSC_CONTEXT* context, STREAM* s)
{
	int i;

	for (i = 0; i < 4; i++)
		stream_read_uint32(s, context->nsc_stream.PlaneByteCount[i]);

	stream_read_uint8(s, context->nsc_stream.ColorLossLevel);
	stream_read_uint8(s, context->nsc_stream.ChromaSubSamplingLevel);
	stream_seek(s, 2);

	context->nsc_stream.Planes = stream_get_tail(s);
}

static void nsc_context_initialize(NSC_CONTEXT* context, STREAM* s)
{
	int i;
	uint32 length;
	uint32 tempWidth;
	uint32 tempHeight;

	nsc_stream_initialize(context, s);
	length = context->width * context->height * 4;
	if (context->bmpdata == NULL)
	{
		context->bmpdata = xzalloc(length + 16);
		context->bmpdata_length = length;
	}
	else if (length > context->bmpdata_length)
	{
		context->bmpdata = xrealloc(context->bmpdata, length + 16);
		context->bmpdata_length = length;
	}

	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);
	/* The maximum length a decoded plane can reach in all cases */
	length = tempWidth * tempHeight;
	if (length > context->priv->plane_buf_length)
	{
		for (i = 0; i < 4; i++)
			context->priv->plane_buf[i] = (uint8*) xrealloc(context->priv->plane_buf[i], length);
		context->priv->plane_buf_length = length;
	}

	for (i = 0; i < 4; i++)
		context->OrgByteCount[i]=context->width * context->height;

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

void nsc_context_free(NSC_CONTEXT* context)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		if (context->priv->plane_buf[i])
			xfree(context->priv->plane_buf[i]);
	}
	if (context->bmpdata)
		xfree(context->bmpdata);

	nsc_profiler_print(context);
	PROFILER_FREE(context->priv->prof_nsc_rle_decompress_data);
	PROFILER_FREE(context->priv->prof_nsc_decode);
	PROFILER_FREE(context->priv->prof_nsc_rle_compress_data);
	PROFILER_FREE(context->priv->prof_nsc_encode);

	xfree(context->priv);
	xfree(context);
}

NSC_CONTEXT* nsc_context_new(void)
{
	NSC_CONTEXT* nsc_context;

	nsc_context = xnew(NSC_CONTEXT);
	nsc_context->priv = xnew(NSC_CONTEXT_PRIV);

	nsc_context->decode = nsc_decode;
	nsc_context->encode = nsc_encode;

	PROFILER_CREATE(nsc_context->priv->prof_nsc_rle_decompress_data, "nsc_rle_decompress_data");
	PROFILER_CREATE(nsc_context->priv->prof_nsc_decode, "nsc_decode");
	PROFILER_CREATE(nsc_context->priv->prof_nsc_rle_compress_data, "nsc_rle_compress_data");
	PROFILER_CREATE(nsc_context->priv->prof_nsc_encode, "nsc_encode");

	/* Default encoding parameters */
	nsc_context->nsc_stream.ColorLossLevel = 3;
	nsc_context->nsc_stream.ChromaSubSamplingLevel = 1;

	return nsc_context;
}

void nsc_context_set_cpu_opt(NSC_CONTEXT* context, uint32 cpu_opt)
{
	if (cpu_opt)
		NSC_INIT_SIMD(context);
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

void nsc_process_message(NSC_CONTEXT* context, uint16 bpp,
	uint16 width, uint16 height, uint8* data, uint32 length)
{
	STREAM* s;

	s = stream_new(0);
	stream_attach(s, data, length);
	context->bpp = bpp;
	context->width = width;
	context->height = height;
	nsc_context_initialize(context, s);
	stream_detach(s);
	stream_free(s);

	/* RLE decode */
	PROFILER_ENTER(context->priv->prof_nsc_rle_decompress_data);
	nsc_rle_decompress_data(context);
	PROFILER_EXIT(context->priv->prof_nsc_rle_decompress_data);

	/* Colorloss recover, Chroma supersample and AYCoCg to ARGB Conversion in one step */
	PROFILER_ENTER(context->priv->prof_nsc_decode);
	context->decode(context);
	PROFILER_EXIT(context->priv->prof_nsc_decode);
}
