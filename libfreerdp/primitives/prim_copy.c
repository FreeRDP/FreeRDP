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

#include <freerdp/config.h>

#include <string.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <freerdp/log.h>

#include "prim_internal.h"
#include "prim_copy.h"
#include "../codec/color.h"

#include <freerdp/codec/color.h>

static primitives_t* generic = NULL;

/* ------------------------------------------------------------------------- */
/*static inline BOOL memory_regions_overlap_1d(*/
static BOOL memory_regions_overlap_1d(const BYTE* p1, const BYTE* p2, size_t bytes)
{
	const ULONG_PTR p1m = (const ULONG_PTR)p1;
	const ULONG_PTR p2m = (const ULONG_PTR)p2;

	if (p1m <= p2m)
	{
		if (p1m + bytes > p2m)
			return TRUE;
	}
	else
	{
		if (p2m + bytes > p1m)
			return TRUE;
	}

	/* else */
	return FALSE;
}

/* ------------------------------------------------------------------------- */
/*static inline BOOL memory_regions_overlap_2d( */
static BOOL memory_regions_overlap_2d(const BYTE* p1, int p1Step, int p1Size, const BYTE* p2,
                                      int p2Step, int p2Size, int width, int height)
{
	ULONG_PTR p1m = (ULONG_PTR)p1;
	ULONG_PTR p2m = (ULONG_PTR)p2;

	if (p1m <= p2m)
	{
		ULONG_PTR p1mEnd = p1m + 1ull * (height - 1) * p1Step + 1ull * width * p1Size;

		if (p1mEnd > p2m)
			return TRUE;
	}
	else
	{
		ULONG_PTR p2mEnd = p2m + 1ull * (height - 1) * p2Step + 1ull * width * p2Size;

		if (p2mEnd > p1m)
			return TRUE;
	}

	/* else */
	return FALSE;
}

/* ------------------------------------------------------------------------- */
static pstatus_t general_copy_8u(const BYTE* pSrc, BYTE* pDst, INT32 len)
{
	if (memory_regions_overlap_1d(pSrc, pDst, (size_t)len))
	{
		memmove((void*)pDst, (const void*)pSrc, (size_t)len);
	}
	else
	{
		memcpy((void*)pDst, (const void*)pSrc, (size_t)len);
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Copy a block of pixels from one buffer to another.
 * The addresses are assumed to have been already offset to the upper-left
 * corners of the source and destination region of interest.
 */
static pstatus_t general_copy_8u_AC4r(const BYTE* pSrc, INT32 srcStep, BYTE* pDst, INT32 dstStep,
                                      INT32 width, INT32 height)
{
	const BYTE* src = pSrc;
	BYTE* dst = pDst;
	int rowbytes = width * sizeof(UINT32);

	if ((width == 0) || (height == 0))
		return PRIMITIVES_SUCCESS;

	if (memory_regions_overlap_2d(pSrc, srcStep, sizeof(UINT32), pDst, dstStep, sizeof(UINT32),
	                              width, height))
	{
		do
		{
			generic->copy(src, dst, rowbytes);
			src += srcStep;
			dst += dstStep;
		} while (--height);
	}
	else
	{
		/* TODO: do it in one operation when the rowdata is adjacent. */
		do
		{
			/* If we find a replacement for memcpy that is consistently
			 * faster, this could be replaced with that.
			 */
			memcpy(dst, src, rowbytes);
			src += srcStep;
			dst += dstStep;
		} while (--height);
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE pstatus_t generic_image_copy_bgr24_bgrx32(BYTE* WINPR_RESTRICT pDstData,
                                                        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                                        UINT32 nWidth, UINT32 nHeight,
                                                        const BYTE* WINPR_RESTRICT pSrcData,
                                                        UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                                        SSIZE_T srcVMultiplier, SSIZE_T srcVOffset,
                                                        SSIZE_T dstVMultiplier, SSIZE_T dstVOffset)
{

	const SSIZE_T srcByte = 3;
	const SSIZE_T dstByte = 4;

	const UINT32 width = nWidth - nWidth % 8;

	for (SSIZE_T y = 0; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT srcLine =
		    &pSrcData[srcVMultiplier * (y + nYSrc) * nSrcStep + srcVOffset];
		BYTE* WINPR_RESTRICT dstLine =
		    &pDstData[dstVMultiplier * (y + nYDst) * nDstStep + dstVOffset];

		SSIZE_T x = 0;
		WINPR_PRAGMA_UNROLL_LOOP
		for (; x < width; x++)
		{
			dstLine[(x + nXDst) * dstByte + 0] = srcLine[(x + nXSrc) * srcByte + 0];
			dstLine[(x + nXDst) * dstByte + 1] = srcLine[(x + nXSrc) * srcByte + 1];
			dstLine[(x + nXDst) * dstByte + 2] = srcLine[(x + nXSrc) * srcByte + 2];
		}

		for (; x < nWidth; x++)
		{
			dstLine[(x + nXDst) * dstByte + 0] = srcLine[(x + nXSrc) * srcByte + 0];
			dstLine[(x + nXDst) * dstByte + 1] = srcLine[(x + nXSrc) * srcByte + 1];
			dstLine[(x + nXDst) * dstByte + 2] = srcLine[(x + nXSrc) * srcByte + 2];
		}
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE pstatus_t generic_image_copy_bgrx32_bgrx32(
    BYTE* WINPR_RESTRICT pDstData, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst, UINT32 nWidth,
    UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, UINT32 nSrcStep, UINT32 nXSrc,
    UINT32 nYSrc, SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier,
    SSIZE_T dstVOffset)
{

	const SSIZE_T srcByte = 4;
	const SSIZE_T dstByte = 4;

	const UINT32 width = nWidth - nWidth % 8;

	for (SSIZE_T y = 0; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT srcLine =
		    &pSrcData[srcVMultiplier * (y + nYSrc) * nSrcStep + srcVOffset];
		BYTE* WINPR_RESTRICT dstLine =
		    &pDstData[dstVMultiplier * (y + nYDst) * nDstStep + dstVOffset];

		SSIZE_T x = 0;
		WINPR_PRAGMA_UNROLL_LOOP
		for (; x < width; x++)
		{
			dstLine[(x + nXDst) * dstByte + 0] = srcLine[(x + nXSrc) * srcByte + 0];
			dstLine[(x + nXDst) * dstByte + 1] = srcLine[(x + nXSrc) * srcByte + 1];
			dstLine[(x + nXDst) * dstByte + 2] = srcLine[(x + nXSrc) * srcByte + 2];
		}
		for (; x < nWidth; x++)
		{
			dstLine[(x + nXDst) * dstByte + 0] = srcLine[(x + nXSrc) * srcByte + 0];
			dstLine[(x + nXDst) * dstByte + 1] = srcLine[(x + nXSrc) * srcByte + 1];
			dstLine[(x + nXDst) * dstByte + 2] = srcLine[(x + nXSrc) * srcByte + 2];
		}
	}

	return PRIMITIVES_SUCCESS;
}

pstatus_t generic_image_copy_no_overlap_convert(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier, SSIZE_T dstVOffset)
{
	const SSIZE_T srcByte = FreeRDPGetBytesPerPixel(SrcFormat);
	const SSIZE_T dstByte = FreeRDPGetBytesPerPixel(DstFormat);

	const UINT32 width = nWidth - nWidth % 8;
	for (SSIZE_T y = 0; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT srcLine =
		    &pSrcData[srcVMultiplier * (y + nYSrc) * nSrcStep + srcVOffset];
		BYTE* WINPR_RESTRICT dstLine =
		    &pDstData[dstVMultiplier * (y + nYDst) * nDstStep + dstVOffset];

		SSIZE_T x = 0;
		WINPR_PRAGMA_UNROLL_LOOP
		for (; x < width; x++)
		{
			const UINT32 color = FreeRDPReadColor_int(&srcLine[(x + nXSrc) * srcByte], SrcFormat);
			const UINT32 dstColor = FreeRDPConvertColor(color, SrcFormat, DstFormat, palette);
			FreeRDPWriteColor_int(&dstLine[(x + nXDst) * dstByte], DstFormat, dstColor);
		}
		for (; x < nWidth; x++)
		{
			const UINT32 color = FreeRDPReadColor_int(&srcLine[(x + nXSrc) * srcByte], SrcFormat);
			const UINT32 dstColor = FreeRDPConvertColor(color, SrcFormat, DstFormat, palette);
			FreeRDPWriteColor_int(&dstLine[(x + nXDst) * dstByte], DstFormat, dstColor);
		}
	}
	return PRIMITIVES_SUCCESS;
}

pstatus_t generic_image_copy_no_overlap_memcpy(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier, SSIZE_T dstVOffset,
    UINT32 flags)
{
	const SSIZE_T dstByte = FreeRDPGetBytesPerPixel(DstFormat);
	const SSIZE_T srcByte = FreeRDPGetBytesPerPixel(SrcFormat);
	const SSIZE_T copyDstWidth = nWidth * dstByte;
	const SSIZE_T xSrcOffset = nXSrc * srcByte;
	const SSIZE_T xDstOffset = nXDst * dstByte;

	for (SSIZE_T y = 0; y < nHeight; y++)
	{
		const BYTE* WINPR_RESTRICT srcLine =
		    &pSrcData[srcVMultiplier * (y + nYSrc) * nSrcStep + srcVOffset];
		BYTE* WINPR_RESTRICT dstLine =
		    &pDstData[dstVMultiplier * (y + nYDst) * nDstStep + dstVOffset];
		memcpy(&dstLine[xDstOffset], &srcLine[xSrcOffset], copyDstWidth);
	}

	return PRIMITIVES_SUCCESS;
}

static INLINE pstatus_t generic_image_copy_no_overlap_dst_alpha(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier, SSIZE_T dstVOffset)
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
					return generic_image_copy_bgr24_bgrx32(
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
					return generic_image_copy_bgrx32_bgrx32(
					    pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, pSrcData, nSrcStep,
					    nXSrc, nYSrc, srcVMultiplier, srcVOffset, dstVMultiplier, dstVOffset);
				default:
					break;
			}
			break;
		default:
			break;
	}

	return generic_image_copy_no_overlap_convert(
	    pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight, pSrcData, SrcFormat, nSrcStep,
	    nXSrc, nYSrc, palette, srcVMultiplier, srcVOffset, dstVMultiplier, dstVOffset);
}

static INLINE pstatus_t generic_image_copy_no_overlap_no_alpha(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier, SSIZE_T dstVOffset,
    UINT32 flags)
{
	if (FreeRDPAreColorFormatsEqualNoAlpha(SrcFormat, DstFormat))
		return generic_image_copy_no_overlap_memcpy(pDstData, DstFormat, nDstStep, nXDst, nYDst,
		                                            nWidth, nHeight, pSrcData, SrcFormat, nSrcStep,
		                                            nXSrc, nYSrc, palette, srcVMultiplier,
		                                            srcVOffset, dstVMultiplier, dstVOffset, flags);
	else
		return generic_image_copy_no_overlap_convert(pDstData, DstFormat, nDstStep, nXDst, nYDst,
		                                             nWidth, nHeight, pSrcData, SrcFormat, nSrcStep,
		                                             nXSrc, nYSrc, palette, srcVMultiplier,
		                                             srcVOffset, dstVMultiplier, dstVOffset);
}

static pstatus_t generic_image_copy_no_overlap(BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat,
                                               UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                               UINT32 nWidth, UINT32 nHeight,
                                               const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
                                               UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                               const gdiPalette* WINPR_RESTRICT palette,
                                               UINT32 flags)
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
		return generic_image_copy_no_overlap_dst_alpha(
		    pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight, pSrcData, SrcFormat,
		    nSrcStep, nXSrc, nYSrc, palette, srcVMultiplier, srcVOffset, dstVMultiplier,
		    dstVOffset);
	else
		return generic_image_copy_no_overlap_no_alpha(
		    pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight, pSrcData, SrcFormat,
		    nSrcStep, nXSrc, nYSrc, palette, srcVMultiplier, srcVOffset, dstVMultiplier, dstVOffset,
		    flags);

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_copy(primitives_t* WINPR_RESTRICT prims)
{
	/* Start with the default. */
	prims->copy_8u = general_copy_8u;
	prims->copy_8u_AC4r = general_copy_8u_AC4r;
	prims->copy = WINPR_FUNC_PTR_CAST(prims->copy_8u, __copy_t);
	prims->copy_no_overlap = generic_image_copy_no_overlap;
}

void primitives_init_copy_opt(primitives_t* prims)
{
	primitives_init_copy_sse41(prims);
#if defined(WITH_AVX2)
	primitives_init_copy_avx2(prims);
#endif
}
