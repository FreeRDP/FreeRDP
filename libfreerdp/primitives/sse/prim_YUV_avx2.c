/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Optimized YUV/RGB conversion operations using AVX2
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

#include <winpr/sysinfo.h>

#include <freerdp/config.h>

#include <string.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <freerdp/log.h>

#include "prim_internal.h"
#include "prim_YUV.h"

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <immintrin.h>

static primitives_t* generic = nullptr;

/* RGBToI444_8u: one Y, U and V sample per pixel, with the exact integer math of
 * RGB2Y/RGB2U/RGB2V (16 bit lanes are wide enough for every intermediate value),
 * so the result matches the generic routine byte for byte. Same method as the
 * SSE4.1 version, 32 pixels per step. */

/* a vpshufb mask that, in each 128 bit half, moves byte off of each of four 32 bit
 * pixels into the low byte of 16 bit lane first..first+3 of that half */
static inline __m256i avx2_I444_mask(size_t off, size_t first)
{
	BYTE m[16];
	memset(m, 0x80, sizeof(m));
	for (size_t x = 0; x < 4; x++)
		m[2 * (first + x)] = (BYTE)(4 * x + off);
	const __m128i half = _mm_loadu_si128((const __m128i*)m);
	return _mm256_broadcastsi128_si256(half);
}

/* Y, U and V of sixteen pixels given as 16 bit R, G and B lanes */
static inline void avx2_I444_YUV(__m256i r, __m256i g, __m256i b, __m256i* WINPR_RESTRICT y,
                                 __m256i* WINPR_RESTRICT u, __m256i* WINPR_RESTRICT v)
{
	const __m256i c128 = _mm256_set1_epi16(128);

	/* (54 R + 183 G + 18 B) >> 8: at most 65025, exact as unsigned 16 bit */
	__m256i t = _mm256_add_epi16(_mm256_mullo_epi16(r, _mm256_set1_epi16(54)),
	                             _mm256_mullo_epi16(g, _mm256_set1_epi16(183)));
	t = _mm256_add_epi16(t, _mm256_mullo_epi16(b, _mm256_set1_epi16(18)));
	*y = _mm256_srli_epi16(t, 8);

	/* ((128 B - (29 R + 99 G)) >> 8) + 128: both terms are at most 32640 */
	t = _mm256_add_epi16(_mm256_mullo_epi16(r, _mm256_set1_epi16(29)),
	                     _mm256_mullo_epi16(g, _mm256_set1_epi16(99)));
	t = _mm256_sub_epi16(_mm256_slli_epi16(b, 7), t);
	*u = _mm256_add_epi16(_mm256_srai_epi16(t, 8), c128);

	/* ((128 R - (116 G + 12 B)) >> 8) + 128 */
	t = _mm256_add_epi16(_mm256_mullo_epi16(g, _mm256_set1_epi16(116)),
	                     _mm256_mullo_epi16(b, _mm256_set1_epi16(12)));
	t = _mm256_sub_epi16(_mm256_slli_epi16(r, 7), t);
	*v = _mm256_add_epi16(_mm256_srai_epi16(t, 8), c128);
}

/* sixteen pixels from two loads of eight. vpshufb works within 128 bit halves, so
 * the 16 bit lanes hold pixels 0-3, 8-11 | 4-7, 12-15 */
static inline void avx2_I444_16px(const BYTE* WINPR_RESTRICT src, const __m256i masks[6],
                                  __m256i* WINPR_RESTRICT y, __m256i* WINPR_RESTRICT u,
                                  __m256i* WINPR_RESTRICT v)
{
	const __m256i p0 = _mm256_loadu_si256(WINPR_PACKED_ALIGN_CAST(const __m256i*, src));
	const __m256i p1 = _mm256_loadu_si256(WINPR_PACKED_ALIGN_CAST(const __m256i*, &src[32]));
	const __m256i r =
	    _mm256_or_si256(_mm256_shuffle_epi8(p0, masks[0]), _mm256_shuffle_epi8(p1, masks[1]));
	const __m256i g =
	    _mm256_or_si256(_mm256_shuffle_epi8(p0, masks[2]), _mm256_shuffle_epi8(p1, masks[3]));
	const __m256i b =
	    _mm256_or_si256(_mm256_shuffle_epi8(p0, masks[4]), _mm256_shuffle_epi8(p1, masks[5]));
	avx2_I444_YUV(r, g, b, y, u, v);
}

/* pack the 16 bit results of pixels 0-15 and 16-31 into 32 bytes in pixel order.
 * vpackuswb also works within halves; its four byte groups then hold pixels
 * 0-3, 8-11, 16-19, 24-27, 4-7, 12-15, 20-23, 28-31 */
static inline __m256i avx2_I444_pack(__m256i a, __m256i b)
{
	const __m256i order = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);
	return _mm256_permutevar8x32_epi32(_mm256_packus_epi16(a, b), order);
}

static pstatus_t avx2_RGBToI444_32bpp(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcStep,
                                      BYTE* WINPR_RESTRICT pDst[3], const UINT32 dstStep[3],
                                      const prim_size_t* WINPR_RESTRICT roi, size_t rOff,
                                      size_t gOff, size_t bOff)
{
	const __m256i masks[6] = { avx2_I444_mask(rOff, 0), avx2_I444_mask(rOff, 4),
		                       avx2_I444_mask(gOff, 0), avx2_I444_mask(gOff, 4),
		                       avx2_I444_mask(bOff, 0), avx2_I444_mask(bOff, 4) };
	const size_t width = roi->width;

	for (size_t row = 0; row < roi->height; row++)
	{
		const BYTE* src = &pSrc[row * srcStep];
		BYTE* yd = &pDst[0][row * dstStep[0]];
		BYTE* ud = &pDst[1][row * dstStep[1]];
		BYTE* vd = &pDst[2][row * dstStep[2]];

		size_t x = 0;
		for (; x + 32 <= width; x += 32)
		{
			__m256i y0 = _mm256_setzero_si256();
			__m256i u0 = _mm256_setzero_si256();
			__m256i v0 = _mm256_setzero_si256();
			__m256i y1 = _mm256_setzero_si256();
			__m256i u1 = _mm256_setzero_si256();
			__m256i v1 = _mm256_setzero_si256();
			avx2_I444_16px(&src[4 * x], masks, &y0, &u0, &v0);
			avx2_I444_16px(&src[4 * x + 64], masks, &y1, &u1, &v1);

			__m256i* cyd = WINPR_PACKED_ALIGN_CAST(__m256i*, &yd[x]);
			__m256i* cud = WINPR_PACKED_ALIGN_CAST(__m256i*, &ud[x]);
			__m256i* cvd = WINPR_PACKED_ALIGN_CAST(__m256i*, &vd[x]);
			_mm256_storeu_si256(cyd, avx2_I444_pack(y0, y1));
			_mm256_storeu_si256(cud, avx2_I444_pack(u0, u1));
			_mm256_storeu_si256(cvd, avx2_I444_pack(v0, v1));
		}

		for (; x < width; x++)
		{
			const BYTE* px = &src[4 * x];
			const BYTE R = px[rOff];
			const BYTE G = px[gOff];
			const BYTE B = px[bOff];
			yd[x] = RGB2Y(R, G, B);
			ud[x] = RGB2U(R, G, B);
			vd[x] = RGB2V(R, G, B);
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t avx2_RGBToI444_8u(const BYTE* WINPR_RESTRICT pSrc, UINT32 SrcFormat,
                                   const UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
                                   const UINT32 dstStep[3], const prim_size_t* WINPR_RESTRICT roi)
{
	/* byte offsets of R, G and B in memory, as in general_RGBToI444_8u */
	switch (SrcFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return avx2_RGBToI444_32bpp(pSrc, srcStep, pDst, dstStep, roi, 2, 1, 0);
		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return avx2_RGBToI444_32bpp(pSrc, srcStep, pDst, dstStep, roi, 0, 1, 2);
		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return avx2_RGBToI444_32bpp(pSrc, srcStep, pDst, dstStep, roi, 1, 2, 3);
		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return avx2_RGBToI444_32bpp(pSrc, srcStep, pDst, dstStep, roi, 3, 2, 1);
		default:
			return generic->RGBToI444_8u(pSrc, SrcFormat, srcStep, pDst, dstStep, roi);
	}
}
#endif

void primitives_init_YUV_avx2_int(primitives_t* WINPR_RESTRICT prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	generic = primitives_get_generic();

	WLog_VRB(PRIM_TAG, "AVX2 optimizations");
	prims->RGBToI444_8u = avx2_RGBToI444_8u;
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or WITH_AVX2 or AVX2 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
