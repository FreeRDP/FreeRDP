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
#include "prim_avxsse.h"
#include "prim_copy.h"
#include "../codec/color.h"

#include <freerdp/codec/color.h>

#if defined(SSE_AVX_INTRINSICS_ENABLED)
#include <emmintrin.h>
#include <immintrin.h>

static INLINE pstatus_t sse_image_copy_bgr24_bgrx32(BYTE* WINPR_RESTRICT pDstData, UINT32 nDstStep,
                                                    UINT32 nXDst, UINT32 nYDst, UINT32 nWidth,
                                                    UINT32 nHeight,
                                                    const BYTE* WINPR_RESTRICT pSrcData,
                                                    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                                    int64_t srcVMultiplier, int64_t srcVOffset,
                                                    int64_t dstVMultiplier, int64_t dstVOffset)
{

	const int64_t srcByte = 3;
	const int64_t dstByte = 4;

	const __m128i mask = mm_set_epu32(0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000);
	const __m128i smask = mm_set_epu32(0xff0b0a09, 0xff080706, 0xff050403, 0xff020100);
	const UINT32 rem = nWidth % 4;

	const int64_t width = nWidth - rem;
	for (int64_t y = 0; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT srcLine =
		    &pSrcData[srcVMultiplier * (y + nYSrc) * nSrcStep + srcVOffset];
		BYTE* WINPR_RESTRICT dstLine =
		    &pDstData[dstVMultiplier * (y + nYDst) * nDstStep + dstVOffset];

		int64_t x = 0;
		/* Ensure alignment requirements can be met */
		for (; x < width; x += 4)
		{
			const __m128i* src = (const __m128i*)&srcLine[(x + nXSrc) * srcByte];
			__m128i* dst = (__m128i*)&dstLine[(x + nXDst) * dstByte];
			const __m128i s0 = LOAD_SI128(src);
			const __m128i s1 = _mm_shuffle_epi8(s0, smask);
			const __m128i s2 = LOAD_SI128(dst);

			__m128i d0 = _mm_blendv_epi8(s1, s2, mask);
			STORE_SI128(dst, d0);
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

static INLINE pstatus_t sse_image_copy_bgrx32_bgrx32(BYTE* WINPR_RESTRICT pDstData, UINT32 nDstStep,
                                                     UINT32 nXDst, UINT32 nYDst, UINT32 nWidth,
                                                     UINT32 nHeight,
                                                     const BYTE* WINPR_RESTRICT pSrcData,
                                                     UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                                     int64_t srcVMultiplier, int64_t srcVOffset,
                                                     int64_t dstVMultiplier, int64_t dstVOffset)
{

	const int64_t srcByte = 4;
	const int64_t dstByte = 4;

	const __m128i mask = _mm_setr_epi8((char)0xFF, (char)0xFF, (char)0xFF, 0x00, (char)0xFF,
	                                   (char)0xFF, (char)0xFF, 0x00, (char)0xFF, (char)0xFF,
	                                   (char)0xFF, 0x00, (char)0xFF, (char)0xFF, (char)0xFF, 0x00);
	const UINT32 rem = nWidth % 4;
	const int64_t width = nWidth - rem;
	for (int64_t y = 0; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT srcLine =
		    &pSrcData[srcVMultiplier * (y + nYSrc) * nSrcStep + srcVOffset];
		BYTE* WINPR_RESTRICT dstLine =
		    &pDstData[dstVMultiplier * (y + nYDst) * nDstStep + dstVOffset];

		int64_t x = 0;
		for (; x < width; x += 4)
		{
			const __m128i* src = (const __m128i*)&srcLine[(x + nXSrc) * srcByte];
			__m128i* dst = (__m128i*)&dstLine[(x + nXDst) * dstByte];
			const __m128i s0 = LOAD_SI128(src);
			const __m128i s1 = LOAD_SI128(dst);
			__m128i d0 = _mm_blendv_epi8(s1, s0, mask);
			STORE_SI128(dst, d0);
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

static pstatus_t sse_image_copy_no_overlap_dst_alpha(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    UINT32 flags, int64_t srcVMultiplier, int64_t srcVOffset, int64_t dstVMultiplier,
    int64_t dstVOffset)
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
					return sse_image_copy_bgr24_bgrx32(
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
					return sse_image_copy_bgrx32_bgrx32(
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
					return sse_image_copy_bgrx32_bgrx32(
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

static pstatus_t sse_image_copy_no_overlap(BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat,
                                           UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                           UINT32 nWidth, UINT32 nHeight,
                                           const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
                                           UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                           const gdiPalette* WINPR_RESTRICT palette, UINT32 flags)
{
	const BOOL vSrcVFlip = (flags & FREERDP_FLIP_VERTICAL) ? TRUE : FALSE;
	int64_t srcVOffset = 0;
	int64_t srcVMultiplier = 1;
	int64_t dstVOffset = 0;
	int64_t dstVMultiplier = 1;

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
		return sse_image_copy_no_overlap_dst_alpha(pDstData, DstFormat, nDstStep, nXDst, nYDst,
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
void primitives_init_copy_sse41_int(primitives_t* WINPR_RESTRICT prims)
{
#if defined(SSE_AVX_INTRINSICS_ENABLED)
	WLog_VRB(PRIM_TAG, "SSE4.1 optimizations");
	prims->copy_no_overlap = sse_image_copy_no_overlap;
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or SSE4.1 intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
