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

#include <freerdp/codec/bitmap.h>
#include <freerdp/primitives.h>

#include "planar.h"

static int planar_decompress_plane_rle(BYTE* pSrcData, UINT32 SrcSize, BYTE* pDstData,
		int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight, int nChannel, BOOL vFlip)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	UINT32 pixel;
	int cRawBytes;
	int nRunLength;
	int deltaValue;
	int beg, end, inc;
	BYTE controlByte;
	BYTE* currentScanline;
	BYTE* previousScanline;

	srcp = pSrcData;
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
				fprintf(stderr, "planar_decompress_plane_rle: error reading input buffer\n");
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
				fprintf(stderr, "planar_decompress_plane_rle: too many pixels in scanline\n");
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

static int planar_decompress_plane_raw(BYTE* pSrcData, UINT32 SrcSize, BYTE* pDstData,
		int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight, int nChannel, BOOL vFlip)
{
	int x, y;
	int beg, end, inc;
	BYTE* dstp = NULL;
	BYTE* srcp = pSrcData;

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

		for (x = 0; x < nWidth; x++)
		{
			*dstp = *srcp;
			dstp += 4;
			srcp++;
		}
	}

	return (int) (srcp - pSrcData);
}

int planar_decompress(BITMAP_PLANAR_CONTEXT* planar, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	int status;
	BYTE* srcp;
	BOOL vFlip;
	BYTE FormatHeader;
	BYTE* pDstData = NULL;
	UINT32 UncompressedSize;

	if ((nWidth * nHeight) <= 0)
		return -1;

	vFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat) ? TRUE : FALSE;

	srcp = pSrcData;
	UncompressedSize = nWidth * nHeight * 4;

	pDstData = *ppDstData;

	if (!pDstData)
	{
		pDstData = (BYTE*) malloc(UncompressedSize);

		if (!pDstData)
			return -1;

		*ppDstData = pDstData;
	}

	FormatHeader = *srcp;
	srcp++;

	/* AlphaPlane */

	if (!(FormatHeader & PLANAR_FORMAT_HEADER_NA))
	{
		if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
		{
			status = planar_decompress_plane_rle(srcp, SrcSize - (srcp - pSrcData),
					pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 3, vFlip);

			if (status < 0)
				return -1;

			srcp += status;
		}
		else
		{
			status = planar_decompress_plane_raw(srcp, SrcSize - (srcp - pSrcData),
					pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 3, vFlip);

			if (status < 0)
				return -1;

			srcp += status;
		}
	}

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		/* LumaOrRedPlane */

		status = planar_decompress_plane_rle(srcp, SrcSize - (srcp - pSrcData),
				pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 2, vFlip);

		if (status < 0)
			return -1;

		srcp += status;

		/* OrangeChromaOrGreenPlane */

		status = planar_decompress_plane_rle(srcp, SrcSize - (srcp - pSrcData),
				pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 1, vFlip);

		if (status < 0)
			return -1;

		srcp += status;

		/* GreenChromeOrBluePlane */

		status = planar_decompress_plane_rle(srcp, SrcSize - (srcp - pSrcData),
				pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 0, vFlip);

		if (status < 0)
			return -1;

		srcp += status;
	}
	else
	{
		/* LumaOrRedPlane */

		status = planar_decompress_plane_raw(srcp, SrcSize - (srcp - pSrcData),
				pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 2, vFlip);

		if (status < 0)
			return -1;

		srcp += status;

		/* OrangeChromaOrGreenPlane */

		status = planar_decompress_plane_raw(srcp, SrcSize - (srcp - pSrcData),
				pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 1, vFlip);

		if (status < 0)
			return -1;

		srcp += status;

		/* GreenChromeOrBluePlane */

		status = planar_decompress_plane_raw(srcp, SrcSize - (srcp - pSrcData),
				pDstData, nDstStep, nXDst, nYDst, nWidth, nHeight, 0, vFlip);

		if (status < 0)
			return -1;

		srcp += status;
		srcp++;
	}

	if (FormatHeader & PLANAR_FORMAT_HEADER_CLL_MASK)
	{
		/* The data is in YCoCg colorspace rather than RGB. */
		if (FormatHeader & PLANAR_FORMAT_HEADER_CS)
		{
			static BOOL been_warned = FALSE;
			if (!been_warned)
				fprintf(stderr, "Chroma-Subsampling is not implemented.\n");
			been_warned = TRUE;
		}
		else
		{
			BOOL alpha;
			int cll;

			alpha = (FormatHeader & PLANAR_FORMAT_HEADER_NA) ? FALSE : TRUE;
			cll = FormatHeader & PLANAR_FORMAT_HEADER_CLL_MASK;
			primitives_get()->YCoCgRToRGB_8u_AC4R(
				pDstData, nDstStep, pDstData, nDstStep,
				nWidth, nHeight, cll, alpha, FALSE);
		}
	}

	status = (SrcSize == (srcp - pSrcData)) ? 1 : -1;

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
		outPlane = (BYTE*) malloc(width * height);

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

int freerdp_bitmap_planar_delta_encode_planes(BYTE* inPlanes[4], int width, int height, BYTE* outPlanes[4])
{
	freerdp_bitmap_planar_delta_encode_plane(inPlanes[0], width, height, outPlanes[0]);
	freerdp_bitmap_planar_delta_encode_plane(inPlanes[1], width, height, outPlanes[1]);
	freerdp_bitmap_planar_delta_encode_plane(inPlanes[2], width, height, outPlanes[2]);
	freerdp_bitmap_planar_delta_encode_plane(inPlanes[3], width, height, outPlanes[3]);

	return 0;
}

BYTE* freerdp_bitmap_compress_planar(BITMAP_PLANAR_CONTEXT* context, BYTE* data, UINT32 format,
		int width, int height, int scanline, BYTE* dstData, int* dstSize)
{
	int size;
	BYTE* dstp;
	int planeSize;
	int dstSizes[4];
	BYTE FormatHeader = 0;

	if (context->AllowSkipAlpha)
		FormatHeader |= PLANAR_FORMAT_HEADER_NA;

	planeSize = width * height;

	freerdp_split_color_planes(data, format, width, height, scanline, context->planes);

	if (context->AllowRunLengthEncoding)
	{
		freerdp_bitmap_planar_delta_encode_planes(context->planes, width, height, context->deltaPlanes);

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

			//printf("R: [%d/%d] G: [%d/%d] B: [%d/%d]\n",
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
		*dstSize = size;
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
	*dstSize = size;

	return dstData;
}

BITMAP_PLANAR_CONTEXT* freerdp_bitmap_planar_context_new(DWORD flags, int maxWidth, int maxHeight)
{
	BITMAP_PLANAR_CONTEXT* context;

	context = (BITMAP_PLANAR_CONTEXT*) malloc(sizeof(BITMAP_PLANAR_CONTEXT));

	if (!context)
		return NULL;

	ZeroMemory(context, sizeof(BITMAP_PLANAR_CONTEXT));

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
	context->planes[0] = &context->planesBuffer[context->maxPlaneSize * 0];
	context->planes[1] = &context->planesBuffer[context->maxPlaneSize * 1];
	context->planes[2] = &context->planesBuffer[context->maxPlaneSize * 2];
	context->planes[3] = &context->planesBuffer[context->maxPlaneSize * 3];

	context->deltaPlanesBuffer = malloc(context->maxPlaneSize * 4);
	context->deltaPlanes[0] = &context->deltaPlanesBuffer[context->maxPlaneSize * 0];
	context->deltaPlanes[1] = &context->deltaPlanesBuffer[context->maxPlaneSize * 1];
	context->deltaPlanes[2] = &context->deltaPlanesBuffer[context->maxPlaneSize * 2];
	context->deltaPlanes[3] = &context->deltaPlanesBuffer[context->maxPlaneSize * 3];

	context->rlePlanesBuffer = malloc(context->maxPlaneSize * 4);

	return context;
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
