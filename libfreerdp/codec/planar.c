/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP6 Planar Codec
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

static int planar_skip_plane_rle(const BYTE* pSrcData, UINT32 SrcSize, int nWidth, int nHeight)
{
	int x, y;
	int cRawBytes;
	int nRunLength;
	BYTE controlByte;
	const BYTE* pRLE = pSrcData;
	const BYTE* pEnd = &pSrcData[SrcSize];

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; )
		{
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
			cRawBytes = 0;

			x += nRunLength;
			nRunLength = 0;

			if (x > nWidth)
				return -1;

			if (pRLE > pEnd)
				return -1;
		}
	}

	return (int) (pRLE - pSrcData);
}

static int planar_decompress_plane_rle(const BYTE* pSrcData, UINT32 SrcSize, BYTE* pDstData,
		int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight, int nChannel, BOOL vFlip)
{
	int x, y;
	BYTE* dstp;
	UINT32 pixel;
	int cRawBytes;
	int nRunLength;
	int deltaValue;
	int beg, end, inc;
	BYTE controlByte;
	BYTE* currentScanline;
	BYTE* previousScanline;
	const BYTE* srcp = pSrcData;

	dstp = pDstData;
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
		dstp = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4) + nChannel];

		pixel = 0;
		currentScanline = dstp;

		for (x = 0; x < nWidth; )
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

	return (int) (srcp - pSrcData);
}

static int planar_decompress_planes_raw(const BYTE* pSrcData[4], int nSrcStep, BYTE* pDstData,
		int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight, BOOL alpha, BOOL vFlip)
{
	int x, y;
	int beg, end, inc;
	BYTE* pRGB = pDstData;
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
			pRGB = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];

			for (x = 0; x < nWidth; x++)
			{
				*pRGB++ = *pB++;
				*pRGB++ = *pG++;
				*pRGB++ = *pR++;
				*pRGB++ = *pA++;
			}
		}
	}
	else
	{
		for (y = beg; y != end; y += inc)
		{
			pRGB = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];

			for (x = 0; x < nWidth; x++)
			{
				*pRGB++ = *pB++;
				*pRGB++ = *pG++;
				*pRGB++ = *pR++;
				*pRGB++ = 0xFF;
			}
		}
	}

	return 1;
}

int planar_decompress(BITMAP_PLANAR_CONTEXT* planar, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight, BOOL vFlip)
{
	BOOL cs;
	BOOL rle;
	UINT32 cll;
	BOOL alpha;
	int status;
	BYTE* srcp;
	int subSize;
	int subWidth;
	int subHeight;
	int planeSize;
	BYTE* pDstData;
	int rleSizes[4];
	int rawSizes[4];
	int rawWidths[4];
	int rawHeights[4];
	BYTE FormatHeader;
	BOOL useTempBuffer;
	int dstBitsPerPixel;
	int dstBytesPerPixel;
	const BYTE* planes[4];
	UINT32 UncompressedSize;
	const primitives_t* prims = primitives_get();

	if ((nWidth < 0) || (nHeight < 0))
		return -1;

	dstBitsPerPixel = FREERDP_PIXEL_FORMAT_DEPTH(DstFormat);
	dstBytesPerPixel = (FREERDP_PIXEL_FORMAT_BPP(DstFormat) / 8);

	if (nDstStep < 0)
		nDstStep = nWidth * 4;

	srcp = pSrcData;
	UncompressedSize = nWidth * nHeight * 4;

	pDstData = *ppDstData;

	if (!pDstData)
	{
		pDstData = (BYTE*) _aligned_malloc(UncompressedSize, 16);

		if (!pDstData)
			return -1;

		*ppDstData = pDstData;
	}

	useTempBuffer = (dstBytesPerPixel != 4) ? TRUE : FALSE;

	if (useTempBuffer)
	{
		if (UncompressedSize > planar->TempSize)
		{
			planar->TempBuffer = _aligned_realloc(planar->TempBuffer, UncompressedSize, 16);
			planar->TempSize = UncompressedSize;
		}

		if (!planar->TempBuffer)
			return -1;

		pDstData = planar->TempBuffer;
	}

	FormatHeader = *srcp++;

	cll = (FormatHeader & PLANAR_FORMAT_HEADER_CLL_MASK);
	cs = (FormatHeader & PLANAR_FORMAT_HEADER_CS) ? TRUE : FALSE;
	rle = (FormatHeader & PLANAR_FORMAT_HEADER_RLE) ? TRUE : FALSE;
	alpha = (FormatHeader & PLANAR_FORMAT_HEADER_NA) ? FALSE : TRUE;

	//WLog_INFO(TAG, "CLL: %d CS: %d RLE: %d ALPHA: %d", cll, cs, rle, alpha);

	if (!cll && cs)
		return -1; /* Chroma subsampling requires YCoCg */

	subWidth = (nWidth / 2) + (nWidth % 2);
	subHeight = (nHeight / 2) + (nHeight % 2);

	planeSize = nWidth * nHeight;
	subSize = subWidth * subHeight;

	if (!cs)
	{
		rawSizes[0] = planeSize; /* LumaOrRedPlane */
		rawWidths[0] = nWidth;
		rawHeights[0] = nHeight;

		rawSizes[1] = planeSize; /* OrangeChromaOrGreenPlane */
		rawWidths[1] = nWidth;
		rawHeights[1] = nHeight;

		rawSizes[2] = planeSize; /* GreenChromaOrBluePlane */
		rawWidths[2] = nWidth;
		rawHeights[2] = nHeight;

		rawSizes[3] = planeSize; /* AlphaPlane */
		rawWidths[3] = nWidth;
		rawHeights[3] = nHeight;
	}
	else /* Chroma Subsampling */
	{
		rawSizes[0] = planeSize; /* LumaOrRedPlane */
		rawWidths[0] = nWidth;
		rawHeights[0] = nHeight;

		rawSizes[1] = subSize; /* OrangeChromaOrGreenPlane */
		rawWidths[1] = subWidth;
		rawHeights[1] = subHeight;

		rawSizes[2] = subSize; /* GreenChromaOrBluePlane */
		rawWidths[2] = subWidth;
		rawHeights[2] = subHeight;

		rawSizes[3] = planeSize; /* AlphaPlane */
		rawWidths[3] = nWidth;
		rawHeights[3] = nHeight;
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
				return -1;
		}
		else
		{
			if ((SrcSize - (srcp - pSrcData)) < (planeSize * 3))
				return -1;

			planes[0] = srcp; /* LumaOrRedPlane */
			planes[1] = planes[0] + rawSizes[0]; /* OrangeChromaOrGreenPlane */
			planes[2] = planes[1] + rawSizes[1]; /* GreenChromaOrBluePlane */

			if ((planes[2] + rawSizes[2]) > &pSrcData[SrcSize])
				return -1;
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
				return -1;

			planes[0] = planes[3] + rleSizes[3];
			rleSizes[0] = planar_skip_plane_rle(planes[0], SrcSize - (planes[0] - pSrcData),
					rawWidths[0], rawHeights[0]); /* RedPlane */

			if (rleSizes[0] < 0)
				return -1;

			planes[1] = planes[0] + rleSizes[0];
			rleSizes[1] = planar_skip_plane_rle(planes[1], SrcSize - (planes[1] - pSrcData),
					rawWidths[1], rawHeights[1]); /* GreenPlane */

			if (rleSizes[1] < 1)
				return -1;

			planes[2] = planes[1] + rleSizes[1];
			rleSizes[2] = planar_skip_plane_rle(planes[2], SrcSize - (planes[2] - pSrcData),
					rawWidths[2], rawHeights[2]); /* BluePlane */

			if (rleSizes[2] < 1)
				return -1;
		}
		else
		{
			planes[0] = srcp;
			rleSizes[0] = planar_skip_plane_rle(planes[0], SrcSize - (planes[0] - pSrcData),
					rawWidths[0], rawHeights[0]); /* RedPlane */

			if (rleSizes[0] < 0)
				return -1;

			planes[1] = planes[0] + rleSizes[0];
			rleSizes[1] = planar_skip_plane_rle(planes[1], SrcSize - (planes[1] - pSrcData),
					rawWidths[1], rawHeights[1]); /* GreenPlane */

			if (rleSizes[1] < 1)
				return -1;

			planes[2] = planes[1] + rleSizes[1];
			rleSizes[2] = planar_skip_plane_rle(planes[2], SrcSize - (planes[2] - pSrcData),
					rawWidths[2], rawHeights[2]); /* BluePlane */

			if (rleSizes[2] < 1)
				return -1;
		}
	}

	if (!cll) /* RGB */
	{
		if (!rle) /* RAW */
		{
			if (alpha)
			{
				planar_decompress_planes_raw(planes, nWidth, pDstData, nDstStep,
						nXDst, nYDst, nWidth, nHeight, alpha, vFlip);

				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2] + rawSizes[3];
			}
			else /* NoAlpha */
			{
				planar_decompress_planes_raw(planes, nWidth, pDstData, nDstStep,
						nXDst, nYDst, nWidth, nHeight, alpha, vFlip);

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
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 3, vFlip); /* AlphaPlane */

				status = planar_decompress_plane_rle(planes[0], rleSizes[0],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 2, vFlip); /* RedPlane */

				status = planar_decompress_plane_rle(planes[1], rleSizes[1],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 1, vFlip); /* GreenPlane */

				status = planar_decompress_plane_rle(planes[2], rleSizes[2],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 0, vFlip); /* BluePlane */

				srcp += rleSizes[0] + rleSizes[1] + rleSizes[2] + rleSizes[3];
			}
			else /* NoAlpha */
			{
				status = planar_decompress_plane_rle(planes[0], rleSizes[0],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 2, vFlip); /* RedPlane */

				status = planar_decompress_plane_rle(planes[1], rleSizes[1],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 1, vFlip); /* GreenPlane */

				status = planar_decompress_plane_rle(planes[2], rleSizes[2],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 0, vFlip); /* BluePlane */

				srcp += rleSizes[0] + rleSizes[1] + rleSizes[2];
			}
		}
	}
	else /* YCoCg */
	{
		if (cs)
		{
			WLog_ERR(TAG, "Chroma subsampling unimplemented");
			return -1;
		}

		if (!rle) /* RAW */
		{
			if (alpha)
			{
				planar_decompress_planes_raw(planes, nWidth, pDstData, nDstStep,
						nXDst, nYDst, nWidth, nHeight, alpha, vFlip);

				srcp += rawSizes[0] + rawSizes[1] + rawSizes[2] + rawSizes[3];
			}
			else /* NoAlpha */
			{
				planar_decompress_planes_raw(planes, nWidth, pDstData, nDstStep,
						nXDst, nYDst, nWidth, nHeight, alpha, vFlip);

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
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 3, vFlip); /* AlphaPlane */

				status = planar_decompress_plane_rle(planes[0], rleSizes[0],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 2, vFlip); /* LumaPlane */

				status = planar_decompress_plane_rle(planes[1], rleSizes[1],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 1, vFlip); /* OrangeChromaPlane */

				status = planar_decompress_plane_rle(planes[2], rleSizes[2],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 0, vFlip); /* GreenChromaPlane */

				srcp += rleSizes[0] + rleSizes[1] + rleSizes[2] + rleSizes[3];
			}
			else /* NoAlpha */
			{
				status = planar_decompress_plane_rle(planes[0], rleSizes[0],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 2, vFlip); /* LumaPlane */

				status = planar_decompress_plane_rle(planes[1], rleSizes[1],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 1, vFlip); /* OrangeChromaPlane */

				status = planar_decompress_plane_rle(planes[2], rleSizes[2],
						pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 0, vFlip); /* GreenChromaPlane */

				srcp += rleSizes[0] + rleSizes[1] + rleSizes[2];
			}
		}

		prims->YCoCgToRGB_8u_AC4R(pDstData, nDstStep, pDstData, nDstStep, nWidth, nHeight, cll, alpha, FALSE);
	}

	status = (SrcSize == (srcp - pSrcData)) ? 1 : -1;

	if (status < 0)
		return status;

	if (useTempBuffer)
	{
		pDstData = *ppDstData;

		status = freerdp_image_copy(pDstData, DstFormat, -1, 0, 0, nWidth, nHeight,
				planar->TempBuffer, PIXEL_FORMAT_XRGB32, -1, 0, 0, NULL);
	}

	return status;
}

int freerdp_split_color_planes(BYTE* data, UINT32 format, int width, int height, int scanline, BYTE* planes[4])
{
	int bpp;
	int i, j, k;

	k = 0;
	bpp = FREERDP_PIXEL_FORMAT_BPP(format);

	if (bpp == 32)
	{
		UINT32* pixel;

		for (i = height - 1; i >= 0; i--)
		{
			pixel = (UINT32*) &data[scanline * i];

			for (j = 0; j < width; j++)
			{
				GetARGB32(planes[0][k], planes[1][k], planes[2][k], planes[3][k], *pixel);
				pixel++;
				k++;
			}
		}
	}
	else if (bpp == 24)
	{
		UINT32* pixel;

		for (i = height - 1; i >= 0; i--)
		{
			pixel = (UINT32*) &data[scanline * i];

			for (j = 0; j < width; j++)
			{
				GetRGB32(planes[1][k], planes[2][k], planes[3][k], *pixel);
				planes[0][k] = 0xFF; /* A */
				pixel++;
				k++;
			}
		}
	}
	else
	{
		return -1;
	}

	return 0;
}

int freerdp_bitmap_planar_write_rle_bytes(BYTE* pInBuffer, int cRawBytes, int nRunLength, BYTE* pOutBuffer, int outBufferSize)
{
	BYTE* pInput;
	BYTE* pOutput;
	BYTE controlByte;
	int nBytesToWrite;

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

		nBytesToWrite = (int) (controlByte >> 4);

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

int freerdp_bitmap_planar_encode_rle_bytes(BYTE* pInBuffer, int inBufferSize, BYTE* pOutBuffer, int outBufferSize)
{
	BYTE symbol;
	BYTE* pInput;
	BYTE* pOutput;
	BYTE* pBytes;
	int cRawBytes;
	int nRunLength;
	int bSymbolMatch;
	int nBytesWritten;
	int nTotalBytesWritten;

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

				nBytesWritten = freerdp_bitmap_planar_write_rle_bytes(pBytes,
						cRawBytes, nRunLength, pOutput, outBufferSize);

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

BYTE* freerdp_bitmap_planar_compress_plane_rle(BYTE* inPlane, int width, int height, BYTE* outPlane, int* dstSize)
{
	int index;
	BYTE* pInput;
	BYTE* pOutput;
	int outBufferSize;
	int nBytesWritten;
	int nTotalBytesWritten;

	if (!outPlane)
	{
		outBufferSize = width * height;
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
		nBytesWritten = freerdp_bitmap_planar_encode_rle_bytes(pInput, width, pOutput, outBufferSize);

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

int freerdp_bitmap_planar_compress_planes_rle(BYTE* inPlanes[4], int width, int height, BYTE* outPlanes, int* dstSizes, BOOL skipAlpha)
{
	int outPlanesSize = width * height * 4;

	/* AlphaPlane */

	if (skipAlpha)
	{
		dstSizes[0] = 0;
	}
	else
	{
		dstSizes[0] = outPlanesSize;

		if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[0], width, height, outPlanes, &dstSizes[0]))
			return 0;

		outPlanes += dstSizes[0];
		outPlanesSize -= dstSizes[0];
	}

	/* LumaOrRedPlane */

	dstSizes[1] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[1], width, height, outPlanes, &dstSizes[1]))
		return 0;

	outPlanes += dstSizes[1];
	outPlanesSize -= dstSizes[1];

	/* OrangeChromaOrGreenPlane */

	dstSizes[2] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[2], width, height, outPlanes, &dstSizes[2]))
		return 0;

	outPlanes += dstSizes[2];
	outPlanesSize -= dstSizes[2];

	/* GreenChromeOrBluePlane */

	dstSizes[3] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[3], width, height, outPlanes, &dstSizes[3]))
		return 0;

	outPlanes += dstSizes[3];
	outPlanesSize -= dstSizes[3];

	return 1;
}

BYTE* freerdp_bitmap_planar_delta_encode_plane(BYTE* inPlane, int width, int height, BYTE* outPlane)
{
	char s2c;
	int delta;
	int y, x;
	BYTE *outPtr, *srcPtr, *prevLinePtr;

	if (!outPlane)
	{
		if (!(outPlane = (BYTE*) malloc(width * height)))
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

			s2c = (delta >= 0) ? (char) delta : (char) (~((BYTE) (-delta)) + 1);

			s2c = (s2c >= 0) ? (s2c << 1) : (char) (((~((BYTE) s2c) + 1) << 1) - 1);

			*outPtr = (BYTE)s2c;
		}
	}

	return outPlane;
}

BOOL freerdp_bitmap_planar_delta_encode_planes(BYTE* inPlanes[4], int width, int height, BYTE* outPlanes[4])
{
	int i;

	for (i = 0; i < 4; i++)
	{
		outPlanes[i] = freerdp_bitmap_planar_delta_encode_plane(inPlanes[i], width, height, outPlanes[i]);
		if (!outPlanes[i])
			return FALSE;
	}

	return TRUE;
}

BYTE* freerdp_bitmap_compress_planar(BITMAP_PLANAR_CONTEXT* context, BYTE* data, UINT32 format,
		int width, int height, int scanline, BYTE* dstData, int* pDstSize)
{
	int size;
	BYTE* dstp;
	int planeSize;
	int dstSizes[4];
	BYTE FormatHeader = 0;

	if (context->AllowSkipAlpha)
		FormatHeader |= PLANAR_FORMAT_HEADER_NA;

	planeSize = width * height;

	if (freerdp_split_color_planes(data, format, width, height, scanline, context->planes) < 0)
	{
		return NULL;
	}

	if (context->AllowRunLengthEncoding)
	{
		if (!freerdp_bitmap_planar_delta_encode_planes(context->planes, width, height, context->deltaPlanes))
			return NULL;;

		if (freerdp_bitmap_planar_compress_planes_rle(context->deltaPlanes, width, height,
				context->rlePlanesBuffer, (int*) &dstSizes, context->AllowSkipAlpha) > 0)
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
			offset += dstSizes[3];
			//WLog_DBG(TAG, "R: [%d/%d] G: [%d/%d] B: [%d/%d]",
			//		dstSizes[1], planeSize, dstSizes[2], planeSize, dstSizes[3], planeSize);
		}
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

BOOL freerdp_bitmap_planar_context_reset(BITMAP_PLANAR_CONTEXT* context)
{
	if (!context)
		return FALSE;

	return TRUE;
}

BITMAP_PLANAR_CONTEXT* freerdp_bitmap_planar_context_new(DWORD flags, int maxWidth, int maxHeight)
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

	context->maxWidth = maxWidth;
	context->maxHeight = maxHeight;
	context->maxPlaneSize = context->maxWidth * context->maxHeight;

	context->planesBuffer = malloc(context->maxPlaneSize * 4);
	if (!context->planesBuffer)
		goto error_planesBuffer;
	context->planes[0] = &context->planesBuffer[context->maxPlaneSize * 0];
	context->planes[1] = &context->planesBuffer[context->maxPlaneSize * 1];
	context->planes[2] = &context->planesBuffer[context->maxPlaneSize * 2];
	context->planes[3] = &context->planesBuffer[context->maxPlaneSize * 3];

	context->deltaPlanesBuffer = malloc(context->maxPlaneSize * 4);
	if (!context->deltaPlanesBuffer)
		goto error_deltaPlanesBuffer;
	context->deltaPlanes[0] = &context->deltaPlanesBuffer[context->maxPlaneSize * 0];
	context->deltaPlanes[1] = &context->deltaPlanesBuffer[context->maxPlaneSize * 1];
	context->deltaPlanes[2] = &context->deltaPlanesBuffer[context->maxPlaneSize * 2];
	context->deltaPlanes[3] = &context->deltaPlanesBuffer[context->maxPlaneSize * 3];

	context->rlePlanesBuffer = malloc(context->maxPlaneSize * 4);
	if (!context->rlePlanesBuffer)
		goto error_rlePlanesBuffer;

	return context;

error_rlePlanesBuffer:
	free(context->deltaPlanesBuffer);
error_deltaPlanesBuffer:
	free(context->planesBuffer);
error_planesBuffer:
	free(context);
	return NULL;
}

void freerdp_bitmap_planar_context_free(BITMAP_PLANAR_CONTEXT* context)
{
	if (!context)
		return;

	free(context->planesBuffer);
	free(context->deltaPlanesBuffer);
	free(context->rlePlanesBuffer);

	free(context);
}
