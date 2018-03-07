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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/primitives.h>
#include <freerdp/log.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/planar.h>

#define TAG FREERDP_TAG("codec")

static INLINE BYTE* freerdp_bitmap_planar_compress_plane_rle(
    const BYTE* plane, UINT32 width, UINT32 height,
    BYTE* outPlane, UINT32* dstSize);
static INLINE BYTE* freerdp_bitmap_planar_delta_encode_plane(
    const BYTE* inPlane, UINT32 width, UINT32 height, BYTE* outPlane);

static INLINE INT32 planar_skip_plane_rle(const BYTE* pSrcData, UINT32 SrcSize,
        UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y;
	BYTE controlByte;
	const BYTE* pRLE = pSrcData;
	const BYTE* pEnd = &pSrcData[SrcSize];

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth;)
		{
			int cRawBytes;
			int nRunLength;

			if (pRLE >= pEnd)
				return -1;

			controlByte = *pRLE++;
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

			pRLE += cRawBytes;
			x += cRawBytes;
			x += nRunLength;

			if (x > nWidth)
				return -1;

			if (pRLE > pEnd)
				return -1;
		}
	}

	return (INT32)(pRLE - pSrcData);
}

static INLINE INT32 planar_decompress_plane_rle(const BYTE* pSrcData, UINT32 SrcSize,
        BYTE* pDstData, INT32 nDstStep,
        UINT32 nXDst, UINT32 nYDst,
        UINT32 nWidth, UINT32 nHeight,
        UINT32 nChannel, BOOL vFlip)
{
	UINT32 x, y;
	UINT32 pixel;
	UINT32 cRawBytes;
	UINT32 nRunLength;
	INT32 deltaValue;
	INT32 beg, end, inc;
	BYTE controlByte;
	BYTE* currentScanline;
	BYTE* previousScanline;
	const BYTE* srcp = pSrcData;
	previousScanline = NULL;

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

	for (y = beg; y != end; y += inc)
	{
		BYTE* dstp = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4) + nChannel];
		pixel = 0;
		currentScanline = dstp;

		for (x = 0; x < nWidth;)
		{
			controlByte = *srcp;
			srcp++;

			if ((srcp - pSrcData) > SrcSize)
			{
				WLog_ERR(TAG,  "error reading input buffer");
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

			if (((dstp + (cRawBytes + nRunLength)) - currentScanline) > nWidth * 4)
			{
				WLog_ERR(TAG,  "too many pixels in scanline");
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

					deltaValue = previousScanline[x * 4] + pixel;
					*dstp = deltaValue;
					dstp += 4;
					x++;
					cRawBytes--;
				}

				while (nRunLength > 0)
				{
					deltaValue = previousScanline[x * 4] + pixel;
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

static INLINE BOOL writeLine(BYTE** ppRgba, UINT32 DstFormat, UINT32 width, const BYTE** ppR,
                             const BYTE** ppG, const BYTE** ppB, const BYTE** ppA)
{
	UINT32 x;

	if (!ppRgba || !ppR || !ppG || !ppB)
		return FALSE;

	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
			for (x = 0; x < width; x++)
			{
				*(*ppRgba)++ = *(*ppB)++;
				*(*ppRgba)++ = *(*ppG)++;
				*(*ppRgba)++ = *(*ppR)++;
				*(*ppRgba)++ = *(*ppA)++;
			}

			return TRUE;

		case PIXEL_FORMAT_BGRX32:
			for (x = 0; x < width; x++)
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
				for (x = 0; x < width; x++)
				{
					BYTE alpha = *(*ppA)++;
					UINT32 color = FreeRDPGetColor(DstFormat, *(*ppR)++, *(*ppG)++, *(*ppB)++, alpha);
					WriteColor(*ppRgba, DstFormat, color);
					*ppRgba += GetBytesPerPixel(DstFormat);
				}
			}
			else
			{
				const BYTE alpha = 0xFF;

				for (x = 0; x < width; x++)
				{
					UINT32 color = FreeRDPGetColor(DstFormat, *(*ppR)++, *(*ppG)++, *(*ppB)++, alpha);
					WriteColor(*ppRgba, DstFormat, color);
					*ppRgba += GetBytesPerPixel(DstFormat);
				}
			}

			return TRUE;
	}
}

static INLINE BOOL planar_decompress_planes_raw(const BYTE* pSrcData[4],
        BYTE* pDstData, UINT32 DstFormat,
        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
        BOOL alpha, BOOL vFlip)
{
	INT32 y;
	INT32 beg, end, inc;
	const BYTE* pR = pSrcData[0];
	const BYTE* pG = pSrcData[1];
	const BYTE* pB = pSrcData[2];
	const BYTE* pA = pSrcData[3];

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

	if (alpha)
	{
		for (y = beg; y != end; y += inc)
		{
			BYTE* pRGB = &pDstData[((nYDst + y) * nDstStep) + (nXDst * GetBytesPerPixel(
			                                        DstFormat))];

			if (!writeLine(&pRGB, DstFormat, nWidth, &pR, &pG, &pB, &pA))
				return FALSE;
		}
	}
	else
	{
		for (y = beg; y != end; y += inc)
		{
			BYTE* pRGB = &pDstData[((nYDst + y) * nDstStep) + (nXDst * GetBytesPerPixel(
			                                        DstFormat))];

			if (!writeLine(&pRGB, DstFormat, nWidth, &pR, &pG, &pB, &pA))
				return FALSE;
		}
	}

	return TRUE;
}

BOOL planar_decompress(BITMAP_PLANAR_CONTEXT* planar,
                       const BYTE* pSrcData, UINT32 SrcSize,
                       UINT32 nSrcWidth, UINT32 nSrcHeight,
                       BYTE* pDstData, UINT32 DstFormat,
                       UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                       UINT32 nDstWidth, UINT32 nDstHeight, BOOL vFlip)
{
	BOOL cs;
	BOOL rle;
	UINT32 cll;
	BOOL alpha;
	INT32 status;
	const BYTE* srcp;
	UINT32 subSize;
	UINT32 subWidth;
	UINT32 subHeight;
	UINT32 planeSize;
	INT32 rleSizes[4] = { 0, 0, 0, 0 };
	UINT32 rawSizes[4];
	UINT32 rawWidths[4];
	UINT32 rawHeights[4];
	BYTE FormatHeader;
	const BYTE* planes[4];
	const UINT32 w = MIN(nSrcWidth, nDstWidth);
	const UINT32 h = MIN(nSrcHeight, nDstHeight);
	const primitives_t* prims = primitives_get();

	if (nDstStep <= 0)
		nDstStep = nDstWidth * GetBytesPerPixel(DstFormat);

	srcp = pSrcData;

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

	//WLog_INFO(TAG, "CLL: %"PRIu32" CS: %"PRIu8" RLE: %"PRIu8" ALPHA: %"PRIu8"", cll, cs, rle, alpha);

	if (!cll && cs)
		return FALSE; /* Chroma subsampling requires YCoCg */

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

	if (!rle) /* RAW */
	{
		if (alpha)
		{
			planes[3] = srcp; /* AlphaPlane */
			planes[0] = planes[3] + rawSizes[3]; /* LumaOrRedPlane */
			planes[1] = planes[0] + rawSizes[0]; /* OrangeChromaOrGreenPlane */
			planes[2] = planes[1] + rawSizes[1]; /* GreenChromaOrBluePlane */

			if ((planes[2] + rawSizes[2]) > &pSrcData[SrcSize])
				return FALSE;
		}
		else
		{
			if ((SrcSize - (srcp - pSrcData)) < (planeSize * 3))
				return FALSE;

			planes[0] = srcp; /* LumaOrRedPlane */
			planes[1] = planes[0] + rawSizes[0]; /* OrangeChromaOrGreenPlane */
			planes[2] = planes[1] + rawSizes[1]; /* GreenChromaOrBluePlane */

			if ((planes[2] + rawSizes[2]) > &pSrcData[SrcSize])
				return FALSE;
		}
	}
	else /* RLE */
	{
		if (alpha)
		{
			planes[3] = srcp;
			rleSizes[3] = planar_skip_plane_rle(planes[3], SrcSize - (planes[3] - pSrcData),
			                                    rawWidths[3], rawHeights[3]); /* AlphaPlane */

			if (rleSizes[3] < 0)
				return FALSE;

			planes[0] = planes[3] + rleSizes[3];
			rleSizes[0] = planar_skip_plane_rle(planes[0], SrcSize - (planes[0] - pSrcData),
			                                    rawWidths[0], rawHeights[0]); /* RedPlane */

			if (rleSizes[0] < 0)
				return FALSE;

			planes[1] = planes[0] + rleSizes[0];
			rleSizes[1] = planar_skip_plane_rle(planes[1], SrcSize - (planes[1] - pSrcData),
			                                    rawWidths[1], rawHeights[1]); /* GreenPlane */

			if (rleSizes[1] < 1)
				return FALSE;

			planes[2] = planes[1] + rleSizes[1];
			rleSizes[2] = planar_skip_plane_rle(planes[2], SrcSize - (planes[2] - pSrcData),
			                                    rawWidths[2], rawHeights[2]); /* BluePlane */

			if (rleSizes[2] < 1)
				return FALSE;
		}
		else
		{
			planes[0] = srcp;
			rleSizes[0] = planar_skip_plane_rle(planes[0], SrcSize - (planes[0] - pSrcData),
			                                    rawWidths[0], rawHeights[0]); /* RedPlane */

			if (rleSizes[0] < 0)
				return FALSE;

			planes[1] = planes[0] + rleSizes[0];
			rleSizes[1] = planar_skip_plane_rle(planes[1], SrcSize - (planes[1] - pSrcData),
			                                    rawWidths[1], rawHeights[1]); /* GreenPlane */

			if (rleSizes[1] < 1)
				return FALSE;

			planes[2] = planes[1] + rleSizes[1];
			rleSizes[2] = planar_skip_plane_rle(planes[2], SrcSize - (planes[2] - pSrcData),
			                                    rawWidths[2], rawHeights[2]); /* BluePlane */

			if (rleSizes[2] < 1)
				return FALSE;
		}
	}

	if (!cll) /* RGB */
	{
		UINT32 TempFormat;
		BYTE* pTempData = pDstData;
		UINT32 nTempStep = nDstStep;

		if (alpha)
			TempFormat = PIXEL_FORMAT_BGRA32;
		else
			TempFormat = PIXEL_FORMAT_BGRX32;

		if ((TempFormat != DstFormat) || (nSrcWidth != nDstWidth)
		    || (nSrcHeight != nDstHeight))
		{
			pTempData = planar->pTempData;
			nTempStep = planar->nTempStep;
		}

		if (!rle) /* RAW */
		{
			if (alpha)
			{
				if (!planar_decompress_planes_raw(planes, pTempData, TempFormat, nTempStep,
				                                  nXDst, nYDst, nSrcWidth, nSrcHeight, alpha, vFlip))
					return FALSE;

				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2] + rawSizes[3];
			}
			else /* NoAlpha */
			{
				if (!planar_decompress_planes_raw(planes, pTempData, TempFormat, nTempStep,
				                                  nXDst, nYDst, nSrcWidth, nSrcHeight, alpha, vFlip))
					return FALSE;

				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2];
			}

			if ((SrcSize - (srcp - pSrcData)) == 1)
				srcp++; /* pad */
		}
		else /* RLE */
		{
			if (alpha)
			{
				status = planar_decompress_plane_rle(planes[3], rleSizes[3],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 3,
				                                     vFlip); /* AlphaPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[0], rleSizes[0],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 2,
				                                     vFlip); /* RedPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[1], rleSizes[1],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 1,
				                                     vFlip); /* GreenPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[2], rleSizes[2],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 0,
				                                     vFlip); /* BluePlane */

				if (status < 0)
					return FALSE;

				srcp += rleSizes[0] + rleSizes[1] + rleSizes[2] + rleSizes[3];
			}
			else /* NoAlpha */
			{
				const UINT32 color = FreeRDPGetColor(TempFormat, 0, 0, 0, 0xFF);

				if (!freerdp_image_fill(pTempData, TempFormat, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight,
				                        color))
					return FALSE;

				status = planar_decompress_plane_rle(planes[0], rleSizes[0],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 2,
				                                     vFlip); /* RedPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[1], rleSizes[1],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 1,
				                                     vFlip); /* GreenPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[2], rleSizes[2],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 0,
				                                     vFlip); /* BluePlane */

				if (status < 0)
					return FALSE;

				srcp += rleSizes[0] + rleSizes[1] + rleSizes[2];
			}
		}

		if (pTempData != pDstData)
		{
			if (!freerdp_image_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst, w,
			                        h, pTempData,
			                        TempFormat, nTempStep, nXDst, nYDst, NULL, FREERDP_FLIP_NONE))
				return FALSE;
		}
	}
	else /* YCoCg */
	{
		UINT32 TempFormat;
		BYTE* pTempData = planar->pTempData;
		UINT32 nTempStep = planar->nTempStep;

		if (alpha)
			TempFormat = PIXEL_FORMAT_BGRA32;
		else
			TempFormat = PIXEL_FORMAT_BGRX32;

		if (!pTempData)
			return FALSE;

		if (cs)
		{
			WLog_ERR(TAG, "Chroma subsampling unimplemented");
			return FALSE;
		}

		if (!rle) /* RAW */
		{
			if (alpha)
			{
				if (!planar_decompress_planes_raw(planes, pTempData, TempFormat, nTempStep,
				                                  nXDst, nYDst, nSrcWidth, nSrcHeight, alpha, vFlip))
					return FALSE;

				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2] + rawSizes[3];
			}
			else /* NoAlpha */
			{
				if (!planar_decompress_planes_raw(planes, pTempData, TempFormat, nTempStep,
				                                  nXDst, nYDst, nSrcWidth, nSrcHeight, alpha, vFlip))
					return FALSE;

				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2];
			}

			if ((SrcSize - (srcp - pSrcData)) == 1)
				srcp++; /* pad */
		}
		else /* RLE */
		{
			if (alpha)
			{
				status = planar_decompress_plane_rle(planes[3], rleSizes[3],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 3,
				                                     vFlip); /* AlphaPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[0], rleSizes[0],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 2,
				                                     vFlip); /* LumaPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[1], rleSizes[1],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 1,
				                                     vFlip); /* OrangeChromaPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[2], rleSizes[2],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 0,
				                                     vFlip); /* GreenChromaPlane */

				if (status < 0)
					return FALSE;

				srcp += rleSizes[0] + rleSizes[1] + rleSizes[2] + rleSizes[3];
			}
			else /* NoAlpha */
			{
				status = planar_decompress_plane_rle(planes[0], rleSizes[0],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 2,
				                                     vFlip); /* LumaPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[1], rleSizes[1],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 1,
				                                     vFlip); /* OrangeChromaPlane */

				if (status < 0)
					return FALSE;

				status = planar_decompress_plane_rle(planes[2], rleSizes[2],
				                                     pTempData, nTempStep, nXDst, nYDst, nSrcWidth, nSrcHeight, 0,
				                                     vFlip); /* GreenChromaPlane */

				if (status < 0)
					return FALSE;

				srcp += rleSizes[0] + rleSizes[1] + rleSizes[2];
			}
		}

		if (prims->YCoCgToRGB_8u_AC4R(pTempData, nTempStep, pDstData, DstFormat,
		                              nDstStep,
		                              w, h, cll, alpha) != PRIMITIVES_SUCCESS)
			return FALSE;
	}

	return (SrcSize == (srcp - pSrcData)) ? TRUE : FALSE;
}

static INLINE BOOL freerdp_split_color_planes(const BYTE* data, UINT32 format,
        UINT32 width, UINT32 height,
        UINT32 scanline, BYTE* planes[4])
{
	INT32 i, j, k;
	k = 0;

	if (scanline == 0)
		scanline = width * GetBytesPerPixel(format);

	for (i = height - 1; i >= 0; i--)
	{
		const BYTE* pixel = &data[scanline * i];

		for (j = 0; j < width; j++)
		{
			const UINT32 color = ReadColor(pixel, format);
			pixel += GetBytesPerPixel(format);
			SplitColor(color, format, &planes[1][k], &planes[2][k],
			           &planes[3][k], &planes[0][k], NULL);
			k++;
		}
	}

	return TRUE;
}

static INLINE UINT32 freerdp_bitmap_planar_write_rle_bytes(
    const BYTE* pInBuffer, UINT32 cRawBytes, UINT32 nRunLength,
    BYTE* pOutBuffer, UINT32 outBufferSize)
{
	const BYTE* pInput;
	BYTE* pOutput;
	BYTE controlByte;
	UINT32 nBytesToWrite;
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
		nBytesToWrite = (int)(controlByte >> 4);

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

	return (pOutput - pOutBuffer);
}

static INLINE UINT32 freerdp_bitmap_planar_encode_rle_bytes(const BYTE* pInBuffer,
        UINT32 inBufferSize,
        BYTE* pOutBuffer,
        UINT32 outBufferSize)
{
	BYTE symbol;
	const BYTE* pInput;
	BYTE* pOutput;
	const BYTE* pBytes;
	UINT32 cRawBytes;
	UINT32 nRunLength;
	UINT32 bSymbolMatch;
	UINT32 nBytesWritten;
	UINT32 nTotalBytesWritten;
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
				nBytesWritten = freerdp_bitmap_planar_write_rle_bytes(
				                    pBytes, cRawBytes,
				                    nRunLength, pOutput,
				                    outBufferSize);
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
	}
	while (outBufferSize);

	if (cRawBytes || nRunLength)
	{
		pBytes = pInput - (cRawBytes + nRunLength);
		nBytesWritten = freerdp_bitmap_planar_write_rle_bytes(pBytes,
		                cRawBytes, nRunLength, pOutput, outBufferSize);

		if (!nBytesWritten)
			return 0;

		nTotalBytesWritten += nBytesWritten;
	}

	if (inBufferSize)
		return 0;

	return nTotalBytesWritten;
}

BYTE* freerdp_bitmap_planar_compress_plane_rle(const BYTE* inPlane,
        UINT32 width, UINT32 height,
        BYTE* outPlane, UINT32* dstSize)
{
	UINT32 index;
	const BYTE* pInput;
	BYTE* pOutput;
	UINT32 outBufferSize;
	UINT32 nBytesWritten;
	UINT32 nTotalBytesWritten;

	if (!outPlane)
	{
		outBufferSize = width * height;

		if (outBufferSize == 0)
			return NULL;

		outPlane = malloc(outBufferSize);

		if (!outPlane)
			return NULL;
	}
	else
	{
		outBufferSize = *dstSize;
	}

	index = 0;
	pInput = inPlane;
	pOutput = outPlane;
	nTotalBytesWritten = 0;

	while (outBufferSize)
	{
		nBytesWritten = freerdp_bitmap_planar_encode_rle_bytes(
		                    pInput, width, pOutput, outBufferSize);

		if ((!nBytesWritten) || (nBytesWritten > outBufferSize))
			return NULL;

		outBufferSize -= nBytesWritten;
		nTotalBytesWritten += nBytesWritten;
		pOutput += nBytesWritten;
		pInput += width;
		index++;

		if (index >= height)
			break;
	}

	*dstSize = nTotalBytesWritten;
	return outPlane;
}

static INLINE BOOL freerdp_bitmap_planar_compress_planes_rle(
    BYTE* inPlanes[4], UINT32 width, UINT32 height,
    BYTE* outPlanes, UINT32* dstSizes, BOOL skipAlpha)
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

		if (!freerdp_bitmap_planar_compress_plane_rle(
		        inPlanes[0], width, height, outPlanes, &dstSizes[0]))
			return FALSE;

		outPlanes += dstSizes[0];
		outPlanesSize -= dstSizes[0];
	}

	/* LumaOrRedPlane */
	dstSizes[1] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[1], width, height,
	        outPlanes, &dstSizes[1]))
		return FALSE;

	outPlanes += dstSizes[1];
	outPlanesSize -= dstSizes[1];
	/* OrangeChromaOrGreenPlane */
	dstSizes[2] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[2], width, height,
	        outPlanes, &dstSizes[2]))
		return FALSE;

	outPlanes += dstSizes[2];
	outPlanesSize -= dstSizes[2];
	/* GreenChromeOrBluePlane */
	dstSizes[3] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[3], width, height,
	        outPlanes, &dstSizes[3]))
		return FALSE;

	return TRUE;
}

BYTE* freerdp_bitmap_planar_delta_encode_plane(const BYTE* inPlane,
        UINT32 width, UINT32 height,
        BYTE* outPlane)
{
	char s2c;
	INT32 delta;
	UINT32 y, x;
	BYTE* outPtr;
	const BYTE* srcPtr, *prevLinePtr;

	if (!outPlane)
	{
		if (width * height == 0)
			return NULL;

		if (!(outPlane = (BYTE*) calloc(height, width)))
			return NULL;
	}

	// first line is copied as is
	CopyMemory(outPlane, inPlane, width);
	outPtr = outPlane + width;
	srcPtr = inPlane + width;
	prevLinePtr = inPlane;

	for (y = 1; y < height; y++)
	{
		for (x = 0; x < width; x++, outPtr++, srcPtr++, prevLinePtr++)
		{
			delta = *srcPtr - *prevLinePtr;
			s2c = (delta >= 0) ? (char) delta : (char)(~((BYTE)(-delta)) + 1);
			s2c = (s2c >= 0) ? (s2c << 1) : (char)(((~((BYTE) s2c) + 1) << 1) - 1);
			*outPtr = (BYTE)s2c;
		}
	}

	return outPlane;
}

static INLINE BOOL freerdp_bitmap_planar_delta_encode_planes(BYTE* inPlanes[4],
        UINT32 width, UINT32 height,
        BYTE* outPlanes[4])
{
	UINT32 i;

	for (i = 0; i < 4; i++)
	{
		outPlanes[i] = freerdp_bitmap_planar_delta_encode_plane(
		                   inPlanes[i], width, height, outPlanes[i]);

		if (!outPlanes[i])
			return FALSE;
	}

	return TRUE;
}

BYTE* freerdp_bitmap_compress_planar(BITMAP_PLANAR_CONTEXT* context,
                                     const BYTE* data, UINT32 format,
                                     UINT32 width, UINT32 height, UINT32 scanline,
                                     BYTE* dstData, UINT32* pDstSize)
{
	UINT32 size;
	BYTE* dstp;
	UINT32 planeSize;
	UINT32 dstSizes[4] = { 0 };
	BYTE FormatHeader = 0;

	if (!context || !context->rlePlanesBuffer)
		return NULL;

	if (context->AllowSkipAlpha)
		FormatHeader |= PLANAR_FORMAT_HEADER_NA;

	planeSize = width * height;

	if (!freerdp_split_color_planes(data, format, width, height, scanline,
	                                context->planes))
		return NULL;

	if (context->AllowRunLengthEncoding)
	{
		if (!freerdp_bitmap_planar_delta_encode_planes(
		        context->planes, width, height,
		        context->deltaPlanes))
			return NULL;

		if (!freerdp_bitmap_planar_compress_planes_rle(
		        context->deltaPlanes, width, height,
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
			//WLog_DBG(TAG, "R: [%"PRIu32"/%"PRIu32"] G: [%"PRIu32"/%"PRIu32"] B: [%"PRIu32"/%"PRIu32"]",
			//		dstSizes[1], planeSize, dstSizes[2], planeSize, dstSizes[3], planeSize);
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

	size = (dstp - dstData);
	*pDstSize = size;
	return dstData;
}

BOOL freerdp_bitmap_planar_context_reset(
    BITMAP_PLANAR_CONTEXT* context, UINT32 width, UINT32 height)
{
	if (!context)
		return FALSE;

	context->maxWidth = width;
	context->maxHeight = height;
	context->maxPlaneSize = context->maxWidth * context->maxHeight;
	context->nTempStep = context->maxWidth * 4;
	free(context->planesBuffer);
	free(context->pTempData);
	free(context->deltaPlanesBuffer);
	free(context->rlePlanesBuffer);
	context->planesBuffer = calloc(context->maxPlaneSize, 4);
	context->pTempData = calloc(context->maxPlaneSize, 4);
	context->deltaPlanesBuffer = calloc(context->maxPlaneSize, 4);
	context->rlePlanesBuffer = calloc(context->maxPlaneSize, 4);

	if (!context->planesBuffer || !context->pTempData ||
	    !context->deltaPlanesBuffer || !context->rlePlanesBuffer)
		return FALSE;

	context->planes[0] = &context->planesBuffer[context->maxPlaneSize * 0];
	context->planes[1] = &context->planesBuffer[context->maxPlaneSize * 1];
	context->planes[2] = &context->planesBuffer[context->maxPlaneSize * 2];
	context->planes[3] = &context->planesBuffer[context->maxPlaneSize * 3];
	context->deltaPlanes[0] = &context->deltaPlanesBuffer[context->maxPlaneSize *
	                          0];
	context->deltaPlanes[1] = &context->deltaPlanesBuffer[context->maxPlaneSize *
	                          1];
	context->deltaPlanes[2] = &context->deltaPlanesBuffer[context->maxPlaneSize *
	                          2];
	context->deltaPlanes[3] = &context->deltaPlanesBuffer[context->maxPlaneSize *
	                          3];
	return TRUE;
}

BITMAP_PLANAR_CONTEXT* freerdp_bitmap_planar_context_new(
    DWORD flags, UINT32 maxWidth, UINT32 maxHeight)
{
	BITMAP_PLANAR_CONTEXT* context;
	context = (BITMAP_PLANAR_CONTEXT*) calloc(1, sizeof(BITMAP_PLANAR_CONTEXT));

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
		freerdp_bitmap_planar_context_free(context);
		return NULL;
	}

	return context;
}

void freerdp_bitmap_planar_context_free(BITMAP_PLANAR_CONTEXT* context)
{
	if (!context)
		return;

	free(context->pTempData);
	free(context->planesBuffer);
	free(context->deltaPlanesBuffer);
	free(context->rlePlanesBuffer);
	free(context);
}
