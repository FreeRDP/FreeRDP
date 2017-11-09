/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Library - SSE2 Optimizations
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

#include <xmmintrin.h>
#include <emmintrin.h>

#include <freerdp/codec/color.h>
#include <winpr/crt.h>

#include "nsc_types.h"
#include "nsc_sse2.h"

static void nsc_encode_argb_to_aycocg_sse2(NSC_CONTEXT* context,
        const BYTE* data, UINT32 scanline)
{
	UINT16 x;
	UINT16 y;
	UINT16 rw;
	BYTE ccl;
	const BYTE* src;
	BYTE* yplane = NULL;
	BYTE* coplane = NULL;
	BYTE* cgplane = NULL;
	BYTE* aplane = NULL;
	__m128i r_val;
	__m128i g_val;
	__m128i b_val;
	__m128i a_val;
	__m128i y_val;
	__m128i co_val;
	__m128i cg_val;
	UINT32 tempWidth;
	UINT32 tempHeight;
	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);
	rw = (context->ChromaSubsamplingLevel > 0 ? tempWidth : context->width);
	ccl = context->ColorLossLevel;

	for (y = 0; y < context->height; y++)
	{
		src = data + (context->height - 1 - y) * scanline;
		yplane = context->priv->PlaneBuffers[0] + y * rw;
		coplane = context->priv->PlaneBuffers[1] + y * rw;
		cgplane = context->priv->PlaneBuffers[2] + y * rw;
		aplane = context->priv->PlaneBuffers[3] + y * context->width;

		for (x = 0; x < context->width; x += 8)
		{
			switch (context->format)
			{
				case PIXEL_FORMAT_BGRX32:
					b_val = _mm_set_epi16(*(src + 28), *(src + 24), *(src + 20), *(src + 16),
					                      *(src + 12), *(src + 8), *(src + 4), *src);
					g_val = _mm_set_epi16(*(src + 29), *(src + 25), *(src + 21), *(src + 17),
					                      *(src + 13), *(src + 9), *(src + 5), *(src + 1));
					r_val = _mm_set_epi16(*(src + 30), *(src + 26), *(src + 22), *(src + 18),
					                      *(src + 14), *(src + 10), *(src + 6), *(src + 2));
					a_val = _mm_set1_epi16(0xFF);
					src += 32;
					break;

				case PIXEL_FORMAT_BGRA32:
					b_val = _mm_set_epi16(*(src + 28), *(src + 24), *(src + 20), *(src + 16),
					                      *(src + 12), *(src + 8), *(src + 4), *src);
					g_val = _mm_set_epi16(*(src + 29), *(src + 25), *(src + 21), *(src + 17),
					                      *(src + 13), *(src + 9), *(src + 5), *(src + 1));
					r_val = _mm_set_epi16(*(src + 30), *(src + 26), *(src + 22), *(src + 18),
					                      *(src + 14), *(src + 10), *(src + 6), *(src + 2));
					a_val = _mm_set_epi16(*(src + 31), *(src + 27), *(src + 23), *(src + 19),
					                      *(src + 15), *(src + 11), *(src + 7), *(src + 3));
					src += 32;
					break;

				case PIXEL_FORMAT_RGBX32:
					r_val = _mm_set_epi16(*(src + 28), *(src + 24), *(src + 20), *(src + 16),
					                      *(src + 12), *(src + 8), *(src + 4), *src);
					g_val = _mm_set_epi16(*(src + 29), *(src + 25), *(src + 21), *(src + 17),
					                      *(src + 13), *(src + 9), *(src + 5), *(src + 1));
					b_val = _mm_set_epi16(*(src + 30), *(src + 26), *(src + 22), *(src + 18),
					                      *(src + 14), *(src + 10), *(src + 6), *(src + 2));
					a_val = _mm_set1_epi16(0xFF);
					src += 32;
					break;

				case PIXEL_FORMAT_RGBA32:
					r_val = _mm_set_epi16(*(src + 28), *(src + 24), *(src + 20), *(src + 16),
					                      *(src + 12), *(src + 8), *(src + 4), *src);
					g_val = _mm_set_epi16(*(src + 29), *(src + 25), *(src + 21), *(src + 17),
					                      *(src + 13), *(src + 9), *(src + 5), *(src + 1));
					b_val = _mm_set_epi16(*(src + 30), *(src + 26), *(src + 22), *(src + 18),
					                      *(src + 14), *(src + 10), *(src + 6), *(src + 2));
					a_val = _mm_set_epi16(*(src + 31), *(src + 27), *(src + 23), *(src + 19),
					                      *(src + 15), *(src + 11), *(src + 7), *(src + 3));
					src += 32;
					break;

				case PIXEL_FORMAT_BGR24:
					b_val = _mm_set_epi16(*(src + 21), *(src + 18), *(src + 15), *(src + 12),
					                      *(src + 9), *(src + 6), *(src + 3), *src);
					g_val = _mm_set_epi16(*(src + 22), *(src + 19), *(src + 16), *(src + 13),
					                      *(src + 10), *(src + 7), *(src + 4), *(src + 1));
					r_val = _mm_set_epi16(*(src + 23), *(src + 20), *(src + 17), *(src + 14),
					                      *(src + 11), *(src + 8), *(src + 5), *(src + 2));
					a_val = _mm_set1_epi16(0xFF);
					src += 24;
					break;

				case PIXEL_FORMAT_RGB24:
					r_val = _mm_set_epi16(*(src + 21), *(src + 18), *(src + 15), *(src + 12),
					                      *(src + 9), *(src + 6), *(src + 3), *src);
					g_val = _mm_set_epi16(*(src + 22), *(src + 19), *(src + 16), *(src + 13),
					                      *(src + 10), *(src + 7), *(src + 4), *(src + 1));
					b_val = _mm_set_epi16(*(src + 23), *(src + 20), *(src + 17), *(src + 14),
					                      *(src + 11), *(src + 8), *(src + 5), *(src + 2));
					a_val = _mm_set1_epi16(0xFF);
					src += 24;
					break;

				case PIXEL_FORMAT_BGR16:
					b_val = _mm_set_epi16(
					            (((*(src + 15)) & 0xF8) | ((*(src + 15)) >> 5)),
					            (((*(src + 13)) & 0xF8) | ((*(src + 13)) >> 5)),
					            (((*(src + 11)) & 0xF8) | ((*(src + 11)) >> 5)),
					            (((*(src + 9)) & 0xF8) | ((*(src + 9)) >> 5)),
					            (((*(src + 7)) & 0xF8) | ((*(src + 7)) >> 5)),
					            (((*(src + 5)) & 0xF8) | ((*(src + 5)) >> 5)),
					            (((*(src + 3)) & 0xF8) | ((*(src + 3)) >> 5)),
					            (((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5)));
					g_val = _mm_set_epi16(
					            ((((*(src + 15)) & 0x07) << 5) | (((*(src + 14)) & 0xE0) >> 3)),
					            ((((*(src + 13)) & 0x07) << 5) | (((*(src + 12)) & 0xE0) >> 3)),
					            ((((*(src + 11)) & 0x07) << 5) | (((*(src + 10)) & 0xE0) >> 3)),
					            ((((*(src + 9)) & 0x07) << 5) | (((*(src + 8)) & 0xE0) >> 3)),
					            ((((*(src + 7)) & 0x07) << 5) | (((*(src + 6)) & 0xE0) >> 3)),
					            ((((*(src + 5)) & 0x07) << 5) | (((*(src + 4)) & 0xE0) >> 3)),
					            ((((*(src + 3)) & 0x07) << 5) | (((*(src + 2)) & 0xE0) >> 3)),
					            ((((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3)));
					r_val = _mm_set_epi16(
					            ((((*(src + 14)) & 0x1F) << 3) | (((*(src + 14)) >> 2) & 0x07)),
					            ((((*(src + 12)) & 0x1F) << 3) | (((*(src + 12)) >> 2) & 0x07)),
					            ((((*(src + 10)) & 0x1F) << 3) | (((*(src + 10)) >> 2) & 0x07)),
					            ((((*(src + 8)) & 0x1F) << 3) | (((*(src + 8)) >> 2) & 0x07)),
					            ((((*(src + 6)) & 0x1F) << 3) | (((*(src + 6)) >> 2) & 0x07)),
					            ((((*(src + 4)) & 0x1F) << 3) | (((*(src + 4)) >> 2) & 0x07)),
					            ((((*(src + 2)) & 0x1F) << 3) | (((*(src + 2)) >> 2) & 0x07)),
					            ((((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07)));
					a_val = _mm_set1_epi16(0xFF);
					src += 16;
					break;

				case PIXEL_FORMAT_RGB16:
					r_val = _mm_set_epi16(
					            (((*(src + 15)) & 0xF8) | ((*(src + 15)) >> 5)),
					            (((*(src + 13)) & 0xF8) | ((*(src + 13)) >> 5)),
					            (((*(src + 11)) & 0xF8) | ((*(src + 11)) >> 5)),
					            (((*(src + 9)) & 0xF8) | ((*(src + 9)) >> 5)),
					            (((*(src + 7)) & 0xF8) | ((*(src + 7)) >> 5)),
					            (((*(src + 5)) & 0xF8) | ((*(src + 5)) >> 5)),
					            (((*(src + 3)) & 0xF8) | ((*(src + 3)) >> 5)),
					            (((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5)));
					g_val = _mm_set_epi16(
					            ((((*(src + 15)) & 0x07) << 5) | (((*(src + 14)) & 0xE0) >> 3)),
					            ((((*(src + 13)) & 0x07) << 5) | (((*(src + 12)) & 0xE0) >> 3)),
					            ((((*(src + 11)) & 0x07) << 5) | (((*(src + 10)) & 0xE0) >> 3)),
					            ((((*(src + 9)) & 0x07) << 5) | (((*(src + 8)) & 0xE0) >> 3)),
					            ((((*(src + 7)) & 0x07) << 5) | (((*(src + 6)) & 0xE0) >> 3)),
					            ((((*(src + 5)) & 0x07) << 5) | (((*(src + 4)) & 0xE0) >> 3)),
					            ((((*(src + 3)) & 0x07) << 5) | (((*(src + 2)) & 0xE0) >> 3)),
					            ((((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3)));
					b_val = _mm_set_epi16(
					            ((((*(src + 14)) & 0x1F) << 3) | (((*(src + 14)) >> 2) & 0x07)),
					            ((((*(src + 12)) & 0x1F) << 3) | (((*(src + 12)) >> 2) & 0x07)),
					            ((((*(src + 10)) & 0x1F) << 3) | (((*(src + 10)) >> 2) & 0x07)),
					            ((((*(src + 8)) & 0x1F) << 3) | (((*(src + 8)) >> 2) & 0x07)),
					            ((((*(src + 6)) & 0x1F) << 3) | (((*(src + 6)) >> 2) & 0x07)),
					            ((((*(src + 4)) & 0x1F) << 3) | (((*(src + 4)) >> 2) & 0x07)),
					            ((((*(src + 2)) & 0x1F) << 3) | (((*(src + 2)) >> 2) & 0x07)),
					            ((((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07)));
					a_val = _mm_set1_epi16(0xFF);
					src += 16;
					break;

				case PIXEL_FORMAT_A4:
					{
						int shift;
						BYTE idx[8];

						for (shift = 7; shift >= 0; shift--)
						{
							idx[shift] = ((*src) >> shift) & 1;
							idx[shift] |= (((*(src + 1)) >> shift) & 1) << 1;
							idx[shift] |= (((*(src + 2)) >> shift) & 1) << 2;
							idx[shift] |= (((*(src + 3)) >> shift) & 1) << 3;
							idx[shift] *= 3;
						}

						r_val = _mm_set_epi16(
						            context->palette[idx[0]],
						            context->palette[idx[1]],
						            context->palette[idx[2]],
						            context->palette[idx[3]],
						            context->palette[idx[4]],
						            context->palette[idx[5]],
						            context->palette[idx[6]],
						            context->palette[idx[7]]);
						g_val = _mm_set_epi16(
						            context->palette[idx[0] + 1],
						            context->palette[idx[1] + 1],
						            context->palette[idx[2] + 1],
						            context->palette[idx[3] + 1],
						            context->palette[idx[4] + 1],
						            context->palette[idx[5] + 1],
						            context->palette[idx[6] + 1],
						            context->palette[idx[7] + 1]);
						b_val = _mm_set_epi16(
						            context->palette[idx[0] + 2],
						            context->palette[idx[1] + 2],
						            context->palette[idx[2] + 2],
						            context->palette[idx[3] + 2],
						            context->palette[idx[4] + 2],
						            context->palette[idx[5] + 2],
						            context->palette[idx[6] + 2],
						            context->palette[idx[7] + 2]);
						src += 4;
					}

					a_val = _mm_set1_epi16(0xFF);
					break;

				case PIXEL_FORMAT_RGB8:
					{
						r_val = _mm_set_epi16(
						            context->palette[(*(src + 7)) * 3],
						            context->palette[(*(src + 6)) * 3],
						            context->palette[(*(src + 5)) * 3],
						            context->palette[(*(src + 4)) * 3],
						            context->palette[(*(src + 3)) * 3],
						            context->palette[(*(src + 2)) * 3],
						            context->palette[(*(src + 1)) * 3],
						            context->palette[(*src) * 3]);
						g_val = _mm_set_epi16(
						            context->palette[(*(src + 7)) * 3 + 1],
						            context->palette[(*(src + 6)) * 3 + 1],
						            context->palette[(*(src + 5)) * 3 + 1],
						            context->palette[(*(src + 4)) * 3 + 1],
						            context->palette[(*(src + 3)) * 3 + 1],
						            context->palette[(*(src + 2)) * 3 + 1],
						            context->palette[(*(src + 1)) * 3 + 1],
						            context->palette[(*src) * 3 + 1]);
						b_val = _mm_set_epi16(
						            context->palette[(*(src + 7)) * 3 + 2],
						            context->palette[(*(src + 6)) * 3 + 2],
						            context->palette[(*(src + 5)) * 3 + 2],
						            context->palette[(*(src + 4)) * 3 + 2],
						            context->palette[(*(src + 3)) * 3 + 2],
						            context->palette[(*(src + 2)) * 3 + 2],
						            context->palette[(*(src + 1)) * 3 + 2],
						            context->palette[(*src) * 3 + 2]);
						src += 8;
					}

					a_val = _mm_set1_epi16(0xFF);
					break;

				default:
					r_val = g_val = b_val = a_val = _mm_set1_epi16(0);
					break;
			}

			y_val = _mm_srai_epi16(r_val, 2);
			y_val = _mm_add_epi16(y_val, _mm_srai_epi16(g_val, 1));
			y_val = _mm_add_epi16(y_val, _mm_srai_epi16(b_val, 2));
			co_val = _mm_sub_epi16(r_val, b_val);
			co_val = _mm_srai_epi16(co_val, ccl);
			cg_val = _mm_sub_epi16(g_val, _mm_srai_epi16(r_val, 1));
			cg_val = _mm_sub_epi16(cg_val, _mm_srai_epi16(b_val, 1));
			cg_val = _mm_srai_epi16(cg_val, ccl);
			y_val = _mm_packus_epi16(y_val, y_val);
			_mm_storeu_si128((__m128i*) yplane, y_val);
			co_val = _mm_packs_epi16(co_val, co_val);
			_mm_storeu_si128((__m128i*) coplane, co_val);
			cg_val = _mm_packs_epi16(cg_val, cg_val);
			_mm_storeu_si128((__m128i*) cgplane, cg_val);
			a_val = _mm_packus_epi16(a_val, a_val);
			_mm_storeu_si128((__m128i*) aplane, a_val);
			yplane += 8;
			coplane += 8;
			cgplane += 8;
			aplane += 8;
		}

		if (context->ChromaSubsamplingLevel > 0 && (context->width % 2) == 1)
		{
			context->priv->PlaneBuffers[0][y * rw + context->width] =
			    context->priv->PlaneBuffers[0][y * rw + context->width - 1];
			context->priv->PlaneBuffers[1][y * rw + context->width] =
			    context->priv->PlaneBuffers[1][y * rw + context->width - 1];
			context->priv->PlaneBuffers[2][y * rw + context->width] =
			    context->priv->PlaneBuffers[2][y * rw + context->width - 1];
		}
	}

	if (context->ChromaSubsamplingLevel > 0 && (y % 2) == 1)
	{
		yplane = context->priv->PlaneBuffers[0] + y * rw;
		coplane = context->priv->PlaneBuffers[1] + y * rw;
		cgplane = context->priv->PlaneBuffers[2] + y * rw;

		CopyMemory(yplane, yplane - rw, rw);
		CopyMemory(coplane, coplane - rw, rw);
		CopyMemory(cgplane, cgplane - rw, rw);
	}
}

static void nsc_encode_subsampling_sse2(NSC_CONTEXT* context)
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
	__m128i t;
	__m128i val;
	__m128i mask = _mm_set1_epi16(0xFF);
	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);

	for (y = 0; y < tempHeight >> 1; y++)
	{
		co_dst = context->priv->PlaneBuffers[1] + y * (tempWidth >> 1);
		cg_dst = context->priv->PlaneBuffers[2] + y * (tempWidth >> 1);
		co_src0 = (INT8*) context->priv->PlaneBuffers[1] + (y << 1) * tempWidth;
		co_src1 = co_src0 + tempWidth;
		cg_src0 = (INT8*) context->priv->PlaneBuffers[2] + (y << 1) * tempWidth;
		cg_src1 = cg_src0 + tempWidth;

		for (x = 0; x < tempWidth >> 1; x += 8)
		{
			t = _mm_loadu_si128((__m128i*) co_src0);
			t = _mm_avg_epu8(t, _mm_loadu_si128((__m128i*) co_src1));
			val = _mm_and_si128(_mm_srli_si128(t, 1), mask);
			val = _mm_avg_epu16(val, _mm_and_si128(t, mask));
			val = _mm_packus_epi16(val, val);
			_mm_storeu_si128((__m128i*) co_dst, val);
			co_dst += 8;
			co_src0 += 16;
			co_src1 += 16;
			t = _mm_loadu_si128((__m128i*) cg_src0);
			t = _mm_avg_epu8(t, _mm_loadu_si128((__m128i*) cg_src1));
			val = _mm_and_si128(_mm_srli_si128(t, 1), mask);
			val = _mm_avg_epu16(val, _mm_and_si128(t, mask));
			val = _mm_packus_epi16(val, val);
			_mm_storeu_si128((__m128i*) cg_dst, val);
			cg_dst += 8;
			cg_src0 += 16;
			cg_src1 += 16;
		}
	}
}

static void nsc_encode_sse2(NSC_CONTEXT* context, const BYTE* data,
                            UINT32 scanline)
{
	nsc_encode_argb_to_aycocg_sse2(context, data, scanline);

	if (context->ChromaSubsamplingLevel > 0)
	{
		nsc_encode_subsampling_sse2(context);
	}
}

void nsc_init_sse2(NSC_CONTEXT* context)
{
	IF_PROFILER(context->priv->prof_nsc_encode->name = "nsc_encode_sse2");
	context->encode = nsc_encode_sse2;
}
