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

BYTE* freerdp_bitmap_planar_compress_plane_rle(BYTE* inPlane, int width, int height, BYTE* outPlane, int* dstSize)
{
	int i, j, k;
	BYTE* dstp;
	int cSegments;
	int nRunLength;
	int cRawBytes;
	BYTE* rawValues;
	BYTE* rawScanline;
	int outPlaneSize;
	int outSegmentSize;
	int nControlBytes;

	cSegments = 0;

	if (!outPlane)
	{
		outPlaneSize = width * height * 2;
		outPlane = malloc(outPlaneSize);
	}
	else
	{
		outPlaneSize = *dstSize;
	}

	dstp = outPlane;

	for (i = 0; i < height; i++)
	{
		cRawBytes = 1;
		nRunLength = 0;
		rawScanline = &inPlane[i * width];
		rawValues = rawScanline;

		for (j = 1; j <= width; j++)
		{
			if (j != width)
			{
				if (rawScanline[j] == rawValues[cRawBytes - 1])
				{
					nRunLength++;
					continue;
				}
			}

			if (nRunLength < 3)
			{
				if (j != width)
				{
					cRawBytes++;
					continue;
				}
				else
				{
					cRawBytes += nRunLength;
					nRunLength = 0;
				}
			}

			{
#if 0
				printf("scanline %d nRunLength: %d cRawBytes: %d\n", i, nRunLength, cRawBytes);

				printf("RAW[");

				for (k = 0; k < cRawBytes; k++)
				{
					printf("0x%02X%s", rawValues[k],
							((k + 1) == cRawBytes) ? "" : ", ");
				}

				printf("] RUN[%d]\n", nRunLength);
#endif

				if ((rawValues - rawScanline) > width)
				{
					printf("rawValues overflow! %d\n", rawValues - rawScanline);
					return NULL;
				}

				while (cRawBytes > 15)
				{
					nControlBytes = 1;
					outSegmentSize = 15 + nControlBytes;

					if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
					{
						printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
						return NULL;
					}

					*dstp = PLANAR_CONTROL_BYTE(0, 15);
					dstp++;

					CopyMemory(dstp, rawValues, 15);
					cRawBytes -= 15;
					rawValues += 15;
					dstp += 15;
				}

				if (cRawBytes == 0)
				{
					if (nRunLength > 47)
					{
						nControlBytes = 1;
						outSegmentSize = nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(2, 15);
						nRunLength -= 47;
						dstp++;

						return NULL;

						//if (j != width)
						{
							j--;
							continue;
						}
					}
					else if (nRunLength > 31)
					{
						nControlBytes = 1;
						outSegmentSize = nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(2, (nRunLength - 32));
						nRunLength -= 32;
						dstp++;

						//if (j != width)
						{
							j--;
							continue;
						}
					}
					else if (nRunLength > 15)
					{
						nControlBytes = 1;
						outSegmentSize = nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(1, (nRunLength - 16));
						nRunLength -= 16;
						dstp++;

						//if (j != width)
						{
							j--;
							continue;
						}
					}
					else
					{
						nControlBytes = 1;
						outSegmentSize = cRawBytes + nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(nRunLength, cRawBytes);
						dstp++;

						CopyMemory(dstp, rawValues, cRawBytes);
						rawValues += (cRawBytes + nRunLength);
						dstp += cRawBytes;
						cRawBytes = 0;

						nRunLength = 0;

						if (j != width)
						{
							j--;
							continue;
						}
					}
				}
				else if (cRawBytes < 16)
				{
					if (nRunLength > 15)
					{
						nControlBytes = 2;
						outSegmentSize = cRawBytes + nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						if ((!rawValues[cRawBytes - 1]) && (cRawBytes == 1))
						{
							rawValues += (cRawBytes + nRunLength);
							cRawBytes = 0;
						}

						*dstp = PLANAR_CONTROL_BYTE(15, cRawBytes);
						dstp++;

						if (cRawBytes)
						{
							CopyMemory(dstp, rawValues, cRawBytes);
							rawValues += (cRawBytes + nRunLength);
							dstp += cRawBytes;
							cRawBytes = 0;
						}

						nRunLength -= 15;

						{
							cRawBytes = 1;
							j--;
							continue;
						}
					}
					else
					{
						nControlBytes = 1;
						outSegmentSize = cRawBytes + nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(nRunLength, cRawBytes);
						dstp++;

						CopyMemory(dstp, rawValues, cRawBytes);
						rawValues += (cRawBytes + nRunLength);
						dstp += cRawBytes;
						cRawBytes = 0;

						nRunLength = 0;

						if (j != width)
						{
							j--;
							continue;
						}
					}
				}
			}
		}
	}

	*dstSize = (dstp - outPlane);

	return outPlane;
}

int freerdp_bitmap_planar_compress_planes_rle(BYTE* inPlanes[5], int width, int height, BYTE* outPlanes, int* dstSizes)
{
	int outPlanesSize = width * height * 4;

	dstSizes[0] = outPlanesSize;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[0], width, height, outPlanes, &dstSizes[0]))
		return 0;

	outPlanes += dstSizes[0];
	outPlanesSize -= dstSizes[0];
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
