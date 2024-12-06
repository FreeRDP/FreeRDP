/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP6 Planar Codec
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/wtypes.h>
#include <winpr/assert.h>
#include <winpr/print.h>

#include <freerdp/primitives.h>
#include <freerdp/log.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/planar.h>

#define TAG FREERDP_TAG("codec")

#define PLANAR_ALIGN(val, align) \
	((val) % (align) == 0) ? (val) : ((val) + (align) - (val) % (align))

typedef struct
{
	/**
	 * controlByte:
	 * [0-3]: nRunLength
	 * [4-7]: cRawBytes
	 */
	BYTE controlByte;
	BYTE* rawValues;
} RDP6_RLE_SEGMENT;

typedef struct
{
	UINT32 cSegments;
	RDP6_RLE_SEGMENT* segments;
} RDP6_RLE_SEGMENTS;

typedef struct
{
	/**
	 * formatHeader:
	 * [0-2]: Color Loss Level (CLL)
	 *  [3] : Chroma Subsampling (CS)
	 *  [4] : Run Length Encoding (RLE)
	 *  [5] : No Alpha (NA)
	 * [6-7]: Reserved
	 */
	BYTE formatHeader;
} RDP6_BITMAP_STREAM;

struct S_BITMAP_PLANAR_CONTEXT
{
	UINT32 maxWidth;
	UINT32 maxHeight;
	UINT32 maxPlaneSize;

	BOOL AllowSkipAlpha;
	BOOL AllowRunLengthEncoding;
	BOOL AllowColorSubsampling;
	BOOL AllowDynamicColorFidelity;

	UINT32 ColorLossLevel;

	BYTE* planes[4];
	BYTE* planesBuffer;

	BYTE* deltaPlanes[4];
	BYTE* deltaPlanesBuffer;

	BYTE* rlePlanes[4];
	BYTE* rlePlanesBuffer;

	BYTE* pTempData;
	UINT32 nTempStep;

	BOOL bgr;
	BOOL topdown;
};

static INLINE UINT32 planar_invert_format(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT planar, BOOL alpha,
                                          UINT32 DstFormat)
{

	if (planar->bgr && alpha)
	{
		switch (DstFormat)
		{
			case PIXEL_FORMAT_ARGB32:
				DstFormat = PIXEL_FORMAT_ABGR32;
				break;
			case PIXEL_FORMAT_XRGB32:
				DstFormat = PIXEL_FORMAT_XBGR32;
				break;
			case PIXEL_FORMAT_ABGR32:
				DstFormat = PIXEL_FORMAT_ARGB32;
				break;
			case PIXEL_FORMAT_XBGR32:
				DstFormat = PIXEL_FORMAT_XRGB32;
				break;
			case PIXEL_FORMAT_BGRA32:
				DstFormat = PIXEL_FORMAT_RGBA32;
				break;
			case PIXEL_FORMAT_BGRX32:
				DstFormat = PIXEL_FORMAT_RGBX32;
				break;
			case PIXEL_FORMAT_RGBA32:
				DstFormat = PIXEL_FORMAT_BGRA32;
				break;
			case PIXEL_FORMAT_RGBX32:
				DstFormat = PIXEL_FORMAT_BGRX32;
				break;
			case PIXEL_FORMAT_RGB24:
				DstFormat = PIXEL_FORMAT_BGR24;
				break;
			case PIXEL_FORMAT_BGR24:
				DstFormat = PIXEL_FORMAT_RGB24;
				break;
			case PIXEL_FORMAT_RGB16:
				DstFormat = PIXEL_FORMAT_BGR16;
				break;
			case PIXEL_FORMAT_BGR16:
				DstFormat = PIXEL_FORMAT_RGB16;
				break;
			case PIXEL_FORMAT_ARGB15:
				DstFormat = PIXEL_FORMAT_ABGR15;
				break;
			case PIXEL_FORMAT_RGB15:
				DstFormat = PIXEL_FORMAT_BGR15;
				break;
			case PIXEL_FORMAT_ABGR15:
				DstFormat = PIXEL_FORMAT_ARGB15;
				break;
			case PIXEL_FORMAT_BGR15:
				DstFormat = PIXEL_FORMAT_RGB15;
				break;
			default:
				break;
		}
	}
	return DstFormat;
}

static INLINE BOOL freerdp_bitmap_planar_compress_plane_rle(const BYTE* WINPR_RESTRICT inPlane,
                                                            UINT32 width, UINT32 height,
                                                            BYTE* WINPR_RESTRICT outPlane,
                                                            UINT32* WINPR_RESTRICT dstSize);
static INLINE BYTE* freerdp_bitmap_planar_delta_encode_plane(const BYTE* WINPR_RESTRICT inPlane,
                                                             UINT32 width, UINT32 height,
                                                             BYTE* WINPR_RESTRICT outPlane);

static INLINE INT32 planar_skip_plane_rle(const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
                                          UINT32 nWidth, UINT32 nHeight)
{
	UINT32 used = 0;
	BYTE controlByte = 0;

	WINPR_ASSERT(pSrcData);

	for (UINT32 y = 0; y < nHeight; y++)
	{
		for (UINT32 x = 0; x < nWidth;)
		{
			int cRawBytes = 0;
			int nRunLength = 0;

			if (used >= SrcSize)
			{
				WLog_ERR(TAG, "planar plane used %" PRIu32 " exceeds SrcSize %" PRIu32, used,
				         SrcSize);
				return -1;
			}

			controlByte = pSrcData[used++];
			nRunLength = PLANAR_CONTROL_BYTE_RUN_LENGTH(controlByte);
			cRawBytes = PLANAR_CONTROL_BYTE_RAW_BYTES(controlByte);

			if (nRunLength == 1)
			{
				nRunLength = cRawBytes + 16;
				cRawBytes = 0;
			}
			else if (nRunLength == 2)
			{
				nRunLength = cRawBytes + 32;
				cRawBytes = 0;
			}

			used += cRawBytes;
			x += cRawBytes;
			x += nRunLength;

			if (x > nWidth)
			{
				WLog_ERR(TAG, "planar plane x %" PRIu32 " exceeds width %" PRIu32, x, nWidth);
				return -1;
			}

			if (used > SrcSize)
			{
				WLog_ERR(TAG, "planar plane used %" PRIu32 " exceeds SrcSize %" PRIu32, used,
				         INT32_MAX);
				return -1;
			}
		}
	}

	if (used > INT32_MAX)
	{
		WLog_ERR(TAG, "planar plane used %" PRIu32 " exceeds SrcSize %" PRIu32, used, SrcSize);
		return -1;
	}
	return (INT32)used;
}

static INLINE INT32 planar_decompress_plane_rle_only(const BYTE* WINPR_RESTRICT pSrcData,
                                                     UINT32 SrcSize, BYTE* WINPR_RESTRICT pDstData,
                                                     UINT32 nWidth, UINT32 nHeight)
{
	UINT32 pixel = 0;
	UINT32 cRawBytes = 0;
	UINT32 nRunLength = 0;
	INT32 deltaValue = 0;
	BYTE controlByte = 0;
	BYTE* currentScanline = NULL;
	BYTE* previousScanline = NULL;
	const BYTE* srcp = pSrcData;

	WINPR_ASSERT(nHeight <= INT32_MAX);
	WINPR_ASSERT(nWidth <= INT32_MAX);

	previousScanline = NULL;

	for (INT32 y = 0; y < (INT32)nHeight; y++)
	{
		BYTE* dstp = &pDstData[(1ULL * (y) * (INT32)nWidth)];
		pixel = 0;
		currentScanline = dstp;

		for (INT32 x = 0; x < (INT32)nWidth;)
		{
			controlByte = *srcp;
			srcp++;

			if ((srcp - pSrcData) > SrcSize * 1ll)
			{
				WLog_ERR(TAG, "error reading input buffer");
				return -1;
			}

			nRunLength = PLANAR_CONTROL_BYTE_RUN_LENGTH(controlByte);
			cRawBytes = PLANAR_CONTROL_BYTE_RAW_BYTES(controlByte);

			if (nRunLength == 1)
			{
				nRunLength = cRawBytes + 16;
				cRawBytes = 0;
			}
			else if (nRunLength == 2)
			{
				nRunLength = cRawBytes + 32;
				cRawBytes = 0;
			}

			if (((dstp + (cRawBytes + nRunLength)) - currentScanline) > nWidth * 1ll)
			{
				WLog_ERR(TAG, "too many pixels in scanline");
				return -1;
			}

			if (!previousScanline)
			{
				/* first scanline, absolute values */
				while (cRawBytes > 0)
				{
					pixel = *srcp;
					srcp++;
					*dstp = pixel;
					dstp++;
					x++;
					cRawBytes--;
				}

				while (nRunLength > 0)
				{
					*dstp = pixel;
					dstp++;
					x++;
					nRunLength--;
				}
			}
			else
			{
				/* delta values relative to previous scanline */
				while (cRawBytes > 0)
				{
					deltaValue = *srcp;
					srcp++;

					if (deltaValue & 1)
					{
						deltaValue = deltaValue >> 1;
						deltaValue = deltaValue + 1;
						pixel = -deltaValue;
					}
					else
					{
						deltaValue = deltaValue >> 1;
						pixel = deltaValue;
					}

					deltaValue = previousScanline[x] + pixel;
					*dstp = deltaValue;
					dstp++;
					x++;
					cRawBytes--;
				}

				while (nRunLength > 0)
				{
					deltaValue = previousScanline[x] + pixel;
					*dstp = deltaValue;
					dstp++;
					x++;
					nRunLength--;
				}
			}
		}

		previousScanline = currentScanline;
	}

	return (INT32)(srcp - pSrcData);
}

static INLINE INT32 planar_decompress_plane_rle(const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
                                                BYTE* WINPR_RESTRICT pDstData, UINT32 nDstStep,
                                                UINT32 nXDst, UINT32 nYDst, UINT32 nWidth,
                                                UINT32 nHeight, UINT32 nChannel, BOOL vFlip)
{
	UINT32 pixel = 0;
	UINT32 cRawBytes = 0;
	UINT32 nRunLength = 0;
	INT32 deltaValue = 0;
	INT32 beg = 0;
	INT32 end = 0;
	INT32 inc = 0;
	BYTE controlByte = 0;
	BYTE* currentScanline = NULL;
	BYTE* previousScanline = NULL;
	const BYTE* srcp = pSrcData;

	WINPR_ASSERT(nHeight <= INT32_MAX);
	WINPR_ASSERT(nWidth <= INT32_MAX);
	WINPR_ASSERT(nDstStep <= INT32_MAX);

	previousScanline = NULL;

	if (vFlip)
	{
		beg = (INT32)nHeight - 1;
		end = -1;
		inc = -1;
	}
	else
	{
		beg = 0;
		end = (INT32)nHeight;
		inc = 1;
	}

	for (INT32 y = beg; y != end; y += inc)
	{
		BYTE* dstp = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4) + nChannel];
		pixel = 0;
		currentScanline = dstp;

		for (INT32 x = 0; x < (INT32)nWidth;)
		{
			controlByte = *srcp;
			srcp++;

			if ((srcp - pSrcData) > SrcSize * 1ll)
			{
				WLog_ERR(TAG, "error reading input buffer");
				return -1;
			}

			nRunLength = PLANAR_CONTROL_BYTE_RUN_LENGTH(controlByte);
			cRawBytes = PLANAR_CONTROL_BYTE_RAW_BYTES(controlByte);

			if (nRunLength == 1)
			{
				nRunLength = cRawBytes + 16;
				cRawBytes = 0;
			}
			else if (nRunLength == 2)
			{
				nRunLength = cRawBytes + 32;
				cRawBytes = 0;
			}

			if (((dstp + (cRawBytes + nRunLength)) - currentScanline) > nWidth * 4ll)
			{
				WLog_ERR(TAG, "too many pixels in scanline");
				return -1;
			}

			if (!previousScanline)
			{
				/* first scanline, absolute values */
				while (cRawBytes > 0)
				{
					pixel = *srcp;
					srcp++;
					*dstp = pixel;
					dstp += 4;
					x++;
					cRawBytes--;
				}

				while (nRunLength > 0)
				{
					*dstp = pixel;
					dstp += 4;
					x++;
					nRunLength--;
				}
			}
			else
			{
				/* delta values relative to previous scanline */
				while (cRawBytes > 0)
				{
					deltaValue = *srcp;
					srcp++;

					if (deltaValue & 1)
					{
						deltaValue = deltaValue >> 1;
						deltaValue = deltaValue + 1;
						pixel = -deltaValue;
					}
					else
					{
						deltaValue = deltaValue >> 1;
						pixel = deltaValue;
					}

					deltaValue = previousScanline[4LL * x] + pixel;
					*dstp = deltaValue;
					dstp += 4;
					x++;
					cRawBytes--;
				}

				while (nRunLength > 0)
				{
					deltaValue = previousScanline[4LL * x] + pixel;
					*dstp = deltaValue;
					dstp += 4;
					x++;
					nRunLength--;
				}
			}
		}

		previousScanline = currentScanline;
	}

	return (INT32)(srcp - pSrcData);
}

static INLINE INT32 planar_set_plane(BYTE bValue, BYTE* pDstData, UINT32 nDstStep, UINT32 nXDst,
                                     UINT32 nYDst, UINT32 nWidth, UINT32 nHeight, UINT32 nChannel,
                                     BOOL vFlip)
{
	INT32 beg = 0;
	INT32 end = 0;
	INT32 inc = 0;

	WINPR_ASSERT(nHeight <= INT32_MAX);
	WINPR_ASSERT(nWidth <= INT32_MAX);
	WINPR_ASSERT(nDstStep <= INT32_MAX);

	if (vFlip)
	{
		beg = (INT32)nHeight - 1;
		end = -1;
		inc = -1;
	}
	else
	{
		beg = 0;
		end = (INT32)nHeight;
		inc = 1;
	}

	for (INT32 y = beg; y != end; y += inc)
	{
		BYTE* dstp = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4) + nChannel];

		for (INT32 x = 0; x < (INT32)nWidth; ++x)
		{
			*dstp = bValue;
			dstp += 4;
		}
	}

	return 0;
}

static INLINE BOOL writeLine(BYTE** WINPR_RESTRICT ppRgba, UINT32 DstFormat, UINT32 width,
                             const BYTE** WINPR_RESTRICT ppR, const BYTE** WINPR_RESTRICT ppG,
                             const BYTE** WINPR_RESTRICT ppB, const BYTE** WINPR_RESTRICT ppA)
{
	WINPR_ASSERT(ppRgba);
	WINPR_ASSERT(ppR);
	WINPR_ASSERT(ppG);
	WINPR_ASSERT(ppB);

	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
			for (UINT32 x = 0; x < width; x++)
			{
				*(*ppRgba)++ = *(*ppB)++;
				*(*ppRgba)++ = *(*ppG)++;
				*(*ppRgba)++ = *(*ppR)++;
				*(*ppRgba)++ = *(*ppA)++;
			}

			return TRUE;

		case PIXEL_FORMAT_BGRX32:
			for (UINT32 x = 0; x < width; x++)
			{
				*(*ppRgba)++ = *(*ppB)++;
				*(*ppRgba)++ = *(*ppG)++;
				*(*ppRgba)++ = *(*ppR)++;
				*(*ppRgba)++ = 0xFF;
			}

			return TRUE;

		default:
			if (ppA)
			{
				for (UINT32 x = 0; x < width; x++)
				{
					BYTE alpha = *(*ppA)++;
					UINT32 color =
					    FreeRDPGetColor(DstFormat, *(*ppR)++, *(*ppG)++, *(*ppB)++, alpha);
					FreeRDPWriteColor(*ppRgba, DstFormat, color);
					*ppRgba += FreeRDPGetBytesPerPixel(DstFormat);
				}
			}
			else
			{
				const BYTE alpha = 0xFF;

				for (UINT32 x = 0; x < width; x++)
				{
					UINT32 color =
					    FreeRDPGetColor(DstFormat, *(*ppR)++, *(*ppG)++, *(*ppB)++, alpha);
					FreeRDPWriteColor(*ppRgba, DstFormat, color);
					*ppRgba += FreeRDPGetBytesPerPixel(DstFormat);
				}
			}

			return TRUE;
	}
}

static INLINE BOOL planar_decompress_planes_raw(const BYTE* WINPR_RESTRICT pSrcData[4],
                                                BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                                UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                                UINT32 nWidth, UINT32 nHeight, BOOL vFlip,
                                                UINT32 totalHeight)
{
	INT32 beg = 0;
	INT32 end = 0;
	INT32 inc = 0;
	const BYTE* pR = pSrcData[0];
	const BYTE* pG = pSrcData[1];
	const BYTE* pB = pSrcData[2];
	const BYTE* pA = pSrcData[3];
	const UINT32 bpp = FreeRDPGetBytesPerPixel(DstFormat);

	if (vFlip)
	{
		beg = nHeight - 1;
		end = -1;
		inc = -1;
	}
	else
	{
		beg = 0;
		end = nHeight;
		inc = 1;
	}

	if (nYDst + nHeight > totalHeight)
	{
		WLog_ERR(TAG,
		         "planar plane destination Y %" PRIu32 " + height %" PRIu32
		         " exceeds totalHeight %" PRIu32,
		         nYDst, nHeight, totalHeight);
		return FALSE;
	}

	if ((nXDst + nWidth) * bpp > nDstStep)
	{
		WLog_ERR(TAG,
		         "planar plane destination (X %" PRIu32 " + width %" PRIu32 ") * bpp %" PRIu32
		         " exceeds stride %" PRIu32,
		         nXDst, nWidth, bpp, nDstStep);
		return FALSE;
	}

	for (INT32 y = beg; y != end; y += inc)
	{
		BYTE* pRGB = NULL;

		if (y > (INT64)nHeight)
		{
			WLog_ERR(TAG, "planar plane destination Y %" PRId32 " exceeds height %" PRIu32, y,
			         nHeight);
			return FALSE;
		}

		pRGB = &pDstData[((nYDst + y) * nDstStep) + (nXDst * bpp)];

		if (!writeLine(&pRGB, DstFormat, nWidth, &pR, &pG, &pB, &pA))
			return FALSE;
	}

	return TRUE;
}

static BOOL planar_subsample_expand(const BYTE* WINPR_RESTRICT plane, size_t planeLength,
                                    UINT32 nWidth, UINT32 nHeight, UINT32 nPlaneWidth,
                                    UINT32 nPlaneHeight, BYTE* WINPR_RESTRICT deltaPlane)
{
	size_t pos = 0;
	WINPR_UNUSED(planeLength);

	WINPR_ASSERT(plane);
	WINPR_ASSERT(deltaPlane);

	if (nWidth > nPlaneWidth * 2)
	{
		WLog_ERR(TAG, "planar subsample width %" PRIu32 " > PlaneWidth %" PRIu32 " * 2", nWidth,
		         nPlaneWidth);
		return FALSE;
	}

	if (nHeight > nPlaneHeight * 2)
	{
		WLog_ERR(TAG, "planar subsample height %" PRIu32 " > PlaneHeight %" PRIu32 " * 2", nHeight,
		         nPlaneHeight);
		return FALSE;
	}

	for (size_t y = 0; y < nHeight; y++)
	{
		const BYTE* src = plane + y / 2 * nPlaneWidth;

		for (UINT32 x = 0; x < nWidth; x++)
		{
			deltaPlane[pos++] = src[x / 2];
		}
	}

	return TRUE;
}

BOOL planar_decompress(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT planar,
                       const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize, UINT32 nSrcWidth,
                       UINT32 nSrcHeight, BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                       UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst, UINT32 nDstWidth,
                       UINT32 nDstHeight, BOOL vFlip)
{
	BOOL cs = 0;
	BOOL rle = 0;
	UINT32 cll = 0;
	BOOL alpha = 0;
	BOOL useAlpha = FALSE;
	INT32 status = 0;
	const BYTE* srcp = NULL;
	UINT32 subSize = 0;
	UINT32 subWidth = 0;
	UINT32 subHeight = 0;
	UINT32 planeSize = 0;
	INT32 rleSizes[4] = { 0, 0, 0, 0 };
	UINT32 rawSizes[4];
	UINT32 rawWidths[4];
	UINT32 rawHeights[4];
	BYTE FormatHeader = 0;
	const BYTE* planes[4] = { 0 };
	const UINT32 w = MIN(nSrcWidth, nDstWidth);
	const UINT32 h = MIN(nSrcHeight, nDstHeight);
	const primitives_t* prims = primitives_get();

	WINPR_ASSERT(planar);
	WINPR_ASSERT(prims);

	if (nDstStep <= 0)
		nDstStep = nDstWidth * FreeRDPGetBytesPerPixel(DstFormat);

	srcp = pSrcData;

	if (!pSrcData)
	{
		WLog_ERR(TAG, "Invalid argument pSrcData=NULL");
		return FALSE;
	}

	if (!pDstData)
	{
		WLog_ERR(TAG, "Invalid argument pDstData=NULL");
		return FALSE;
	}

	FormatHeader = *srcp++;
	cll = (FormatHeader & PLANAR_FORMAT_HEADER_CLL_MASK);
	cs = (FormatHeader & PLANAR_FORMAT_HEADER_CS) ? TRUE : FALSE;
	rle = (FormatHeader & PLANAR_FORMAT_HEADER_RLE) ? TRUE : FALSE;
	alpha = (FormatHeader & PLANAR_FORMAT_HEADER_NA) ? FALSE : TRUE;

	DstFormat = planar_invert_format(planar, alpha, DstFormat);

	if (alpha)
		useAlpha = FreeRDPColorHasAlpha(DstFormat);

	// WLog_INFO(TAG, "CLL: %"PRIu32" CS: %"PRIu8" RLE: %"PRIu8" ALPHA: %"PRIu8"", cll, cs, rle,
	// alpha);

	if (!cll && cs)
	{
		WLog_ERR(TAG, "Chroma subsampling requires YCoCg and does not work with RGB data");
		return FALSE; /* Chroma subsampling requires YCoCg */
	}

	subWidth = (nSrcWidth / 2) + (nSrcWidth % 2);
	subHeight = (nSrcHeight / 2) + (nSrcHeight % 2);
	planeSize = nSrcWidth * nSrcHeight;
	subSize = subWidth * subHeight;

	if (!cs)
	{
		rawSizes[0] = planeSize; /* LumaOrRedPlane */
		rawWidths[0] = nSrcWidth;
		rawHeights[0] = nSrcHeight;
		rawSizes[1] = planeSize; /* OrangeChromaOrGreenPlane */
		rawWidths[1] = nSrcWidth;
		rawHeights[1] = nSrcHeight;
		rawSizes[2] = planeSize; /* GreenChromaOrBluePlane */
		rawWidths[2] = nSrcWidth;
		rawHeights[2] = nSrcHeight;
		rawSizes[3] = planeSize; /* AlphaPlane */
		rawWidths[3] = nSrcWidth;
		rawHeights[3] = nSrcHeight;
	}
	else /* Chroma Subsampling */
	{
		rawSizes[0] = planeSize; /* LumaOrRedPlane */
		rawWidths[0] = nSrcWidth;
		rawHeights[0] = nSrcHeight;
		rawSizes[1] = subSize; /* OrangeChromaOrGreenPlane */
		rawWidths[1] = subWidth;
		rawHeights[1] = subHeight;
		rawSizes[2] = subSize; /* GreenChromaOrBluePlane */
		rawWidths[2] = subWidth;
		rawHeights[2] = subHeight;
		rawSizes[3] = planeSize; /* AlphaPlane */
		rawWidths[3] = nSrcWidth;
		rawHeights[3] = nSrcHeight;
	}

	const size_t diff = srcp - pSrcData;
	if (SrcSize < diff)
	{
		WLog_ERR(TAG, "Size mismatch %" PRIu32 " < %" PRIuz, SrcSize, diff);
		return FALSE;
	}

	if (!rle) /* RAW */
	{

		UINT32 base = planeSize * 3;
		if (cs)
			base = planeSize + planeSize / 2;

		if (alpha)
		{
			if ((SrcSize - diff) < (planeSize + base))
			{
				WLog_ERR(TAG, "Alpha plane size mismatch %" PRIuz " < %" PRIu32, SrcSize - diff,
				         (planeSize + base));
				return FALSE;
			}

			planes[3] = srcp;                    /* AlphaPlane */
			planes[0] = planes[3] + rawSizes[3]; /* LumaOrRedPlane */
			planes[1] = planes[0] + rawSizes[0]; /* OrangeChromaOrGreenPlane */
			planes[2] = planes[1] + rawSizes[1]; /* GreenChromaOrBluePlane */

			if ((planes[2] + rawSizes[2]) > &pSrcData[SrcSize])
			{
				WLog_ERR(TAG, "plane size mismatch %p + %" PRIu32 " > %p", planes[2], rawSizes[2],
				         &pSrcData[SrcSize]);
				return FALSE;
			}
		}
		else
		{
			if ((SrcSize - diff) < base)
			{
				WLog_ERR(TAG, "plane size mismatch %" PRIu32 " < %" PRIu32, SrcSize - diff, base);
				return FALSE;
			}

			planes[0] = srcp;                    /* LumaOrRedPlane */
			planes[1] = planes[0] + rawSizes[0]; /* OrangeChromaOrGreenPlane */
			planes[2] = planes[1] + rawSizes[1]; /* GreenChromaOrBluePlane */

			if ((planes[2] + rawSizes[2]) > &pSrcData[SrcSize])
			{
				WLog_ERR(TAG, "plane size mismatch %p + %" PRIu32 " > %p", planes[2], rawSizes[2],
				         &pSrcData[SrcSize]);
				return FALSE;
			}
		}
	}
	else /* RLE */
	{
		if (alpha)
		{
			planes[3] = srcp;
			rleSizes[3] = planar_skip_plane_rle(planes[3], (UINT32)(SrcSize - diff), rawWidths[3],
			                                    rawHeights[3]); /* AlphaPlane */

			if (rleSizes[3] < 0)
				return FALSE;

			planes[0] = planes[3] + rleSizes[3];
		}
		else
			planes[0] = srcp;

		const size_t diff0 = (planes[0] - pSrcData);
		if (SrcSize < diff0)
		{
			WLog_ERR(TAG, "Size mismatch %" PRIu32 " < %" PRIuz, SrcSize, diff0);
			return FALSE;
		}
		rleSizes[0] = planar_skip_plane_rle(planes[0], (UINT32)(SrcSize - diff0), rawWidths[0],
		                                    rawHeights[0]); /* RedPlane */

		if (rleSizes[0] < 0)
			return FALSE;

		planes[1] = planes[0] + rleSizes[0];

		const size_t diff1 = (planes[1] - pSrcData);
		if (SrcSize < diff1)
		{
			WLog_ERR(TAG, "Size mismatch %" PRIu32 " < %" PRIuz, SrcSize, diff1);
			return FALSE;
		}
		rleSizes[1] = planar_skip_plane_rle(planes[1], (UINT32)(SrcSize - diff1), rawWidths[1],
		                                    rawHeights[1]); /* GreenPlane */

		if (rleSizes[1] < 1)
			return FALSE;

		planes[2] = planes[1] + rleSizes[1];
		const size_t diff2 = (planes[2] - pSrcData);
		if (SrcSize < diff2)
		{
			WLog_ERR(TAG, "Size mismatch %" PRIu32 " < %" PRIuz, SrcSize, diff);
			return FALSE;
		}
		rleSizes[2] = planar_skip_plane_rle(planes[2], (UINT32)(SrcSize - diff2), rawWidths[2],
		                                    rawHeights[2]); /* BluePlane */

		if (rleSizes[2] < 1)
			return FALSE;
	}

	if (!cll) /* RGB */
	{
		UINT32 TempFormat = 0;
		BYTE* pTempData = pDstData;
		UINT32 nTempStep = nDstStep;
		UINT32 nTotalHeight = nYDst + nDstHeight;

		if (useAlpha)
			TempFormat = PIXEL_FORMAT_BGRA32;
		else
			TempFormat = PIXEL_FORMAT_BGRX32;

		TempFormat = planar_invert_format(planar, alpha, TempFormat);

		if ((TempFormat != DstFormat) || (nSrcWidth != nDstWidth) || (nSrcHeight != nDstHeight))
		{
			pTempData = planar->pTempData;
			nTempStep = planar->nTempStep;
			nTotalHeight = planar->maxHeight;
		}

		if (!rle) /* RAW */
		{
			if (!planar_decompress_planes_raw(planes, pTempData, TempFormat, nTempStep, nXDst,
			                                  nYDst, nSrcWidth, nSrcHeight, vFlip, nTotalHeight))
				return FALSE;

			if (alpha)
				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2] + rawSizes[3];
			else /* NoAlpha */
				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2];

			if ((SrcSize - (srcp - pSrcData)) == 1)
				srcp++; /* pad */
		}
		else /* RLE */
		{
			status =
			    planar_decompress_plane_rle(planes[0], rleSizes[0], pTempData, nTempStep, nXDst,
			                                nYDst, nSrcWidth, nSrcHeight, 2, vFlip); /* RedPlane */

			if (status < 0)
				return FALSE;

			status = planar_decompress_plane_rle(planes[1], rleSizes[1], pTempData, nTempStep,
			                                     nXDst, nYDst, nSrcWidth, nSrcHeight, 1,
			                                     vFlip); /* GreenPlane */

			if (status < 0)
				return FALSE;

			status =
			    planar_decompress_plane_rle(planes[2], rleSizes[2], pTempData, nTempStep, nXDst,
			                                nYDst, nSrcWidth, nSrcHeight, 0, vFlip); /* BluePlane */

			if (status < 0)
				return FALSE;

			srcp += rleSizes[0] + rleSizes[1] + rleSizes[2];

			if (useAlpha)
			{
				status = planar_decompress_plane_rle(planes[3], rleSizes[3], pTempData, nTempStep,
				                                     nXDst, nYDst, nSrcWidth, nSrcHeight, 3,
				                                     vFlip); /* AlphaPlane */
			}
			else
				status = planar_set_plane(0xFF, pTempData, nTempStep, nXDst, nYDst, nSrcWidth,
				                          nSrcHeight, 3, vFlip);

			if (status < 0)
				return FALSE;

			if (alpha)
				srcp += rleSizes[3];
		}

		if (pTempData != pDstData)
		{
			if (!freerdp_image_copy_no_overlap(pDstData, DstFormat, nDstStep, nXDst, nYDst, w, h,
			                                   pTempData, TempFormat, nTempStep, nXDst, nYDst, NULL,
			                                   FREERDP_FLIP_NONE))
			{
				WLog_ERR(TAG, "planar image copy failed");
				return FALSE;
			}
		}
	}
	else /* YCoCg */
	{
		UINT32 TempFormat = 0;
		BYTE* pTempData = planar->pTempData;
		UINT32 nTempStep = planar->nTempStep;
		UINT32 nTotalHeight = planar->maxHeight;
		BYTE* dst = &pDstData[nXDst * FreeRDPGetBytesPerPixel(DstFormat) + nYDst * nDstStep];

		if (useAlpha)
			TempFormat = PIXEL_FORMAT_BGRA32;
		else
			TempFormat = PIXEL_FORMAT_BGRX32;

		if (!pTempData)
			return FALSE;

		if (rle) /* RLE encoded data. Decode and handle it like raw data. */
		{
			BYTE* rleBuffer[4] = { 0 };

			if (!planar->rlePlanesBuffer)
				return FALSE;

			rleBuffer[3] = planar->rlePlanesBuffer;  /* AlphaPlane */
			rleBuffer[0] = rleBuffer[3] + planeSize; /* LumaOrRedPlane */
			rleBuffer[1] = rleBuffer[0] + planeSize; /* OrangeChromaOrGreenPlane */
			rleBuffer[2] = rleBuffer[1] + planeSize; /* GreenChromaOrBluePlane */
			if (useAlpha)
			{
				status =
				    planar_decompress_plane_rle_only(planes[3], rleSizes[3], rleBuffer[3],
				                                     rawWidths[3], rawHeights[3]); /* AlphaPlane */

				if (status < 0)
					return FALSE;
			}

			if (alpha)
				srcp += rleSizes[3];

			status = planar_decompress_plane_rle_only(planes[0], rleSizes[0], rleBuffer[0],
			                                          rawWidths[0], rawHeights[0]); /* LumaPlane */

			if (status < 0)
				return FALSE;

			status =
			    planar_decompress_plane_rle_only(planes[1], rleSizes[1], rleBuffer[1], rawWidths[1],
			                                     rawHeights[1]); /* OrangeChromaPlane */

			if (status < 0)
				return FALSE;

			status =
			    planar_decompress_plane_rle_only(planes[2], rleSizes[2], rleBuffer[2], rawWidths[2],
			                                     rawHeights[2]); /* GreenChromaPlane */

			if (status < 0)
				return FALSE;

			planes[0] = rleBuffer[0];
			planes[1] = rleBuffer[1];
			planes[2] = rleBuffer[2];
			planes[3] = rleBuffer[3];
		}

		/* RAW */
		{
			if (cs)
			{ /* Chroma subsampling for Co and Cg:
			   * Each pixel contains the value that should be expanded to
			   * [2x,2y;2x+1,2y;2x+1,2y+1;2x;2y+1] */
				if (!planar_subsample_expand(planes[1], rawSizes[1], nSrcWidth, nSrcHeight,
				                             rawWidths[1], rawHeights[1], planar->deltaPlanes[0]))
					return FALSE;

				planes[1] = planar->deltaPlanes[0];
				rawSizes[1] = planeSize; /* OrangeChromaOrGreenPlane */
				rawWidths[1] = nSrcWidth;
				rawHeights[1] = nSrcHeight;

				if (!planar_subsample_expand(planes[2], rawSizes[2], nSrcWidth, nSrcHeight,
				                             rawWidths[2], rawHeights[2], planar->deltaPlanes[1]))
					return FALSE;

				planes[2] = planar->deltaPlanes[1];
				rawSizes[2] = planeSize; /* GreenChromaOrBluePlane */
				rawWidths[2] = nSrcWidth;
				rawHeights[2] = nSrcHeight;
			}

			if (!planar_decompress_planes_raw(planes, pTempData, TempFormat, nTempStep, nXDst,
			                                  nYDst, nSrcWidth, nSrcHeight, vFlip, nTotalHeight))
				return FALSE;

			if (alpha)
				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2] + rawSizes[3];
			else /* NoAlpha */
				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2];

			if ((SrcSize - (srcp - pSrcData)) == 1)
				srcp++; /* pad */
		}

		WINPR_ASSERT(prims->YCoCgToRGB_8u_AC4R);
		int rc = prims->YCoCgToRGB_8u_AC4R(pTempData, nTempStep, dst, DstFormat, nDstStep, w, h,
		                                   cll, useAlpha);
		if (rc != PRIMITIVES_SUCCESS)
		{
			WLog_ERR(TAG, "YCoCgToRGB_8u_AC4R failed with %d", rc);
			return FALSE;
		}
	}

	WINPR_UNUSED(srcp);
	return TRUE;
}

static INLINE BOOL freerdp_split_color_planes(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT planar,
                                              const BYTE* WINPR_RESTRICT data, UINT32 format,
                                              UINT32 width, UINT32 height, UINT32 scanline,
                                              BYTE* WINPR_RESTRICT planes[4])
{
	WINPR_ASSERT(planar);

	if ((width > INT32_MAX) || (height > INT32_MAX) || (scanline > INT32_MAX))
		return FALSE;

	if (scanline == 0)
		scanline = width * FreeRDPGetBytesPerPixel(format);

	if (planar->topdown)
	{
		UINT32 k = 0;
		for (UINT32 i = 0; i < height; i++)
		{
			const BYTE* pixel = &data[1ULL * scanline * i];

			for (UINT32 j = 0; j < width; j++)
			{
				const UINT32 color = FreeRDPReadColor(pixel, format);
				pixel += FreeRDPGetBytesPerPixel(format);
				FreeRDPSplitColor(color, format, &planes[1][k], &planes[2][k], &planes[3][k],
				                  &planes[0][k], NULL);
				k++;
			}
		}
	}
	else
	{
		UINT32 k = 0;

		for (INT64 i = (INT64)height - 1; i >= 0; i--)
		{
			const BYTE* pixel = &data[1ULL * scanline * (UINT32)i];

			for (UINT32 j = 0; j < width; j++)
			{
				const UINT32 color = FreeRDPReadColor(pixel, format);
				pixel += FreeRDPGetBytesPerPixel(format);
				FreeRDPSplitColor(color, format, &planes[1][k], &planes[2][k], &planes[3][k],
				                  &planes[0][k], NULL);
				k++;
			}
		}
	}
	return TRUE;
}

static INLINE UINT32 freerdp_bitmap_planar_write_rle_bytes(const BYTE* WINPR_RESTRICT pInBuffer,
                                                           UINT32 cRawBytes, UINT32 nRunLength,
                                                           BYTE* WINPR_RESTRICT pOutBuffer,
                                                           UINT32 outBufferSize)
{
	const BYTE* pInput = NULL;
	BYTE* pOutput = NULL;
	BYTE controlByte = 0;
	UINT32 nBytesToWrite = 0;
	pInput = pInBuffer;
	pOutput = pOutBuffer;

	if (!cRawBytes && !nRunLength)
		return 0;

	if (nRunLength < 3)
	{
		cRawBytes += nRunLength;
		nRunLength = 0;
	}

	while (cRawBytes)
	{
		if (cRawBytes < 16)
		{
			if (nRunLength > 15)
			{
				if (nRunLength < 18)
				{
					controlByte = PLANAR_CONTROL_BYTE(13, cRawBytes);
					nRunLength -= 13;
					cRawBytes = 0;
				}
				else
				{
					controlByte = PLANAR_CONTROL_BYTE(15, cRawBytes);
					nRunLength -= 15;
					cRawBytes = 0;
				}
			}
			else
			{
				controlByte = PLANAR_CONTROL_BYTE(nRunLength, cRawBytes);
				nRunLength = 0;
				cRawBytes = 0;
			}
		}
		else
		{
			controlByte = PLANAR_CONTROL_BYTE(0, 15);
			cRawBytes -= 15;
		}

		if (outBufferSize < 1)
			return 0;

		outBufferSize--;
		*pOutput = controlByte;
		pOutput++;
		nBytesToWrite = (controlByte >> 4);

		if (nBytesToWrite)
		{
			if (outBufferSize < nBytesToWrite)
				return 0;

			outBufferSize -= nBytesToWrite;
			CopyMemory(pOutput, pInput, nBytesToWrite);
			pOutput += nBytesToWrite;
			pInput += nBytesToWrite;
		}
	}

	while (nRunLength)
	{
		if (nRunLength > 47)
		{
			if (nRunLength < 50)
			{
				controlByte = PLANAR_CONTROL_BYTE(2, 13);
				nRunLength -= 45;
			}
			else
			{
				controlByte = PLANAR_CONTROL_BYTE(2, 15);
				nRunLength -= 47;
			}
		}
		else if (nRunLength > 31)
		{
			controlByte = PLANAR_CONTROL_BYTE(2, (nRunLength - 32));
			nRunLength = 0;
		}
		else if (nRunLength > 15)
		{
			controlByte = PLANAR_CONTROL_BYTE(1, (nRunLength - 16));
			nRunLength = 0;
		}
		else
		{
			controlByte = PLANAR_CONTROL_BYTE(nRunLength, 0);
			nRunLength = 0;
		}

		if (outBufferSize < 1)
			return 0;

		--outBufferSize;
		*pOutput = controlByte;
		pOutput++;
	}

	const intptr_t diff = (pOutput - pOutBuffer);
	if ((diff < 0) || (diff > UINT32_MAX))
		return 0;
	return (UINT32)diff;
}

static INLINE UINT32 freerdp_bitmap_planar_encode_rle_bytes(const BYTE* WINPR_RESTRICT pInBuffer,
                                                            UINT32 inBufferSize,
                                                            BYTE* WINPR_RESTRICT pOutBuffer,
                                                            UINT32 outBufferSize)
{
	BYTE symbol = 0;
	const BYTE* pInput = NULL;
	BYTE* pOutput = NULL;
	const BYTE* pBytes = NULL;
	UINT32 cRawBytes = 0;
	UINT32 nRunLength = 0;
	UINT32 bSymbolMatch = 0;
	UINT32 nBytesWritten = 0;
	UINT32 nTotalBytesWritten = 0;
	symbol = 0;
	cRawBytes = 0;
	nRunLength = 0;
	pInput = pInBuffer;
	pOutput = pOutBuffer;
	nTotalBytesWritten = 0;

	if (!outBufferSize)
		return 0;

	do
	{
		if (!inBufferSize)
			break;

		bSymbolMatch = (symbol == *pInput) ? TRUE : FALSE;
		symbol = *pInput;
		pInput++;
		inBufferSize--;

		if (nRunLength && !bSymbolMatch)
		{
			if (nRunLength < 3)
			{
				cRawBytes += nRunLength;
				nRunLength = 0;
			}
			else
			{
				pBytes = pInput - (cRawBytes + nRunLength + 1);
				nBytesWritten = freerdp_bitmap_planar_write_rle_bytes(pBytes, cRawBytes, nRunLength,
				                                                      pOutput, outBufferSize);
				nRunLength = 0;

				if (!nBytesWritten || (nBytesWritten > outBufferSize))
					return nRunLength;

				nTotalBytesWritten += nBytesWritten;
				outBufferSize -= nBytesWritten;
				pOutput += nBytesWritten;
				cRawBytes = 0;
			}
		}

		nRunLength += bSymbolMatch;
		cRawBytes += (!bSymbolMatch) ? TRUE : FALSE;
	} while (outBufferSize);

	if (cRawBytes || nRunLength)
	{
		pBytes = pInput - (cRawBytes + nRunLength);
		nBytesWritten = freerdp_bitmap_planar_write_rle_bytes(pBytes, cRawBytes, nRunLength,
		                                                      pOutput, outBufferSize);

		if (!nBytesWritten)
			return 0;

		nTotalBytesWritten += nBytesWritten;
	}

	if (inBufferSize)
		return 0;

	return nTotalBytesWritten;
}

BOOL freerdp_bitmap_planar_compress_plane_rle(const BYTE* WINPR_RESTRICT inPlane, UINT32 width,
                                              UINT32 height, BYTE* WINPR_RESTRICT outPlane,
                                              UINT32* WINPR_RESTRICT dstSize)
{
	UINT32 index = 0;
	const BYTE* pInput = NULL;
	BYTE* pOutput = NULL;
	UINT32 outBufferSize = 0;
	UINT32 nBytesWritten = 0;
	UINT32 nTotalBytesWritten = 0;

	if (!outPlane)
		return FALSE;

	index = 0;
	pInput = inPlane;
	pOutput = outPlane;
	outBufferSize = *dstSize;
	nTotalBytesWritten = 0;

	while (outBufferSize)
	{
		nBytesWritten =
		    freerdp_bitmap_planar_encode_rle_bytes(pInput, width, pOutput, outBufferSize);

		if ((!nBytesWritten) || (nBytesWritten > outBufferSize))
			return FALSE;

		outBufferSize -= nBytesWritten;
		nTotalBytesWritten += nBytesWritten;
		pOutput += nBytesWritten;
		pInput += width;
		index++;

		if (index >= height)
			break;
	}

	*dstSize = nTotalBytesWritten;
	return TRUE;
}

static INLINE BOOL freerdp_bitmap_planar_compress_planes_rle(BYTE* WINPR_RESTRICT inPlanes[4],
                                                             UINT32 width, UINT32 height,
                                                             BYTE* WINPR_RESTRICT outPlanes,
                                                             UINT32* WINPR_RESTRICT dstSizes,
                                                             BOOL skipAlpha)
{
	UINT32 outPlanesSize = width * height * 4;

	/* AlphaPlane */
	if (skipAlpha)
	{
		dstSizes[0] = 0;
	}
	else
	{
		dstSizes[0] = outPlanesSize;

		if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[0], width, height, outPlanes,
		                                              &dstSizes[0]))
			return FALSE;

		outPlanes += dstSizes[0];
		outPlanesSize -= dstSizes[0];
	}

	/* LumaOrRedPlane */
	dstSizes[1] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[1], width, height, outPlanes,
	                                              &dstSizes[1]))
		return FALSE;

	outPlanes += dstSizes[1];
	outPlanesSize -= dstSizes[1];
	/* OrangeChromaOrGreenPlane */
	dstSizes[2] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[2], width, height, outPlanes,
	                                              &dstSizes[2]))
		return FALSE;

	outPlanes += dstSizes[2];
	outPlanesSize -= dstSizes[2];
	/* GreenChromeOrBluePlane */
	dstSizes[3] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[3], width, height, outPlanes,
	                                              &dstSizes[3]))
		return FALSE;

	return TRUE;
}

BYTE* freerdp_bitmap_planar_delta_encode_plane(const BYTE* WINPR_RESTRICT inPlane, UINT32 width,
                                               UINT32 height, BYTE* WINPR_RESTRICT outPlane)
{
	char s2c = 0;
	BYTE* outPtr = NULL;
	const BYTE* srcPtr = NULL;
	const BYTE* prevLinePtr = NULL;

	if (!outPlane)
	{
		if (width * height == 0)
			return NULL;

		if (!(outPlane = (BYTE*)calloc(height, width)))
			return NULL;
	}

	// first line is copied as is
	CopyMemory(outPlane, inPlane, width);
	outPtr = outPlane + width;
	srcPtr = inPlane + width;
	prevLinePtr = inPlane;

	for (UINT32 y = 1; y < height; y++)
	{
		for (UINT32 x = 0; x < width; x++, outPtr++, srcPtr++, prevLinePtr++)
		{
			INT32 delta = *srcPtr - *prevLinePtr;
			s2c = (delta >= 0) ? (char)delta : (char)(~((BYTE)(-delta)) + 1);
			s2c = (s2c >= 0) ? (char)((UINT32)s2c << 1)
			                 : (char)(((UINT32)(~((BYTE)s2c) + 1) << 1) - 1);
			*outPtr = (BYTE)s2c;
		}
	}

	return outPlane;
}

static INLINE BOOL freerdp_bitmap_planar_delta_encode_planes(BYTE* WINPR_RESTRICT inPlanes[4],
                                                             UINT32 width, UINT32 height,
                                                             BYTE* WINPR_RESTRICT outPlanes[4])
{
	for (UINT32 i = 0; i < 4; i++)
	{
		outPlanes[i] =
		    freerdp_bitmap_planar_delta_encode_plane(inPlanes[i], width, height, outPlanes[i]);

		if (!outPlanes[i])
			return FALSE;
	}

	return TRUE;
}

BYTE* freerdp_bitmap_compress_planar(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT context,
                                     const BYTE* WINPR_RESTRICT data, UINT32 format, UINT32 width,
                                     UINT32 height, UINT32 scanline, BYTE* WINPR_RESTRICT dstData,
                                     UINT32* WINPR_RESTRICT pDstSize)
{
	UINT32 size = 0;
	BYTE* dstp = NULL;
	UINT32 planeSize = 0;
	UINT32 dstSizes[4] = { 0 };
	BYTE FormatHeader = 0;

	if (!context || !context->rlePlanesBuffer)
		return NULL;

	if (context->AllowSkipAlpha)
		FormatHeader |= PLANAR_FORMAT_HEADER_NA;

	planeSize = width * height;

	if (!context->AllowSkipAlpha)
		format = planar_invert_format(context, TRUE, format);

	if (!freerdp_split_color_planes(context, data, format, width, height, scanline,
	                                context->planes))
		return NULL;

	if (context->AllowRunLengthEncoding)
	{
		if (!freerdp_bitmap_planar_delta_encode_planes(context->planes, width, height,
		                                               context->deltaPlanes))
			return NULL;

		if (!freerdp_bitmap_planar_compress_planes_rle(context->deltaPlanes, width, height,
		                                               context->rlePlanesBuffer, dstSizes,
		                                               context->AllowSkipAlpha))
			return NULL;

		{
			int offset = 0;
			FormatHeader |= PLANAR_FORMAT_HEADER_RLE;
			context->rlePlanes[0] = &context->rlePlanesBuffer[offset];
			offset += dstSizes[0];
			context->rlePlanes[1] = &context->rlePlanesBuffer[offset];
			offset += dstSizes[1];
			context->rlePlanes[2] = &context->rlePlanesBuffer[offset];
			offset += dstSizes[2];
			context->rlePlanes[3] = &context->rlePlanesBuffer[offset];

#if defined(WITH_DEBUG_CODECS)
			WLog_DBG(TAG,
			         "R: [%" PRIu32 "/%" PRIu32 "] G: [%" PRIu32 "/%" PRIu32 "] B: [%" PRIu32
			         " / %" PRIu32 "] ",
			         dstSizes[1], planeSize, dstSizes[2], planeSize, dstSizes[3], planeSize);
#endif
		}
	}

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		if (!context->AllowRunLengthEncoding)
			return NULL;

		if (context->rlePlanes[0] == NULL)
			return NULL;

		if (context->rlePlanes[1] == NULL)
			return NULL;

		if (context->rlePlanes[2] == NULL)
			return NULL;

		if (context->rlePlanes[3] == NULL)
			return NULL;
	}

	if (!dstData)
	{
		size = 1;

		if (!(FormatHeader & PLANAR_FORMAT_HEADER_NA))
		{
			if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
				size += dstSizes[0];
			else
				size += planeSize;
		}

		if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
			size += (dstSizes[1] + dstSizes[2] + dstSizes[3]);
		else
			size += (planeSize * 3);

		if (!(FormatHeader & PLANAR_FORMAT_HEADER_RLE))
			size++;

		dstData = malloc(size);

		if (!dstData)
			return NULL;

		*pDstSize = size;
	}

	dstp = dstData;
	*dstp = FormatHeader; /* FormatHeader */
	dstp++;

	/* AlphaPlane */

	if (!(FormatHeader & PLANAR_FORMAT_HEADER_NA))
	{
		if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
		{
			CopyMemory(dstp, context->rlePlanes[0], dstSizes[0]); /* Alpha */
			dstp += dstSizes[0];
		}
		else
		{
			CopyMemory(dstp, context->planes[0], planeSize); /* Alpha */
			dstp += planeSize;
		}
	}

	/* LumaOrRedPlane */

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		CopyMemory(dstp, context->rlePlanes[1], dstSizes[1]); /* Red */
		dstp += dstSizes[1];
	}
	else
	{
		CopyMemory(dstp, context->planes[1], planeSize); /* Red */
		dstp += planeSize;
	}

	/* OrangeChromaOrGreenPlane */

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		CopyMemory(dstp, context->rlePlanes[2], dstSizes[2]); /* Green */
		dstp += dstSizes[2];
	}
	else
	{
		CopyMemory(dstp, context->planes[2], planeSize); /* Green */
		dstp += planeSize;
	}

	/* GreenChromeOrBluePlane */

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		CopyMemory(dstp, context->rlePlanes[3], dstSizes[3]); /* Blue */
		dstp += dstSizes[3];
	}
	else
	{
		CopyMemory(dstp, context->planes[3], planeSize); /* Blue */
		dstp += planeSize;
	}

	/* Pad1 (1 byte) */

	if (!(FormatHeader & PLANAR_FORMAT_HEADER_RLE))
	{
		*dstp = 0;
		dstp++;
	}

	const intptr_t diff = (dstp - dstData);
	if ((diff < 0) || (diff > UINT32_MAX))
	{
		free(dstData);
		return NULL;
	}
	size = (UINT32)diff;
	*pDstSize = size;
	return dstData;
}

BOOL freerdp_bitmap_planar_context_reset(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT context,
                                         UINT32 width, UINT32 height)
{
	if (!context)
		return FALSE;

	context->bgr = FALSE;
	context->maxWidth = PLANAR_ALIGN(width, 4);
	context->maxHeight = PLANAR_ALIGN(height, 4);
	{
		const UINT64 tmp = (UINT64)context->maxWidth * context->maxHeight;
		if (tmp > UINT32_MAX)
			return FALSE;
		context->maxPlaneSize = (UINT32)tmp;
	}

	if (context->maxWidth > UINT32_MAX / 4)
		return FALSE;
	context->nTempStep = context->maxWidth * 4;

	memset((void*)context->planes, 0, sizeof(context->planes));
	memset((void*)context->rlePlanes, 0, sizeof(context->rlePlanes));
	memset((void*)context->deltaPlanes, 0, sizeof(context->deltaPlanes));

	if (context->maxPlaneSize > 0)
	{
		void* tmp = winpr_aligned_recalloc(context->planesBuffer, context->maxPlaneSize, 4, 32);
		if (!tmp)
			return FALSE;
		context->planesBuffer = tmp;

		tmp = winpr_aligned_recalloc(context->pTempData, context->maxPlaneSize, 6, 32);
		if (!tmp)
			return FALSE;
		context->pTempData = tmp;

		tmp = winpr_aligned_recalloc(context->deltaPlanesBuffer, context->maxPlaneSize, 4, 32);
		if (!tmp)
			return FALSE;
		context->deltaPlanesBuffer = tmp;

		tmp = winpr_aligned_recalloc(context->rlePlanesBuffer, context->maxPlaneSize, 4, 32);
		if (!tmp)
			return FALSE;
		context->rlePlanesBuffer = tmp;

		context->planes[0] = &context->planesBuffer[0ULL * context->maxPlaneSize];
		context->planes[1] = &context->planesBuffer[1ULL * context->maxPlaneSize];
		context->planes[2] = &context->planesBuffer[2ULL * context->maxPlaneSize];
		context->planes[3] = &context->planesBuffer[3ULL * context->maxPlaneSize];
		context->deltaPlanes[0] = &context->deltaPlanesBuffer[0ULL * context->maxPlaneSize];
		context->deltaPlanes[1] = &context->deltaPlanesBuffer[1ULL * context->maxPlaneSize];
		context->deltaPlanes[2] = &context->deltaPlanesBuffer[2ULL * context->maxPlaneSize];
		context->deltaPlanes[3] = &context->deltaPlanesBuffer[3ULL * context->maxPlaneSize];
	}
	return TRUE;
}

BITMAP_PLANAR_CONTEXT* freerdp_bitmap_planar_context_new(DWORD flags, UINT32 maxWidth,
                                                         UINT32 maxHeight)
{
	BITMAP_PLANAR_CONTEXT* context =
	    (BITMAP_PLANAR_CONTEXT*)winpr_aligned_calloc(1, sizeof(BITMAP_PLANAR_CONTEXT), 32);

	if (!context)
		return NULL;

	if (flags & PLANAR_FORMAT_HEADER_NA)
		context->AllowSkipAlpha = TRUE;

	if (flags & PLANAR_FORMAT_HEADER_RLE)
		context->AllowRunLengthEncoding = TRUE;

	if (flags & PLANAR_FORMAT_HEADER_CS)
		context->AllowColorSubsampling = TRUE;

	context->ColorLossLevel = flags & PLANAR_FORMAT_HEADER_CLL_MASK;

	if (context->ColorLossLevel)
		context->AllowDynamicColorFidelity = TRUE;

	if (!freerdp_bitmap_planar_context_reset(context, maxWidth, maxHeight))
	{
		WINPR_PRAGMA_DIAG_PUSH
		WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
		freerdp_bitmap_planar_context_free(context);
		WINPR_PRAGMA_DIAG_POP
		return NULL;
	}

	return context;
}

void freerdp_bitmap_planar_context_free(BITMAP_PLANAR_CONTEXT* context)
{
	if (!context)
		return;

	winpr_aligned_free(context->pTempData);
	winpr_aligned_free(context->planesBuffer);
	winpr_aligned_free(context->deltaPlanesBuffer);
	winpr_aligned_free(context->rlePlanesBuffer);
	winpr_aligned_free(context);
}

void freerdp_planar_switch_bgr(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT planar, BOOL bgr)
{
	WINPR_ASSERT(planar);
	planar->bgr = bgr;
}

void freerdp_planar_topdown_image(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT planar, BOOL topdown)
{
	WINPR_ASSERT(planar);
	planar->topdown = topdown;
}
