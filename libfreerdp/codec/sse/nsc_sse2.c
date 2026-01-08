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

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/platform.h>
#include <freerdp/config.h>

#include "../nsc_types.h"
#include "nsc_sse2.h"

#include "../../core/simd.h"
#include "../../primitives/sse/prim_avxsse.h"

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#include <freerdp/codec/color.h>
#include <winpr/crt.h>
#include <winpr/sysinfo.h>

static inline size_t nsc_encode_next_bgrx32(const BYTE* src, __m128i* r_val, __m128i* g_val,
                                            __m128i* b_val, __m128i* a_val)
{
	*b_val = _mm_set_epi16(*(src + 28), *(src + 24), *(src + 20), *(src + 16), *(src + 12),
	                       *(src + 8), *(src + 4), *src);
	*g_val = _mm_set_epi16(*(src + 29), *(src + 25), *(src + 21), *(src + 17), *(src + 13),
	                       *(src + 9), *(src + 5), *(src + 1));
	*r_val = _mm_set_epi16(*(src + 30), *(src + 26), *(src + 22), *(src + 18), *(src + 14),
	                       *(src + 10), *(src + 6), *(src + 2));
	*a_val = _mm_set1_epi16(0xFF);
	return 32;
}

static inline size_t nsc_encode_next_bgra32(const BYTE* src, __m128i* r_val, __m128i* g_val,
                                            __m128i* b_val, __m128i* a_val)
{
	*b_val = _mm_set_epi16(*(src + 28), *(src + 24), *(src + 20), *(src + 16), *(src + 12),
	                       *(src + 8), *(src + 4), *src);
	*g_val = _mm_set_epi16(*(src + 29), *(src + 25), *(src + 21), *(src + 17), *(src + 13),
	                       *(src + 9), *(src + 5), *(src + 1));
	*r_val = _mm_set_epi16(*(src + 30), *(src + 26), *(src + 22), *(src + 18), *(src + 14),
	                       *(src + 10), *(src + 6), *(src + 2));
	*a_val = _mm_set_epi16(*(src + 31), *(src + 27), *(src + 23), *(src + 19), *(src + 15),
	                       *(src + 11), *(src + 7), *(src + 3));
	return 32;
}

static inline size_t nsc_encode_next_rgbx32(const BYTE* src, __m128i* r_val, __m128i* g_val,
                                            __m128i* b_val, __m128i* a_val)
{
	*r_val = _mm_set_epi16(*(src + 28), *(src + 24), *(src + 20), *(src + 16), *(src + 12),
	                       *(src + 8), *(src + 4), *src);
	*g_val = _mm_set_epi16(*(src + 29), *(src + 25), *(src + 21), *(src + 17), *(src + 13),
	                       *(src + 9), *(src + 5), *(src + 1));
	*b_val = _mm_set_epi16(*(src + 30), *(src + 26), *(src + 22), *(src + 18), *(src + 14),
	                       *(src + 10), *(src + 6), *(src + 2));
	*a_val = _mm_set1_epi16(0xFF);
	return 32;
}

static inline size_t nsc_encode_next_rgba32(const BYTE* src, __m128i* r_val, __m128i* g_val,
                                            __m128i* b_val, __m128i* a_val)
{
	*r_val = _mm_set_epi16(*(src + 28), *(src + 24), *(src + 20), *(src + 16), *(src + 12),
	                       *(src + 8), *(src + 4), *src);
	*g_val = _mm_set_epi16(*(src + 29), *(src + 25), *(src + 21), *(src + 17), *(src + 13),
	                       *(src + 9), *(src + 5), *(src + 1));
	*b_val = _mm_set_epi16(*(src + 30), *(src + 26), *(src + 22), *(src + 18), *(src + 14),
	                       *(src + 10), *(src + 6), *(src + 2));
	*a_val = _mm_set_epi16(*(src + 31), *(src + 27), *(src + 23), *(src + 19), *(src + 15),
	                       *(src + 11), *(src + 7), *(src + 3));
	return 32;
}

static inline size_t nsc_encode_next_bgr24(const BYTE* src, __m128i* r_val, __m128i* g_val,
                                           __m128i* b_val, __m128i* a_val)
{
	*b_val = _mm_set_epi16(*(src + 21), *(src + 18), *(src + 15), *(src + 12), *(src + 9),
	                       *(src + 6), *(src + 3), *src);
	*g_val = _mm_set_epi16(*(src + 22), *(src + 19), *(src + 16), *(src + 13), *(src + 10),
	                       *(src + 7), *(src + 4), *(src + 1));
	*r_val = _mm_set_epi16(*(src + 23), *(src + 20), *(src + 17), *(src + 14), *(src + 11),
	                       *(src + 8), *(src + 5), *(src + 2));
	*a_val = _mm_set1_epi16(0xFF);
	return 24;
}

static inline size_t nsc_encode_next_rgb24(const BYTE* src, __m128i* r_val, __m128i* g_val,
                                           __m128i* b_val, __m128i* a_val)
{
	*r_val = _mm_set_epi16(*(src + 21), *(src + 18), *(src + 15), *(src + 12), *(src + 9),
	                       *(src + 6), *(src + 3), *src);
	*g_val = _mm_set_epi16(*(src + 22), *(src + 19), *(src + 16), *(src + 13), *(src + 10),
	                       *(src + 7), *(src + 4), *(src + 1));
	*b_val = _mm_set_epi16(*(src + 23), *(src + 20), *(src + 17), *(src + 14), *(src + 11),
	                       *(src + 8), *(src + 5), *(src + 2));
	*a_val = _mm_set1_epi16(0xFF);
	return 24;
}

static inline size_t nsc_encode_next_bgr16(const BYTE* src, __m128i* r_val, __m128i* g_val,
                                           __m128i* b_val, __m128i* a_val)
{
	*b_val = _mm_set_epi16(
	    WINPR_ASSERTING_INT_CAST(INT16, ((*(src + 15)) & 0xF8) | ((*(src + 15)) >> 5)),
	    WINPR_ASSERTING_INT_CAST(INT16, ((*(src + 13)) & 0xF8) | ((*(src + 13)) >> 5)),
	    WINPR_ASSERTING_INT_CAST(INT16, ((*(src + 11)) & 0xF8) | ((*(src + 11)) >> 5)),
	    WINPR_ASSERTING_INT_CAST(INT16, ((*(src + 9)) & 0xF8) | ((*(src + 9)) >> 5)),
	    WINPR_ASSERTING_INT_CAST(INT16, ((*(src + 7)) & 0xF8) | ((*(src + 7)) >> 5)),
	    WINPR_ASSERTING_INT_CAST(INT16, ((*(src + 5)) & 0xF8) | ((*(src + 5)) >> 5)),
	    WINPR_ASSERTING_INT_CAST(INT16, ((*(src + 3)) & 0xF8) | ((*(src + 3)) >> 5)),
	    WINPR_ASSERTING_INT_CAST(INT16, ((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5)));
	*g_val = _mm_set_epi16(
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 15)) & 0x07) << 5) | (((*(src + 14)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 13)) & 0x07) << 5) | (((*(src + 12)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 11)) & 0x07) << 5) | (((*(src + 10)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 9)) & 0x07) << 5) | (((*(src + 8)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 7)) & 0x07) << 5) | (((*(src + 6)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 5)) & 0x07) << 5) | (((*(src + 4)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 3)) & 0x07) << 5) | (((*(src + 2)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16, (((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3)));
	*r_val = _mm_set_epi16(
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 14)) & 0x1F) << 3) | (((*(src + 14)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 12)) & 0x1F) << 3) | (((*(src + 12)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 10)) & 0x1F) << 3) | (((*(src + 10)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 8)) & 0x1F) << 3) | (((*(src + 8)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 6)) & 0x1F) << 3) | (((*(src + 6)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 4)) & 0x1F) << 3) | (((*(src + 4)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 2)) & 0x1F) << 3) | (((*(src + 2)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16, (((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07)));
	*a_val = _mm_set1_epi16(0xFF);
	return 16;
}

static inline size_t nsc_encode_next_rgb16(const BYTE* src, __m128i* r_val, __m128i* g_val,
                                           __m128i* b_val, __m128i* a_val)
{
	*r_val = _mm_set_epi16(WINPR_ASSERTING_INT_CAST(INT16, ((src[15] & 0xF8) | (src[15] >> 5))),
	                       WINPR_ASSERTING_INT_CAST(INT16, ((src[13] & 0xF8) | (src[13] >> 5))),
	                       WINPR_ASSERTING_INT_CAST(INT16, ((src[11] & 0xF8) | (src[11] >> 5))),
	                       WINPR_ASSERTING_INT_CAST(INT16, ((src[9] & 0xF8) | (src[9] >> 5))),
	                       WINPR_ASSERTING_INT_CAST(INT16, ((src[7] & 0xF8) | (src[7] >> 5))),
	                       WINPR_ASSERTING_INT_CAST(INT16, ((src[5] & 0xF8) | (src[5] >> 5))),
	                       WINPR_ASSERTING_INT_CAST(INT16, ((src[3] & 0xF8) | (src[3] >> 5))),
	                       WINPR_ASSERTING_INT_CAST(INT16, ((src[1] & 0xF8) | (src[1] >> 5))));
	*g_val = _mm_set_epi16(
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 15)) & 0x07) << 5) | (((*(src + 14)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 13)) & 0x07) << 5) | (((*(src + 12)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 11)) & 0x07) << 5) | (((*(src + 10)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 9)) & 0x07) << 5) | (((*(src + 8)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 7)) & 0x07) << 5) | (((*(src + 6)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 5)) & 0x07) << 5) | (((*(src + 4)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 3)) & 0x07) << 5) | (((*(src + 2)) & 0xE0) >> 3)),
	    WINPR_ASSERTING_INT_CAST(INT16, (((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3)));
	*b_val = _mm_set_epi16(
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 14)) & 0x1F) << 3) | (((*(src + 14)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 12)) & 0x1F) << 3) | (((*(src + 12)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 10)) & 0x1F) << 3) | (((*(src + 10)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 8)) & 0x1F) << 3) | (((*(src + 8)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 6)) & 0x1F) << 3) | (((*(src + 6)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 4)) & 0x1F) << 3) | (((*(src + 4)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16,
	                             (((*(src + 2)) & 0x1F) << 3) | (((*(src + 2)) >> 2) & 0x07)),
	    WINPR_ASSERTING_INT_CAST(INT16, (((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07)));
	*a_val = _mm_set1_epi16(0xFF);
	return 16;
}

static inline size_t nsc_encode_next_a4(const BYTE* src, const BYTE* palette, __m128i* r_val,
                                        __m128i* g_val, __m128i* b_val, __m128i* a_val)
{
	BYTE idx[8] = { 0 };

	for (int shift = 7; shift >= 0; shift--)
	{
		idx[shift] = ((*src) >> shift) & 1;
		idx[shift] |= (((*(src + 1)) >> shift) & 1) << 1;
		idx[shift] |= (((*(src + 2)) >> shift) & 1) << 2;
		idx[shift] |= (((*(src + 3)) >> shift) & 1) << 3;
		idx[shift] *= 3;
	}

	*r_val = _mm_set_epi16(palette[idx[0]], palette[idx[1]], palette[idx[2]], palette[idx[3]],
	                       palette[idx[4]], palette[idx[5]], palette[idx[6]], palette[idx[7]]);
	*g_val = _mm_set_epi16(palette[idx[0] + 1], palette[idx[1] + 1], palette[idx[2] + 1],
	                       palette[idx[3] + 1], palette[idx[4] + 1], palette[idx[5] + 1],
	                       palette[idx[6] + 1], palette[idx[7] + 1]);
	*b_val = _mm_set_epi16(palette[idx[0] + 2], palette[idx[1] + 2], palette[idx[2] + 2],
	                       palette[idx[3] + 2], palette[idx[4] + 2], palette[idx[5] + 2],
	                       palette[idx[6] + 2], palette[idx[7] + 2]);
	*a_val = _mm_set1_epi16(0xFF);
	return 4;
}

static inline size_t nsc_encode_next_rgb8(const BYTE* src, const BYTE* palette, __m128i* r_val,
                                          __m128i* g_val, __m128i* b_val, __m128i* a_val)
{
	*r_val = _mm_set_epi16(palette[(*(src + 7ULL)) * 3ULL], palette[(*(src + 6ULL)) * 3ULL],
	                       palette[(*(src + 5ULL)) * 3ULL], palette[(*(src + 4ULL)) * 3ULL],
	                       palette[(*(src + 3ULL)) * 3ULL], palette[(*(src + 2ULL)) * 3ULL],
	                       palette[(*(src + 1ULL)) * 3ULL], palette[(*src) * 3ULL]);
	*g_val = _mm_set_epi16(
	    palette[(*(src + 7ULL)) * 3ULL + 1ULL], palette[(*(src + 6ULL)) * 3ULL + 1ULL],
	    palette[(*(src + 5ULL)) * 3ULL + 1ULL], palette[(*(src + 4ULL)) * 3ULL + 1ULL],
	    palette[(*(src + 3ULL)) * 3ULL + 1ULL], palette[(*(src + 2ULL)) * 3ULL + 1ULL],
	    palette[(*(src + 1ULL)) * 3ULL + 1ULL], palette[(*src) * 3ULL + 1ULL]);
	*b_val = _mm_set_epi16(
	    palette[(*(src + 7ULL)) * 3ULL + 2ULL], palette[(*(src + 6ULL)) * 3ULL + 2ULL],
	    palette[(*(src + 5ULL)) * 3ULL + 2ULL], palette[(*(src + 4ULL)) * 3ULL + 2ULL],
	    palette[(*(src + 3ULL)) * 3ULL + 2ULL], palette[(*(src + 2ULL)) * 3ULL + 2ULL],
	    palette[(*(src + 1ULL)) * 3ULL + 2ULL], palette[(*src) * 3ULL + 2ULL]);
	*a_val = _mm_set1_epi16(0xFF);
	return 8;
}

static inline size_t nsc_encode_next_rgba(UINT32 format, const BYTE* src, const BYTE* palette,
                                          __m128i* r_val, __m128i* g_val, __m128i* b_val,
                                          __m128i* a_val)
{
	switch (format)
	{
		case PIXEL_FORMAT_BGRX32:
			return nsc_encode_next_bgrx32(src, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_BGRA32:
			return nsc_encode_next_bgra32(src, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_RGBX32:
			return nsc_encode_next_rgbx32(src, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_RGBA32:
			return nsc_encode_next_rgba32(src, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_BGR24:
			return nsc_encode_next_bgr24(src, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_RGB24:
			return nsc_encode_next_rgb24(src, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_BGR16:
			return nsc_encode_next_bgr16(src, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_RGB16:
			return nsc_encode_next_rgb16(src, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_A4:
			return nsc_encode_next_a4(src, palette, r_val, g_val, b_val, a_val);

		case PIXEL_FORMAT_RGB8:
			return nsc_encode_next_rgb8(src, palette, r_val, g_val, b_val, a_val);

		default:
			return 0;
	}
}

static BOOL nsc_encode_argb_to_aycocg_sse2(NSC_CONTEXT* context, const BYTE* data, UINT32 scanline)
{
	size_t y = 0;

	if (!context || !data || (scanline == 0))
		return FALSE;

	const UINT16 tempWidth = ROUND_UP_TO(context->width, 8);
	const UINT16 rw = (context->ChromaSubsamplingLevel > 0 ? tempWidth : context->width);

	const BYTE ccl = WINPR_ASSERTING_INT_CAST(BYTE, context->ColorLossLevel);

	for (; y < context->height; y++)
	{
		const BYTE* src = data + (context->height - 1 - y) * scanline;
		BYTE* yplane = context->priv->PlaneBuffers[0] + y * rw;
		BYTE* coplane = context->priv->PlaneBuffers[1] + y * rw;
		BYTE* cgplane = context->priv->PlaneBuffers[2] + y * rw;
		BYTE* aplane = context->priv->PlaneBuffers[3] + y * context->width;

		for (UINT16 x = 0; x < context->width; x += 8)
		{
			__m128i r_val = { 0 };
			__m128i g_val = { 0 };
			__m128i b_val = { 0 };
			__m128i a_val = { 0 };

			const size_t rc = nsc_encode_next_rgba(context->format, src, context->palette, &r_val,
			                                       &g_val, &b_val, &a_val);
			src += rc;

			__m128i y_val = _mm_srai_epi16(r_val, 2);
			y_val = _mm_add_epi16(y_val, _mm_srai_epi16(g_val, 1));
			y_val = _mm_add_epi16(y_val, _mm_srai_epi16(b_val, 2));
			__m128i co_val = _mm_sub_epi16(r_val, b_val);
			co_val = _mm_srai_epi16(co_val, ccl);
			__m128i cg_val = _mm_sub_epi16(g_val, _mm_srai_epi16(r_val, 1));
			cg_val = _mm_sub_epi16(cg_val, _mm_srai_epi16(b_val, 1));
			cg_val = _mm_srai_epi16(cg_val, ccl);
			y_val = _mm_packus_epi16(y_val, y_val);
			STORE_SI128(yplane, y_val);
			co_val = _mm_packs_epi16(co_val, co_val);
			STORE_SI128(coplane, co_val);
			cg_val = _mm_packs_epi16(cg_val, cg_val);
			STORE_SI128(cgplane, cg_val);
			a_val = _mm_packus_epi16(a_val, a_val);
			STORE_SI128(aplane, a_val);
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
		BYTE* yplane = context->priv->PlaneBuffers[0] + y * rw;
		BYTE* coplane = context->priv->PlaneBuffers[1] + y * rw;
		BYTE* cgplane = context->priv->PlaneBuffers[2] + y * rw;
		CopyMemory(yplane, yplane - rw, rw);
		CopyMemory(coplane, coplane - rw, rw);
		CopyMemory(cgplane, cgplane - rw, rw);
	}

	return TRUE;
}

static void nsc_encode_subsampling_sse2(NSC_CONTEXT* context)
{
	BYTE* co_dst = NULL;
	BYTE* cg_dst = NULL;
	INT8* co_src0 = NULL;
	INT8* co_src1 = NULL;
	INT8* cg_src0 = NULL;
	INT8* cg_src1 = NULL;
	UINT32 tempWidth = 0;
	UINT32 tempHeight = 0;
	__m128i t;
	__m128i val;
	__m128i mask = _mm_set1_epi16(0xFF);
	tempWidth = ROUND_UP_TO(context->width, 8);
	tempHeight = ROUND_UP_TO(context->height, 2);

	for (size_t y = 0; y < tempHeight >> 1; y++)
	{
		co_dst = context->priv->PlaneBuffers[1] + y * (tempWidth >> 1);
		cg_dst = context->priv->PlaneBuffers[2] + y * (tempWidth >> 1);
		co_src0 = (INT8*)context->priv->PlaneBuffers[1] + (y << 1) * tempWidth;
		co_src1 = co_src0 + tempWidth;
		cg_src0 = (INT8*)context->priv->PlaneBuffers[2] + (y << 1) * tempWidth;
		cg_src1 = cg_src0 + tempWidth;

		for (UINT32 x = 0; x < tempWidth >> 1; x += 8)
		{
			t = LOAD_SI128(co_src0);
			t = _mm_avg_epu8(t, LOAD_SI128(co_src1));
			val = _mm_and_si128(_mm_srli_si128(t, 1), mask);
			val = _mm_avg_epu16(val, _mm_and_si128(t, mask));
			val = _mm_packus_epi16(val, val);
			STORE_SI128(co_dst, val);
			co_dst += 8;
			co_src0 += 16;
			co_src1 += 16;
			t = LOAD_SI128(cg_src0);
			t = _mm_avg_epu8(t, LOAD_SI128(cg_src1));
			val = _mm_and_si128(_mm_srli_si128(t, 1), mask);
			val = _mm_avg_epu16(val, _mm_and_si128(t, mask));
			val = _mm_packus_epi16(val, val);
			STORE_SI128(cg_dst, val);
			cg_dst += 8;
			cg_src0 += 16;
			cg_src1 += 16;
		}
	}
}

static BOOL nsc_encode_sse2(NSC_CONTEXT* WINPR_RESTRICT context, const BYTE* WINPR_RESTRICT data,
                            UINT32 scanline)
{
	if (!nsc_encode_argb_to_aycocg_sse2(context, data, scanline))
		return FALSE;

	if (context->ChromaSubsamplingLevel > 0)
		nsc_encode_subsampling_sse2(context);

	return TRUE;
}
#endif

void nsc_init_sse2_int(NSC_CONTEXT* WINPR_RESTRICT context)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	WLog_VRB(PRIM_TAG, "SSE2/SSE3 optimizations");
	PROFILER_RENAME(context->priv->prof_nsc_encode, "nsc_encode_sse2")
	context->encode = nsc_encode_sse2;
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or SSE2 intrinsics not available");
	WINPR_UNUSED(context);
#endif
}
