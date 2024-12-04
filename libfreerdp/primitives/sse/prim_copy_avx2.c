/* FreeRDP: A Remote Desktop Protocol Client
 * Copy operations.
 * vi:ts=4 sw=4:
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <winpr/sysinfo.h>

#include <freerdp/config.h>

#include <string.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <freerdp/log.h>

#include "prim_internal.h"
#include "prim_copy.h"
#include "../codec/color.h"

#include <freerdp/codec/color.h>

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#define TAG FREERDP_TAG("primitives.copy")

#include <emmintrin.h>
#include <immintrin.h>

static INLINE pstatus_t avx2_image_copy_no_overlap_convert(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier, SSIZE_T dstVOffset);

static INLINE pstatus_t avx2_image_copy_bgr24_bgrx32(BYTE* WINPR_RESTRICT pDstData, UINT32 nDstStep,
                                                     UINT32 nXDst, UINT32 nYDst, UINT32 nWidth,
                                                     UINT32 nHeight,
                                                     const BYTE* WINPR_RESTRICT pSrcData,
                                                     UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                                     SSIZE_T srcVMultiplier, SSIZE_T srcVOffset,
                                                     SSIZE_T dstVMultiplier, SSIZE_T dstVOffset)
{

	const SSIZE_T srcByte = 3;
	const SSIZE_T dstByte = 4;

	const __m256i mask = _mm256_set_epi32(0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
	                                      0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000);
	const __m256i smask = _mm256_set_epi32(0xff171615, 0xff141312, 0xff1110ff, 0xffffffff,
	                                       0xff0b0a09, 0xff080706, 0xff050403, 0xff020100);
	const __m256i shelpmask = _mm256_set_epi32(0xffffffff, 0xffffffff, 0xffffff1f, 0xff1e1d1c,
	                                           0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);
	const UINT32 rem = nWidth % 8;
	const SSIZE_T width = nWidth - rem;

	const size_t align = nSrcStep % 32;
	const BOOL fast = (align == 0) ? TRUE : (align >= 8 - MIN(8, (size_t)rem) ? TRUE : FALSE);
	for (SSIZE_T y = 0; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT srcLine =
		    &pSrcData[srcVMultiplier * (y + nYSrc) * nSrcStep + srcVOffset];
		BYTE* WINPR_RESTRICT dstLine =
		    &pDstData[dstVMultiplier * (y + nYDst) * nDstStep + dstVOffset];

		SSIZE_T x = 0;

		/* Ensure alignment requirements can be met */
		if (fast)
		{
			for (; x < width; x += 8)
			{
				const __m256i* src = (const __m256i*)&srcLine[(x + nXSrc) * srcByte];
				__m256i* dst = (__m256i*)&dstLine[(x + nXDst) * dstByte];
				const __m256i s0 = _mm256_loadu_si256(src);
				__m256i s1 = _mm256_shuffle_epi8(s0, smask);

				/* _mm256_shuffle_epi8 can not cross 128bit lanes.
				 * manually copy these bytes with extract/insert */
				const __m256i sx = _mm256_broadcastsi128_si256(_mm256_extractf128_si256(s0, 0));
				const __m256i sxx = _mm256_shuffle_epi8(sx, shelpmask);
				const __m256i bmask =
				    _mm256_set_epi32(0x00000000, 0x00000000, 0x000000FF, 0x00FFFFFF, 0x00000000,
				                     0x00000000, 0x00000000, 0x00000000);
				const __m256i merged = _mm256_blendv_epi8(s1, sxx, bmask);

				const __m256i s2 = _mm256_loadu_si256(dst);
				__m256i d0 = _mm256_blendv_epi8(merged, s2, mask);
				_mm256_storeu_si256(dst, d0);
			}
		}
		for (; x < nWidth; x++)
		{
			const BYTE* src = &srcLine[(x + nXSrc) * srcByte];
			BYTE* dst = &dstLine[(x + nXDst) * dstByte];
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE pstatus_t avx2_image_copy_bgrx32_bgrx32(BYTE* WINPR_RESTRICT pDstData,
                                                      UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                                      UINT32 nWidth, UINT32 nHeight,
                                                      const BYTE* WINPR_RESTRICT pSrcData,
                                                      UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                                      SSIZE_T srcVMultiplier, SSIZE_T srcVOffset,
                                                      SSIZE_T dstVMultiplier, SSIZE_T dstVOffset)
{

	const SSIZE_T srcByte = 4;
	const SSIZE_T dstByte = 4;

	const __m256i mask = _mm256_setr_epi8(
	    (char)0xFF, (char)0xFF, (char)0xFF, 0x00, (char)0xFF, (char)0xFF, (char)0xFF, 0x00,
	    (char)0xFF, (char)0xFF, (char)0xFF, 0x00, (char)0xFF, (char)0xFF, (char)0xFF, 0x00,
	    (char)0xFF, (char)0xFF, (char)0xFF, 0x00, (char)0xFF, (char)0xFF, (char)0xFF, 0x00,
	    (char)0xFF, (char)0xFF, (char)0xFF, 0x00, (char)0xFF, (char)0xFF, (char)0xFF, 0x00);
	const UINT32 rem = nWidth % 8;
	const SSIZE_T width = nWidth - rem;
	for (SSIZE_T y = 0; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT srcLine =
		    &pSrcData[srcVMultiplier * (y + nYSrc) * nSrcStep + srcVOffset];
		BYTE* WINPR_RESTRICT dstLine =
		    &pDstData[dstVMultiplier * (y + nYDst) * nDstStep + dstVOffset];

		SSIZE_T x = 0;
		for (; x < width; x += 8)
		{
			const __m256i* src = (const __m256i*)&srcLine[(x + nXSrc) * srcByte];
			__m256i* dst = (__m256i*)&dstLine[(x + nXDst) * dstByte];
			const __m256i s0 = _mm256_loadu_si256(src);
			const __m256i s1 = _mm256_loadu_si256(dst);
			__m256i d0 = _mm256_blendv_epi8(s1, s0, mask);
			_mm256_storeu_si256(dst, d0);
		}

		for (; x < nWidth; x++)
		{
			const BYTE* src = &srcLine[(x + nXSrc) * srcByte];
			BYTE* dst = &dstLine[(x + nXDst) * dstByte];
			*dst++ = *src++;
			*dst++ = *src++;
			*dst++ = *src++;
		}
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t avx2_image_copy_no_overlap_dst_alpha(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    UINT32 flags, SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier,
    SSIZE_T dstVOffset)
{
	WINPR_ASSERT(pDstData);
	WINPR_ASSERT(pSrcData);

	switch (SrcFormat)
	{
		case PIXEL_FORMAT_BGR24:
			switch (DstFormat)
			{
				case PIXEL_FORMAT_BGRX32:
				case PIXEL_FORMAT_BGRA32:
					return avx2_image_copy_bgr24_bgrx32(
					    pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, pSrcData, nSrcStep,
					    nXSrc, nYSrc, srcVMultiplier, srcVOffset, dstVMultiplier, dstVOffset);
				default:
					break;
			}
			break;
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_BGRA32:
			switch (DstFormat)
			{
				case PIXEL_FORMAT_BGRX32:
				case PIXEL_FORMAT_BGRA32:
					return avx2_image_copy_bgrx32_bgrx32(
					    pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, pSrcData, nSrcStep,
					    nXSrc, nYSrc, srcVMultiplier, srcVOffset, dstVMultiplier, dstVOffset);
				default:
					break;
			}
			break;
		case PIXEL_FORMAT_RGBX32:
		case PIXEL_FORMAT_RGBA32:
			switch (DstFormat)
			{
				case PIXEL_FORMAT_RGBX32:
				case PIXEL_FORMAT_RGBA32:
					return avx2_image_copy_bgrx32_bgrx32(
					    pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, pSrcData, nSrcStep,
					    nXSrc, nYSrc, srcVMultiplier, srcVOffset, dstVMultiplier, dstVOffset);
				default:
					break;
			}
			break;
		default:
			break;
	}

	primitives_t* gen = primitives_get_generic();
	return gen->copy_no_overlap(pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight,
	                            pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette, flags);
}

static pstatus_t avx2_image_copy_no_overlap(BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat,
                                            UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                            UINT32 nWidth, UINT32 nHeight,
                                            const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
                                            UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                            const gdiPalette* WINPR_RESTRICT palette, UINT32 flags)
{
	const BOOL vSrcVFlip = (flags & FREERDP_FLIP_VERTICAL) ? TRUE : FALSE;
	SSIZE_T srcVOffset = 0;
	SSIZE_T srcVMultiplier = 1;
	SSIZE_T dstVOffset = 0;
	SSIZE_T dstVMultiplier = 1;

	if ((nWidth == 0) || (nHeight == 0))
		return PRIMITIVES_SUCCESS;

	if ((nHeight > INT32_MAX) || (nWidth > INT32_MAX))
		return -1;

	if (!pDstData || !pSrcData)
		return -1;

	if (nDstStep == 0)
		nDstStep = nWidth * FreeRDPGetBytesPerPixel(DstFormat);

	if (nSrcStep == 0)
		nSrcStep = nWidth * FreeRDPGetBytesPerPixel(SrcFormat);

	if (vSrcVFlip)
	{
		srcVOffset = (nHeight - 1ll) * nSrcStep;
		srcVMultiplier = -1;
	}

	if (((flags & FREERDP_KEEP_DST_ALPHA) != 0) && FreeRDPColorHasAlpha(DstFormat))
		return avx2_image_copy_no_overlap_dst_alpha(pDstData, DstFormat, nDstStep, nXDst, nYDst,
		                                            nWidth, nHeight, pSrcData, SrcFormat, nSrcStep,
		                                            nXSrc, nYSrc, palette, flags, srcVMultiplier,
		                                            srcVOffset, dstVMultiplier, dstVOffset);
	else if (FreeRDPAreColorFormatsEqualNoAlpha(SrcFormat, DstFormat))
		return generic_image_copy_no_overlap_memcpy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
		                                            nWidth, nHeight, pSrcData, SrcFormat, nSrcStep,
		                                            nXSrc, nYSrc, palette, srcVMultiplier,
		                                            srcVOffset, dstVMultiplier, dstVOffset, flags);
	else
	{
		primitives_t* gen = primitives_get_generic();
		return gen->copy_no_overlap(pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight,
		                            pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, palette, flags);
	}
}
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_copy_avx2(primitives_t* prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	if (IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE))
	{
		WLog_VRB(PRIM_TAG, "AVX2 optimizations");
		prims->copy_no_overlap = avx2_image_copy_no_overlap;
	}
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or WITH_AVX2 or AVX2 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
