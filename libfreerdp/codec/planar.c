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

#include "planar.h"

static int freerdp_bitmap_planar_decompress_plane_rle(BYTE* inPlane, int width, int height, BYTE* outPlane, int srcSize)
{
	int k;
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	UINT32 pixel;
	int scanline;
	int cRawBytes;
	int nRunLength;
	int deltaValue;
	BYTE controlByte;
	BYTE* currentScanline;
	BYTE* previousScanline;

	k = 0;

	srcp = inPlane;
	dstp = outPlane;
	scanline = width * 4;
	previousScanline = NULL;

	y = 0;

	while (y < height)
	{
		pixel = 0;
		dstp = (outPlane + height * scanline) - ((y + 1) * scanline);
		currentScanline = dstp;

		x = 0;

		while (x < width)
		{
			controlByte = *srcp;
			srcp++;

			if ((srcp - inPlane) > srcSize)
			{
				printf("freerdp_bitmap_planar_decompress_plane_rle: error reading input buffer\n");
				return -1;
			}

			nRunLength = PLANAR_CONTROL_BYTE_RUN_LENGTH(controlByte);
			cRawBytes = PLANAR_CONTROL_BYTE_RAW_BYTES(controlByte);

			//printf("CONTROL(%d, %d)\n", cRawBytes, nRunLength);

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

#if 0
			printf("y: %d cRawBytes: %d nRunLength: %d\n", y, cRawBytes, nRunLength);

			printf("RAW[");

			for (k = 0; k < cRawBytes; k++)
			{
				printf("0x%02X%s", srcp[k],
						((k + 1) == cRawBytes) ? "" : ", ");
			}

			printf("] RUN[%d]\n", nRunLength);
#endif

			if (((dstp + (cRawBytes + nRunLength)) - currentScanline) > width * 4)
			{
				printf("freerdp_bitmap_planar_decompress_plane_rle: too many pixels in scanline\n");
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
		y++;
	}

	return (int) (srcp - inPlane);
}

static int freerdp_bitmap_planar_decompress_plane_raw(BYTE* srcData, int width, int height, BYTE* dstData, int size)
{
	int x, y;

	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			dstData[(((height - y - 1) * width) + x) * 4] = srcData[((y * width) + x)];
		}
	}

	return (width * height);
}

//int g_DecompressCount = 0;

int freerdp_bitmap_planar_decompress(BYTE* srcData, BYTE* dstData, int width, int height, int size)
{
	BYTE* srcp;
	int dstSize;
	BYTE FormatHeader;

	//printf("freerdp_bitmap_planar_decompress: %d\n", ++g_DecompressCount);

	srcp = srcData;

	FormatHeader = *srcp;
	srcp++;

	if (!(FormatHeader & PLANAR_FORMAT_HEADER_NA))
	{
		if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
		{
			dstSize = freerdp_bitmap_planar_decompress_plane_rle(srcp, width, height, dstData + 3, size - (srcp - srcData));

			if (dstSize < 0)
				return -1;

			srcp += dstSize;
		}
		else
		{
			dstSize = freerdp_bitmap_planar_decompress_plane_raw(srcp, width, height, dstData + 3, size - (srcp - srcData));
			srcp += dstSize;
		}
	}

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		dstSize = freerdp_bitmap_planar_decompress_plane_rle(srcp, width, height, dstData + 2, size - (srcp - srcData));

		if (dstSize < 0)
			return -1;

		srcp += dstSize;

		dstSize = freerdp_bitmap_planar_decompress_plane_rle(srcp, width, height, dstData + 1, size - (srcp - srcData));

		if (dstSize < 0)
			return -1;

		srcp += dstSize;

		dstSize = freerdp_bitmap_planar_decompress_plane_rle(srcp, width, height, dstData + 0, size - (srcp - srcData));

		if (dstSize < 0)
			return -1;

		srcp += dstSize;
	}
	else
	{
		dstSize = freerdp_bitmap_planar_decompress_plane_raw(srcp, width, height, dstData + 2, size - (srcp - srcData));
		srcp += dstSize;

		dstSize = freerdp_bitmap_planar_decompress_plane_raw(srcp, width, height, dstData + 1, size - (srcp - srcData));
		srcp += dstSize;

		dstSize = freerdp_bitmap_planar_decompress_plane_raw(srcp, width, height, dstData + 0, size - (srcp - srcData));
		srcp += dstSize;
		srcp++;
	}

	return (size == (srcp - srcData)) ? 0 : -1;
}

int freerdp_split_color_planes(BYTE* data, UINT32 format, int width, int height, int scanline, BYTE* planes[5])
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

struct _PLANAR_RLE_CONTEXT
{
	int width;
	int height;
	BYTE* output;
	int nRunLength;
	int cRawBytes;
	BYTE* rawValues;
	BYTE* rawScanline;
	BYTE* outPlane;
	int outPlaneSize;
	int outSegmentSize;
	int nControlBytes;
};
typedef struct _PLANAR_RLE_CONTEXT PLANAR_RLE_CONTEXT;

int freerdp_bitmap_planar_compress_plane_rle_segment(PLANAR_RLE_CONTEXT* rle)
{
#if 0
	if (rle->nRunLength >= 3)
		printf("### cRawBytes: %d nRunLength: %d\n", rle->cRawBytes, rle->nRunLength);

	{
		int k;

		printf("RAW[");

		for (k = 0; k < rle->cRawBytes; k++)
		{
			printf("0x%02X%s", rle->rawValues[k],
					((k + 1) == rle->cRawBytes) ? "" : ", ");
		}

		printf("] RUN[%d]\n", rle->nRunLength);
	}
#endif

	while ((rle->cRawBytes != 0) || (rle->nRunLength != 0))
	{
		//printf("|| cRawBytes: %d nRunLength: %d\n", rle->cRawBytes, rle->nRunLength);

		if (rle->nRunLength < 3)
		{
			rle->cRawBytes += rle->nRunLength;
			rle->nRunLength = 0;
		}

		if ((rle->rawValues - rle->rawScanline) > rle->width)
		{
			printf("rawValues overflow! %d\n", rle->rawValues - rle->rawScanline);
			return 0;
		}

		if (rle->cRawBytes > 15)
		{
			rle->nControlBytes = 1;
			rle->outSegmentSize = 15 + rle->nControlBytes;

			if (((rle->output - rle->outPlane) + rle->outSegmentSize) > rle->outPlaneSize)
			{
				printf("overflow: %d > %d\n", ((rle->output - rle->outPlane) + rle->outSegmentSize), rle->outPlaneSize);
				return -1;
			}

			*rle->output = PLANAR_CONTROL_BYTE(0, 15);
			rle->output++;

			CopyMemory(rle->output, rle->rawValues, 15);
			rle->cRawBytes -= 15;
			rle->rawValues += 15;
			rle->output += 15;

			/* continue */
		}
		else if (rle->cRawBytes == 0)
		{
			if (rle->nRunLength > 47)
			{
				rle->nControlBytes = 1;
				rle->outSegmentSize = rle->nControlBytes;

				if (((rle->output - rle->outPlane) + rle->outSegmentSize) > rle->outPlaneSize)
				{
					printf("overflow: %d > %d\n", ((rle->output - rle->outPlane) + rle->outSegmentSize), rle->outPlaneSize);
					return -1;
				}

				*rle->output = PLANAR_CONTROL_BYTE(2, 15);
				rle->nRunLength -= 47;
				rle->output++;

				/* continue */
			}
			else if (rle->nRunLength > 31)
			{
				rle->nControlBytes = 1;
				rle->outSegmentSize = rle->nControlBytes;

				if (((rle->output - rle->outPlane) + rle->outSegmentSize) > rle->outPlaneSize)
				{
					printf("overflow: %d > %d\n", ((rle->output - rle->outPlane) + rle->outSegmentSize), rle->outPlaneSize);
					return -1;
				}

				*rle->output = PLANAR_CONTROL_BYTE(2, (rle->nRunLength - 32));
				rle->nRunLength -= 32;
				rle->output++;

				return 0; /* finish */
			}
			else if (rle->nRunLength > 15)
			{
				rle->nControlBytes = 1;
				rle->outSegmentSize = rle->nControlBytes;

				if (((rle->output - rle->outPlane) + rle->outSegmentSize) > rle->outPlaneSize)
				{
					printf("overflow: %d > %d\n", ((rle->output - rle->outPlane) + rle->outSegmentSize), rle->outPlaneSize);
					return -1;
				}

				*rle->output = PLANAR_CONTROL_BYTE(1, (rle->nRunLength - 16));
				rle->nRunLength -= 16;
				rle->output++;

				return 0; /* finish */
			}
			else
			{
				rle->nControlBytes = 1;
				rle->outSegmentSize = rle->cRawBytes + rle->nControlBytes;

				if (((rle->output - rle->outPlane) + rle->outSegmentSize) > rle->outPlaneSize)
				{
					printf("overflow: %d > %d\n", ((rle->output - rle->outPlane) + rle->outSegmentSize), rle->outPlaneSize);
					return -1;
				}

				*rle->output = PLANAR_CONTROL_BYTE(rle->nRunLength, rle->cRawBytes);
				rle->output++;

				if (rle->cRawBytes)
				{
					CopyMemory(rle->output, rle->rawValues, rle->cRawBytes);
					rle->rawValues += (rle->cRawBytes + rle->nRunLength);
					rle->output += rle->cRawBytes;
				}

				rle->cRawBytes = 0;
				rle->nRunLength = 0;

				return 0; /* finish */
			}
		}
		else if (rle->cRawBytes < 16)
		{
			if (rle->nRunLength > 15)
			{
				rle->nControlBytes = 2;
				rle->outSegmentSize = rle->cRawBytes + rle->nControlBytes;

				if (((rle->output - rle->outPlane) + rle->outSegmentSize) > rle->outPlaneSize)
				{
					printf("overflow: %d > %d\n", ((rle->output - rle->outPlane) + rle->outSegmentSize), rle->outPlaneSize);
					return -1;
				}

				if ((!rle->rawValues[rle->cRawBytes - 1]) && (rle->cRawBytes == 1))
				{
					//rle->rawValues += (rle->cRawBytes + rle->nRunLength);
					//rle->cRawBytes = 0;
				}

				*rle->output = PLANAR_CONTROL_BYTE(15, rle->cRawBytes);
				rle->output++;

				if (rle->cRawBytes)
				{
					CopyMemory(rle->output, rle->rawValues, rle->cRawBytes);
					rle->rawValues += (rle->cRawBytes + rle->nRunLength);
					rle->output += rle->cRawBytes;
				}

				rle->nRunLength -= 15;
				rle->cRawBytes = 0;

				/* continue */
			}
			else
			{
				rle->nControlBytes = 1;
				rle->outSegmentSize = rle->cRawBytes + rle->nControlBytes;

				if (((rle->output - rle->outPlane) + rle->outSegmentSize) > rle->outPlaneSize)
				{
					printf("overflow: %d > %d\n", ((rle->output - rle->outPlane) + rle->outSegmentSize), rle->outPlaneSize);
					return -1;
				}

				if ((!rle->rawValues[rle->cRawBytes - 1]) && (rle->cRawBytes == 1))
				{
					//rle->rawValues += (rle->cRawBytes + rle->nRunLength);
					//rle->cRawBytes = 0;
				}

				*rle->output = PLANAR_CONTROL_BYTE(rle->nRunLength, rle->cRawBytes);
				rle->output++;

				if (rle->cRawBytes)
				{
					CopyMemory(rle->output, rle->rawValues, rle->cRawBytes);
					rle->rawValues += (rle->cRawBytes + rle->nRunLength);
					rle->output += rle->cRawBytes;
				}

				rle->cRawBytes = 0;
				rle->nRunLength = 0;

				return 0; /* finish */
			}
		}
	}

	return 0;
}

BYTE* freerdp_bitmap_planar_compress_plane_rle(BYTE* inPlane, int width, int height, BYTE* outPlane, int* dstSize)
{
	int i, j;
	int bSymbolMatch;
	int bSequenceEnd;
	PLANAR_RLE_CONTEXT rle_s;
	PLANAR_RLE_CONTEXT* rle = &rle_s;

	if (!outPlane)
	{
		rle->outPlaneSize = width * height * 2;
		outPlane = malloc(rle->outPlaneSize);
	}
	else
	{
		rle->outPlaneSize = *dstSize;
	}

	rle->output = outPlane;
	rle->outPlane = outPlane;
	rle->width = width;
	rle->height = height;

	for (i = 0; i < height; i++)
	{
		rle->nRunLength = 0;
		rle->cRawBytes = 1;

		rle->rawScanline = &inPlane[i * width];
		rle->rawValues = rle->rawScanline;

		for (j = 1; j < width; j++)
		{
			bSymbolMatch = FALSE;
			bSequenceEnd = ((j + 1) == width) ? TRUE : FALSE;

			if (rle->rawScanline[j] == rle->rawValues[rle->cRawBytes - 1])
			{
				bSymbolMatch = TRUE;
			}
			else
			{
				//printf("mismatch: nRunLength: %d cRawBytes: %d\n", rle->nRunLength, rle->cRawBytes);

				if (bSequenceEnd)
					rle->cRawBytes++;

				if (rle->nRunLength < 3)
				{
					rle->cRawBytes += rle->nRunLength;
					rle->nRunLength = 0;
				}
				else
				{
					bSequenceEnd = TRUE;
				}
			}

			if (bSymbolMatch)
			{
				rle->nRunLength++;
			}

			//printf("j: %d [0x%02X] cRawBytes: %d nRunLength: %d bSymbolMatch: %d bSequenceEnd: %d\n",
			//		j, rle->rawScanline[j], rle->cRawBytes, rle->nRunLength, bSymbolMatch, bSequenceEnd);

			if (bSequenceEnd)
			{
				if (rle->nRunLength < 3)
				{
					rle->cRawBytes += rle->nRunLength;
					rle->nRunLength = 0;
				}

				if (freerdp_bitmap_planar_compress_plane_rle_segment(rle) < 0)
					return NULL;

				rle->nRunLength = 0;
				rle->cRawBytes = 1;
			}
			else
			{
				if (!bSymbolMatch)
				{
					rle->cRawBytes++;
				}
			}
		}
	}

	*dstSize = (rle->output - outPlane);

	return outPlane;
}

int freerdp_bitmap_planar_compress_planes_rle(BYTE* inPlanes[5], int width, int height, BYTE* outPlanes, int* dstSizes)
{
	int outPlanesSize = width * height * 4;

#if 0
	dstSizes[0] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[0], width, height, outPlanes, &dstSizes[0]))
		return 0;

	outPlanes += dstSizes[0];
	outPlanesSize -= dstSizes[0];
#else
	dstSizes[0] = 0;
#endif
	dstSizes[1] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[1], width, height, outPlanes, &dstSizes[1]))
		return 0;

	outPlanes += dstSizes[1];
	outPlanesSize -= dstSizes[1];
	dstSizes[2] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[2], width, height, outPlanes, &dstSizes[2]))
		return 0;

	outPlanes += dstSizes[2];
	outPlanesSize -= dstSizes[2];
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
	BYTE u2c;
	int delta;
	int i, j, k;

	if (!outPlane)
	{
		outPlane = (BYTE*) malloc(width * height);
	}

	k = 0;

	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
		{
			if (i < 1)
			{
				delta = inPlane[j];

				s2c = (delta >= 0) ? (char) delta : (char) (~((BYTE) (delta * -1)) + 1);
			}
			else
			{
				delta = inPlane[(i * width) + j] - inPlane[((i - 1) * width) + j];

				s2c = (delta >= 0) ? (char) delta : (char) (~((BYTE) (delta * -1)) + 1);

				s2c = (s2c >= 0) ? (s2c << 1) : (char) (((~((BYTE) s2c) + 1) << 1) - 1);
			}

			u2c = (BYTE) s2c;

			outPlane[(i * width) + j] = u2c;

			k++;
		}
	}

	return outPlane;
}

int freerdp_bitmap_planar_delta_encode_planes(BYTE* inPlanes[5], int width, int height, BYTE* outPlanes[5])
{
	freerdp_bitmap_planar_delta_encode_plane(inPlanes[0], width, height, outPlanes[0]);
	freerdp_bitmap_planar_delta_encode_plane(inPlanes[1], width, height, outPlanes[1]);
	freerdp_bitmap_planar_delta_encode_plane(inPlanes[2], width, height, outPlanes[2]);
	freerdp_bitmap_planar_delta_encode_plane(inPlanes[3], width, height, outPlanes[3]);

	return 0;
}

//static int g_Count = 0;

BYTE* freerdp_bitmap_compress_planar(BYTE* data, UINT32 format, int width, int height, int scanline, BYTE* dstData, int* dstSize)
{
	int size;
	BYTE* dstp;
	int planeSize;
	int dstSizes[4];
	BYTE* planes[5];
	BYTE* planesBuffer;
	BYTE* deltaPlanes[5];
	BYTE* deltaPlanesBuffer;
	BYTE* rlePlanes[4];
	BYTE* rlePlanesBuffer;
	BYTE FormatHeader = 0;

	FormatHeader |= PLANAR_FORMAT_HEADER_NA;

	planeSize = width * height;

	planesBuffer = malloc(planeSize * 5);
	planes[0] = &planesBuffer[planeSize * 0];
	planes[1] = &planesBuffer[planeSize * 1];
	planes[2] = &planesBuffer[planeSize * 2];
	planes[3] = &planesBuffer[planeSize * 3];
	planes[4] = &planesBuffer[planeSize * 4];

	deltaPlanesBuffer = malloc(planeSize * 5);
	deltaPlanes[0] = &deltaPlanesBuffer[planeSize * 0];
	deltaPlanes[1] = &deltaPlanesBuffer[planeSize * 1];
	deltaPlanes[2] = &deltaPlanesBuffer[planeSize * 2];
	deltaPlanes[3] = &deltaPlanesBuffer[planeSize * 3];
	deltaPlanes[4] = &deltaPlanesBuffer[planeSize * 4];

	rlePlanesBuffer = malloc(planeSize * 4);

	freerdp_split_color_planes(data, format, width, height, scanline, planes);

	freerdp_bitmap_planar_delta_encode_planes(planes, width, height, deltaPlanes);

#if 0
	{
		g_Count++;
		BYTE* bitmap;
		int bitmapLength;

		bitmapLength = width * height * 4;
		bitmap = malloc(width * height * 4);
		ZeroMemory(bitmap, bitmapLength);

		freerdp_bitmap_planar_decompress_plane_raw(planes[0], width, height, bitmap + 3, planeSize); /* A */
		freerdp_bitmap_planar_decompress_plane_raw(planes[1], width, height, bitmap + 2, planeSize); /* R */
		freerdp_bitmap_planar_decompress_plane_raw(planes[2], width, height, bitmap + 1, planeSize); /* G */
		freerdp_bitmap_planar_decompress_plane_raw(planes[3], width, height, bitmap + 0, planeSize); /* B */

		printf("Bitmap %02d\n", g_Count);
		winpr_CArrayDump(bitmap, bitmapLength, 32);

		free(bitmap);
	}
#endif

	if (freerdp_bitmap_planar_compress_planes_rle(deltaPlanes, width, height, rlePlanesBuffer, (int*) &dstSizes) > 0)
	{
		int offset = 0;

		FormatHeader |= PLANAR_FORMAT_HEADER_RLE;

		rlePlanes[0] = &rlePlanesBuffer[offset];
		offset += dstSizes[0];

		rlePlanes[1] = &rlePlanesBuffer[offset];
		offset += dstSizes[1];

		rlePlanes[2] = &rlePlanesBuffer[offset];
		offset += dstSizes[2];

		rlePlanes[3] = &rlePlanesBuffer[offset];
		offset += dstSizes[3];

		printf("R: [%d/%d] G: [%d/%d] B: [%d/%d]\n",
				dstSizes[1], planeSize, dstSizes[2], planeSize, dstSizes[3], planeSize);
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
			CopyMemory(dstp, rlePlanes[0], dstSizes[0]); /* Alpha */
			dstp += dstSizes[0];
		}
		else
		{
			CopyMemory(dstp, planes[0], planeSize); /* Alpha */
			dstp += planeSize;
		}
	}

	/* LumaOrRedPlane */

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		CopyMemory(dstp, rlePlanes[1], dstSizes[1]); /* Red */
		dstp += dstSizes[1];
	}
	else
	{
		CopyMemory(dstp, planes[1], planeSize); /* Red */
		dstp += planeSize;
	}

	/* OrangeChromaOrGreenPlane */

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		CopyMemory(dstp, rlePlanes[2], dstSizes[2]); /* Green */
		dstp += dstSizes[2];
	}
	else
	{
		CopyMemory(dstp, planes[2], planeSize); /* Green */
		dstp += planeSize;
	}

	/* GreenChromeOrBluePlane */

	if (FormatHeader & PLANAR_FORMAT_HEADER_RLE)
	{
		CopyMemory(dstp, rlePlanes[3], dstSizes[3]); /* Blue */
		dstp += dstSizes[3];
	}
	else
	{
		CopyMemory(dstp, planes[3], planeSize); /* Blue */
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

	free(rlePlanesBuffer);
	free(deltaPlanesBuffer);
	free(planesBuffer);

	return dstData;
}
