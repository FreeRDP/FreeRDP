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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/codec/nsc.h>
#include <freerdp/codec/color.h>

#include "nsc_types.h"
#include "nsc_encode.h"

#include "nsc_sse2.h"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("codec.nsc")

#ifndef NSC_INIT_SIMD
#define NSC_INIT_SIMD(_nsc_context) \
	do                              \
	{                               \
	} while (0)
#endif

static BOOL nsc_decode(NSC_CONTEXT* context)
{
	UINT16 x;
	UINT16 y;
	UINT16 rw;
	BYTE shift;
	BYTE* bmpdata;
	size_t pos = 0;

	if (!context)
		return FALSE;

	rw = ROUND_UP_TO(context->width, 8);
	shift = context->ColorLossLevel - 1; /* colorloss recovery + YCoCg shift */
	bmpdata = context->BitmapData;

	if (!bmpdata)
		return FALSE;

	for (y = 0; y < context->height; y++)
	{
		const BYTE* yplane;
		const BYTE* coplane;
		const BYTE* cgplane;
		const BYTE* aplane = context->priv->PlaneBuffers[3] + y * context->width; /* A */

		if (context->ChromaSubsamplingLevel)
		{
			yplane = context->priv->PlaneBuffers[0] + y * rw;                /* Y */
			coplane = context->priv->PlaneBuffers[1] + (y >> 1) * (rw >> 1); /* Co, supersampled */
			cgplane = context->priv->PlaneBuffers[2] + (y >> 1) * (rw >> 1); /* Cg, supersampled */
		}
		else
		{
			yplane = context->priv->PlaneBuffers[0] + y * context->width;  /* Y */
			coplane = context->priv->PlaneBuffers[1] + y * context->width; /* Co */
			cgplane = context->priv->PlaneBuffers[2] + y * context->width; /* Cg */
		}

		for (x = 0; x < context->width; x++)
		{
			INT16 y_val = (INT16)*yplane;
			INT16 co_val = (INT16)(INT8)(*coplane << shift);
			INT16 cg_val = (INT16)(INT8)(*cgplane << shift);
			INT16 r_val = y_val + co_val - cg_val;
			INT16 g_val = y_val + cg_val;
			INT16 b_val = y_val - co_val - cg_val;

			if (pos + 4 > context->BitmapDataLength)
				return FALSE;

			pos += 4;
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

	return TRUE;
}

static BOOL nsc_rle_decode(BYTE* in, BYTE* out, UINT32 outSize, UINT32 originalSize)
{
	UINT32 left = originalSize;

	while (left > 4)
	{
		const BYTE value = *in++;
		UINT32 len = 0;

		if (left == 5)
		{
			if (outSize < 1)
				return FALSE;

			outSize--;
			*out++ = value;
			left--;
		}
		else if (value == *in)
		{
			in++;

			if (*in < 0xFF)
			{
				len = (UINT32)*in++;
				len += 2;
			}
			else
			{
				in++;
				len = ((UINT32)(*in++));
				len |= ((UINT32)(*in++)) << 8U;
				len |= ((UINT32)(*in++)) << 16U;
				len |= ((UINT32)(*in++)) << 24U;
			}

			if (outSize < len)
				return FALSE;

			outSize -= len;
			FillMemory(out, len, value);
			out += len;
			left -= len;
		}
		else
		{
			if (outSize < 1)
				return FALSE;

			outSize--;
			*out++ = value;
			left--;
		}
	}

	if ((outSize < 4) || (left < 4))
		return FALSE;

	memcpy(out, in, 4);
	return TRUE;
}

static BOOL nsc_rle_decompress_data(NSC_CONTEXT* context)
{
	UINT16 i;
	BYTE* rle;
	UINT32 planeSize;
	UINT32 originalSize;

	if (!context)
		return FALSE;

	rle = context->Planes;

	for (i = 0; i < 4; i++)
	{
		originalSize = context->OrgByteCount[i];
		planeSize = context->PlaneByteCount[i];

		if (planeSize == 0)
		{
			if (context->priv->PlaneBuffersLength < originalSize)
				return FALSE;

			FillMemory(context->priv->PlaneBuffers[i], originalSize, 0xFF);
		}
		else if (planeSize < originalSize)
		{
			if (!nsc_rle_decode(rle, context->priv->PlaneBuffers[i],
			                    context->priv->PlaneBuffersLength, originalSize))
				return FALSE;
		}
		else
		{
			if (context->priv->PlaneBuffersLength < originalSize)
				return FALSE;

			CopyMemory(context->priv->PlaneBuffers[i], rle, originalSize);
		}

		rle += planeSize;
	}

	return TRUE;
}

static BOOL nsc_stream_initialize(NSC_CONTEXT* context, wStream* s)
{
	int i;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return FALSE;

	for (i = 0; i < 4; i++)
		Stream_Read_UINT32(s, context->PlaneByteCount[i]);

	Stream_Read_UINT8(s, context->ColorLossLevel);         /* ColorLossLevel (1 byte) */
	Stream_Read_UINT8(s, context->ChromaSubsamplingLevel); /* ChromaSubsamplingLevel (1 byte) */
	Stream_Seek(s, 2);                                     /* Reserved (2 bytes) */
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
			void* tmp = (BYTE*)realloc(context->priv->PlaneBuffers[i], length);

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

static void nsc_profiler_print(NSC_CONTEXT_PRIV* priv)
{
	WINPR_UNUSED(priv);

	PROFILER_PRINT_HEADER
	PROFILER_PRINT(priv->prof_nsc_rle_decompress_data)
	PROFILER_PRINT(priv->prof_nsc_decode)
	PROFILER_PRINT(priv->prof_nsc_rle_compress_data)
	PROFILER_PRINT(priv->prof_nsc_encode)
	PROFILER_PRINT_FOOTER
}

BOOL nsc_context_reset(NSC_CONTEXT* context, UINT32 width, UINT32 height)
{
	if (!context)
		return FALSE;

	if ((width > UINT16_MAX) || (height > UINT16_MAX))
		return FALSE;

	context->width = (UINT16)width;
	context->height = (UINT16)height;
	return TRUE;
}

NSC_CONTEXT* nsc_context_new(void)
{
	NSC_CONTEXT* context;
	context = (NSC_CONTEXT*)calloc(1, sizeof(NSC_CONTEXT));

	if (!context)
		return NULL;

	context->priv = (NSC_CONTEXT_PRIV*)calloc(1, sizeof(NSC_CONTEXT_PRIV));

	if (!context->priv)
		goto error;

	context->priv->log = WLog_Get("com.freerdp.codec.nsc");
	WLog_OpenAppender(context->priv->log);
	context->BitmapData = NULL;
	context->decode = nsc_decode;
	context->encode = nsc_encode;

	PROFILER_CREATE(context->priv->prof_nsc_rle_decompress_data, "nsc_rle_decompress_data")
	PROFILER_CREATE(context->priv->prof_nsc_decode, "nsc_decode")
	PROFILER_CREATE(context->priv->prof_nsc_rle_compress_data, "nsc_rle_compress_data")
	PROFILER_CREATE(context->priv->prof_nsc_encode, "nsc_encode")
	/* Default encoding parameters */
	context->ColorLossLevel = 3;
	context->ChromaSubsamplingLevel = 1;
	/* init optimized methods */
	NSC_INIT_SIMD(context);
	return context;
error:
	nsc_context_free(context);
	return NULL;
}

void nsc_context_free(NSC_CONTEXT* context)
{
	size_t i;

	if (!context)
		return;

	if (context->priv)
	{
		for (i = 0; i < 5; i++)
			free(context->priv->PlaneBuffers[i]);

		nsc_profiler_print(context->priv);
		PROFILER_FREE(context->priv->prof_nsc_rle_decompress_data)
		PROFILER_FREE(context->priv->prof_nsc_decode)
		PROFILER_FREE(context->priv->prof_nsc_rle_compress_data)
		PROFILER_FREE(context->priv->prof_nsc_encode)
		free(context->priv);
	}

	free(context->BitmapData);
	free(context);
}

#if defined(WITH_FREERDP_DEPRECATED)
BOOL nsc_context_set_pixel_format(NSC_CONTEXT* context, UINT32 pixel_format)
{
	return nsc_context_set_parameters(context, NSC_COLOR_FORMAT, pixel_format);
}
#endif

BOOL nsc_context_set_parameters(NSC_CONTEXT* context, NSC_PARAMETER what, UINT32 value)
{
	if (!context)
		return FALSE;

	switch (what)
	{
		case NSC_COLOR_LOSS_LEVEL:
			context->ColorLossLevel = value;
			break;
		case NSC_ALLOW_SUBSAMPLING:
			context->ChromaSubsamplingLevel = value;
			break;
		case NSC_DYNAMIC_COLOR_FIDELITY:
			context->DynamicColorFidelity = value != 0;
			break;
		case NSC_COLOR_FORMAT:
			context->format = value;
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

BOOL nsc_process_message(NSC_CONTEXT* context, UINT16 bpp, UINT32 width, UINT32 height,
                         const BYTE* data, UINT32 length, BYTE* pDstData, UINT32 DstFormat,
                         UINT32 nDstStride, UINT32 nXDst, UINT32 nYDst, UINT32 nWidth,
                         UINT32 nHeight, UINT32 flip)
{
	wStream* s;
	wStream sbuffer = { 0 };
	BOOL ret;
	if (!context || !data || !pDstData)
		return FALSE;

	s = Stream_StaticConstInit(&sbuffer, data, length);

	if (!s)
		return FALSE;

	if (nDstStride == 0)
		nDstStride = nWidth * FreeRDPGetBytesPerPixel(DstFormat);

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

	if (!ret)
		return FALSE;

	/* RLE decode */
	{
		BOOL rc;
		PROFILER_ENTER(context->priv->prof_nsc_rle_decompress_data)
		rc = nsc_rle_decompress_data(context);
		PROFILER_EXIT(context->priv->prof_nsc_rle_decompress_data)

		if (!rc)
			return FALSE;
	}
	/* Colorloss recover, Chroma supersample and AYCoCg to ARGB Conversion in one step */
	{
		BOOL rc;
		PROFILER_ENTER(context->priv->prof_nsc_decode)
		rc = context->decode(context);
		PROFILER_EXIT(context->priv->prof_nsc_decode)

		if (!rc)
			return FALSE;
	}

	if (!freerdp_image_copy(pDstData, DstFormat, nDstStride, nXDst, nYDst, width, height,
	                        context->BitmapData, PIXEL_FORMAT_BGRA32, 0, 0, 0, NULL, flip))
		return FALSE;

	return TRUE;
}
