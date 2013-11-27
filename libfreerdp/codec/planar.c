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
				GetARGB32(planes[0][k], planes[0][k], planes[2][k], planes[3][k], *pixel);
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
	int i, j;
	BYTE* dstp;
	BYTE symbol;
	int cSegments;
	int nRunLength;
	int cRawBytes;
	BYTE* rawValues;
	int outPlaneSize;
	int outSegmentSize;
	int nControlBytes;

	cSegments = 0;
	outPlaneSize = width * height;

	if (!outPlane)
		outPlane = malloc(outPlaneSize);

	dstp = outPlane;

	for (i = 0; i < height; i++)
	{
		cRawBytes = 0;
		nRunLength = -1;
		rawValues = &inPlane[i * width];

		for (j = 0; j <= width; j++)
		{
			if ((nRunLength < 0) && (j != width))
			{
				symbol = inPlane[(i * width) + j];
				nRunLength = 0;
				continue;
			}

			if ((j != width) && (inPlane[(i * width) + j] == symbol))
			{
				nRunLength++;
			}
			else
			{
				if (nRunLength >= 3)
				{
					cRawBytes += 1;

#if 0
					printf("RAW[");

					for (k = 0; k < cRawBytes; k++)
					{
						printf("0x%02X%s", rawValues[k],
								((k + 1) == cRawBytes) ? "" : ", ");
					}

					printf("] RUN[%d]\n", nRunLength);
#endif

					while (cRawBytes > 15)
					{
						printf("handling cRawBytes > 15\n");

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

						return NULL;
					}

					if (nRunLength > 47)
					{
						nControlBytes = (((nRunLength - 15) + 46) / 47) + 1;

						outSegmentSize = cRawBytes + nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(15, cRawBytes);
						nRunLength -= 15;
						nControlBytes--;
						dstp++;

						CopyMemory(dstp, rawValues, cRawBytes);
						dstp += cRawBytes;

						while (nControlBytes--)
						{
							if (nRunLength > 47)
							{
								*dstp = PLANAR_CONTROL_BYTE(2, (47 - 32));
								nRunLength -= 47;
								dstp++;
							}
							else if (nRunLength > 31)
							{
								*dstp = PLANAR_CONTROL_BYTE(2, (nRunLength - 32));
								nRunLength = 0;
								dstp++;
							}
							else if (nRunLength > 15)
							{
								*dstp = PLANAR_CONTROL_BYTE(1, (nRunLength - 16));
								nRunLength = 0;
								dstp++;
							}
							else
							{
								*dstp = PLANAR_CONTROL_BYTE(0, nRunLength);
								nRunLength = 0;
								dstp++;
							}
						}

						return NULL;
					}
					else if (nRunLength > 31)
					{
						nControlBytes = 2;
						outSegmentSize = cRawBytes + nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(15, cRawBytes);
						dstp++;

						CopyMemory(dstp, rawValues, cRawBytes);
						dstp += cRawBytes;

						nRunLength -= 32;
						*dstp = PLANAR_CONTROL_BYTE(2, nRunLength);
						dstp++;
					}
					else if (nRunLength > 15)
					{
						nControlBytes = 2;
						outSegmentSize = cRawBytes + nControlBytes;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(15, cRawBytes);
						dstp++;

						CopyMemory(dstp, rawValues, cRawBytes);
						dstp += cRawBytes;

						nRunLength -= 16;
						*dstp = PLANAR_CONTROL_BYTE(1, nRunLength);
						dstp++;
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
						dstp += cRawBytes;
					}

					rawValues = &inPlane[(i * width) + j];
					cRawBytes = 0;
				}
				else
				{
					cRawBytes += (nRunLength + 1);

					if (j == width)
					{
						nRunLength = 0;

						if (cRawBytes > 15)
						{
							printf("cRawBytes > 15 unhandled\n");
							return NULL;
						}

#if 0
						printf("RAW[");

						for (k = 0; k < cRawBytes; k++)
						{
							printf("0x%02X%s", rawValues[k],
									((k + 1) == cRawBytes) ? "" : ", ");
						}

						printf("] RUN[%d]\n", nRunLength);
#endif

						outSegmentSize = cRawBytes + 1;

						if (((dstp - outPlane) + outSegmentSize) > outPlaneSize)
						{
							printf("overflow: %d > %d\n", ((dstp - outPlane) + outSegmentSize), outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(nRunLength, cRawBytes);
						dstp++;

						CopyMemory(dstp, rawValues, cRawBytes);
						dstp += cRawBytes;
					}
				}

				if (j != width)
				{
					symbol = inPlane[(i * width) + j];
					nRunLength = 0;
				}
			}
		}

		//printf("---\n");
	}

	//printf("\n");

	*dstSize = (dstp - outPlane);

	return outPlane;
}

int freerdp_bitmap_planar_compress_planes_rle(BYTE* inPlanes[5], int width, int height, BYTE* outPlanes[5], int* dstSizes)
{
	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[0], width, height, outPlanes[0], &dstSizes[0]))
		return 0;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[1], width, height, outPlanes[1], &dstSizes[1]))
		return 0;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[2], width, height, outPlanes[2], &dstSizes[2]))
		return 0;

	if (!freerdp_bitmap_planar_compress_plane_rle(inPlanes[3], width, height, outPlanes[3], &dstSizes[3]))
		return 0;

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
	int dstSizes[5];
	BYTE* planes[5];
	BYTE* planesBuffer;
	BYTE* deltaPlanes[5];
	BYTE* deltaPlanesBuffer;
	BYTE* rlePlanes[5];
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

	rlePlanesBuffer = malloc(planeSize * 5);
	rlePlanes[0] = &rlePlanesBuffer[planeSize * 0];
	rlePlanes[1] = &rlePlanesBuffer[planeSize * 1];
	rlePlanes[2] = &rlePlanesBuffer[planeSize * 2];
	rlePlanes[3] = &rlePlanesBuffer[planeSize * 3];
	rlePlanes[4] = &rlePlanesBuffer[planeSize * 4];

	freerdp_split_color_planes(data, format, width, height, scanline, planes);

	freerdp_bitmap_planar_delta_encode_planes(planes, width, height, deltaPlanes);

	if (freerdp_bitmap_planar_compress_planes_rle(deltaPlanes, width, height, rlePlanes, (int*) &dstSizes) > 0)
	{
		FormatHeader |= PLANAR_FORMAT_HEADER_RLE;

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
