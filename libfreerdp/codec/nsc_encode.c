/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Encoder
 *
 * Copyright 2012 Vic Lee
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
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <freerdp/codec/nsc.h>

#include "nsc_types.h"
#include "nsc_encode.h"

static void nsc_context_initialize_encode(NSC_CONTEXT* context)
{
	int i;
	UINT32 length;
	UINT32 tempWidth;
	UINT32 tempHeight;

	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);
	/* The maximum length a decoded plane can reach in all cases */
	length = tempWidth * tempHeight + 16;
	if (length > context->priv->plane_buf_length)
	{
		for (i = 0; i < 5; i++)
			context->priv->plane_buf[i] = (BYTE*) realloc(context->priv->plane_buf[i], length);
		context->priv->plane_buf_length = length;
	}

	if (context->nsc_stream.ChromaSubSamplingLevel > 0)
	{
		context->OrgByteCount[0] = tempWidth * context->height;
		context->OrgByteCount[1] = tempWidth * tempHeight / 4;
		context->OrgByteCount[2] = tempWidth * tempHeight / 4;
		context->OrgByteCount[3] = context->width * context->height;
	}
	else
	{
		context->OrgByteCount[0] = context->width * context->height;
		context->OrgByteCount[1] = context->width * context->height;
		context->OrgByteCount[2] = context->width * context->height;
		context->OrgByteCount[3] = context->width * context->height;
	}
}

static void nsc_encode_argb_to_aycocg(NSC_CONTEXT* context, BYTE* bmpdata, int rowstride)
{
	UINT16 x;
	UINT16 y;
	UINT16 rw;
	BYTE ccl;
	BYTE* src;
	BYTE* yplane;
	BYTE* coplane;
	BYTE* cgplane;
	BYTE* aplane;
	INT16 r_val;
	INT16 g_val;
	INT16 b_val;
	BYTE a_val;
	UINT32 tempWidth;
	UINT32 tempHeight;

	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);
	rw = (context->nsc_stream.ChromaSubSamplingLevel > 0 ? tempWidth : context->width);
	ccl = context->nsc_stream.ColorLossLevel;
	yplane = context->priv->plane_buf[0];
	coplane = context->priv->plane_buf[1];
	cgplane = context->priv->plane_buf[2];
	aplane = context->priv->plane_buf[3];

	for (y = 0; y < context->height; y++)
	{
		src = bmpdata + (context->height - 1 - y) * rowstride;
		yplane = context->priv->plane_buf[0] + y * rw;
		coplane = context->priv->plane_buf[1] + y * rw;
		cgplane = context->priv->plane_buf[2] + y * rw;
		aplane = context->priv->plane_buf[3] + y * context->width;
		for (x = 0; x < context->width; x++)
		{
			switch (context->pixel_format)
			{
				case RDP_PIXEL_FORMAT_B8G8R8A8:
					b_val = *src++;
					g_val = *src++;
					r_val = *src++;
					a_val = *src++;
					break;
				case RDP_PIXEL_FORMAT_R8G8B8A8:
					r_val = *src++;
					g_val = *src++;
					b_val = *src++;
					a_val = *src++;
					break;
				case RDP_PIXEL_FORMAT_B8G8R8:
					b_val = *src++;
					g_val = *src++;
					r_val = *src++;
					a_val = 0xFF;
					break;
				case RDP_PIXEL_FORMAT_R8G8B8:
					r_val = *src++;
					g_val = *src++;
					b_val = *src++;
					a_val = 0xFF;
					break;
				case RDP_PIXEL_FORMAT_B5G6R5_LE:
					b_val = (INT16) (((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5));
					g_val = (INT16) ((((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3));
					r_val = (INT16) ((((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07));
					a_val = 0xFF;
					src += 2;
					break;
				case RDP_PIXEL_FORMAT_R5G6B5_LE:
					r_val = (INT16) (((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5));
					g_val = (INT16) ((((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3));
					b_val = (INT16) ((((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07));
					a_val = 0xFF;
					src += 2;
					break;
				case RDP_PIXEL_FORMAT_P4_PLANER:
					{
						int shift;
						BYTE idx;

						shift = (7 - (x % 8));
						idx = ((*src) >> shift) & 1;
						idx |= (((*(src + 1)) >> shift) & 1) << 1;
						idx |= (((*(src + 2)) >> shift) & 1) << 2;
						idx |= (((*(src + 3)) >> shift) & 1) << 3;
						idx *= 3;
						r_val = (INT16) context->palette[idx];
						g_val = (INT16) context->palette[idx + 1];
						b_val = (INT16) context->palette[idx + 2];
						if (shift == 0)
							src += 4;
					}
					a_val = 0xFF;
					break;
				case RDP_PIXEL_FORMAT_P8:
					{
						int idx = (*src) * 3;

						r_val = (INT16) context->palette[idx];
						g_val = (INT16) context->palette[idx + 1];
						b_val = (INT16) context->palette[idx + 2];
						src++;
					}
					a_val = 0xFF;
					break;
				default:
					r_val = g_val = b_val = a_val = 0;
					break;
			}
			*yplane++ = (BYTE) ((r_val >> 2) + (g_val >> 1) + (b_val >> 2));
			/* Perform color loss reduction here */
			*coplane++ = (BYTE) ((r_val - b_val) >> ccl);
			*cgplane++ = (BYTE) ((-(r_val >> 1) + g_val - (b_val >> 1)) >> ccl);
			*aplane++ = a_val;
		}
		if (context->nsc_stream.ChromaSubSamplingLevel > 0 && (x % 2) == 1)
		{
			*yplane = *(yplane - 1);
			*coplane = *(coplane - 1);
			*cgplane = *(cgplane - 1);
		}
	}
	if (context->nsc_stream.ChromaSubSamplingLevel > 0 && (y % 2) == 1)
	{
		memcpy(yplane + rw, yplane, rw);
		memcpy(coplane + rw, coplane, rw);
		memcpy(cgplane + rw, cgplane, rw);
	}
}

static void nsc_encode_subsampling(NSC_CONTEXT* context)
{
	UINT16 x;
	UINT16 y;
	BYTE* co_dst;
	BYTE* cg_dst;
	INT8* co_src0;
	INT8* co_src1;
	INT8* cg_src0;
	INT8* cg_src1;
	UINT32 tempWidth;
	UINT32 tempHeight;

	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);

	for (y = 0; y < tempHeight >> 1; y++)
	{
		co_dst = context->priv->plane_buf[1] + y * (tempWidth >> 1);
		cg_dst = context->priv->plane_buf[2] + y * (tempWidth >> 1);
		co_src0 = (INT8*) context->priv->plane_buf[1] + (y << 1) * tempWidth;
		co_src1 = co_src0 + tempWidth;
		cg_src0 = (INT8*) context->priv->plane_buf[2] + (y << 1) * tempWidth;
		cg_src1 = cg_src0 + tempWidth;
		for (x = 0; x < tempWidth >> 1; x++)
		{
			*co_dst++ = (BYTE) (((INT16) *co_src0 + (INT16) *(co_src0 + 1) +
				(INT16) *co_src1 + (INT16) *(co_src1 + 1)) >> 2);
			*cg_dst++ = (BYTE) (((INT16) *cg_src0 + (INT16) *(cg_src0 + 1) +
				(INT16) *cg_src1 + (INT16) *(cg_src1 + 1)) >> 2);
			co_src0 += 2;
			co_src1 += 2;
			cg_src0 += 2;
			cg_src1 += 2;
		}
	}
}

void nsc_encode(NSC_CONTEXT* context, BYTE* bmpdata, int rowstride)
{
	nsc_encode_argb_to_aycocg(context, bmpdata, rowstride);
	if (context->nsc_stream.ChromaSubSamplingLevel > 0)
	{
		nsc_encode_subsampling(context);
	}
}

static UINT32 nsc_rle_encode(BYTE* in, BYTE* out, UINT32 origsz)
{
	UINT32 left;
	UINT32 runlength = 1;
	UINT32 planesize = 0;

	left = origsz;
	/**
	 * We quit the loop if the running compressed size is larger than the original.
	 * In such cases data will be sent uncompressed.
	 */
	while (left > 4 && planesize < origsz - 4)
	{
		if (left > 5 && *in == *(in + 1))
		{
			runlength++;
		}
		else if (runlength == 1)
		{
			*out++ = *in;
			planesize++;
		}
		else if (runlength < 256)
		{
			*out++ = *in;
			*out++ = *in;
			*out++ = runlength - 2;
			runlength = 1;
			planesize += 3;
		}
		else
		{
			*out++ = *in;
			*out++ = *in;
			*out++ = 0xFF;
			*out++ = (runlength & 0x000000FF);
			*out++ = (runlength & 0x0000FF00) >> 8;
			*out++ = (runlength & 0x00FF0000) >> 16;
			*out++ = (runlength & 0xFF000000) >> 24;
			runlength = 1;
			planesize += 7;
		}
		in++;
		left--;
	}
	if (planesize < origsz - 4)
	{
		memcpy(out, in, 4);
	}
	planesize += 4;

	return planesize;
}

static void nsc_rle_compress_data(NSC_CONTEXT* context)
{
	UINT16 i;
	BYTE* rle;
	UINT32 origsize;
	UINT32 planesize;

	rle = context->nsc_stream.Planes;

	for (i = 0; i < 4; i++)
	{
		origsize = context->OrgByteCount[i];
		if (origsize == 0)
		{
			planesize = 0;
		}
		else
		{
			planesize = nsc_rle_encode(context->priv->plane_buf[i],
				context->priv->plane_buf[4], origsize);
			if (planesize < origsize)
				memcpy(context->priv->plane_buf[i], context->priv->plane_buf[4], planesize);
			else
				planesize = origsize;
		}

		context->nsc_stream.PlaneByteCount[i] = planesize;
	}
}

void nsc_compose_message(NSC_CONTEXT* context, wStream* s,
	BYTE* bmpdata, int width, int height, int rowstride)
{
	int i;

	context->width = width;
	context->height = height;
	nsc_context_initialize_encode(context);

	/* ARGB to AYCoCg conversion, chroma subsampling and colorloss reduction */
	PROFILER_ENTER(context->priv->prof_nsc_encode);
	context->encode(context, bmpdata, rowstride);
	PROFILER_EXIT(context->priv->prof_nsc_encode);

	/* RLE encode */
	PROFILER_ENTER(context->priv->prof_nsc_rle_compress_data);
	nsc_rle_compress_data(context);
	PROFILER_EXIT(context->priv->prof_nsc_rle_compress_data);

	/* Assemble the NSCodec message into stream */
	stream_check_size(s, 20);
	stream_write_UINT32(s, context->nsc_stream.PlaneByteCount[0]); /* LumaPlaneByteCount (4 bytes) */
	stream_write_UINT32(s, context->nsc_stream.PlaneByteCount[1]); /* OrangeChromaPlaneByteCount (4 bytes) */
	stream_write_UINT32(s, context->nsc_stream.PlaneByteCount[2]); /* GreenChromaPlaneByteCount (4 bytes) */
	stream_write_UINT32(s, context->nsc_stream.PlaneByteCount[3]); /* AlphaPlaneByteCount (4 bytes) */
	stream_write_BYTE(s, context->nsc_stream.ColorLossLevel); /* ColorLossLevel (1 byte) */
	stream_write_BYTE(s, context->nsc_stream.ChromaSubSamplingLevel); /* ChromaSubsamplingLevel (1 byte) */
	stream_write_UINT16(s, 0); /* Reserved (2 bytes) */

	for (i = 0; i < 4; i++)
	{
		if (context->nsc_stream.PlaneByteCount[i] > 0)
		{
			stream_check_size(s, (int) context->nsc_stream.PlaneByteCount[i]);
			stream_write(s, context->priv->plane_buf[i], context->nsc_stream.PlaneByteCount[i]);
		}
	}
}
