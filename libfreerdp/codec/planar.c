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

BYTE* freerdp_bitmap_planar_compress_plane_rle(BYTE* inPlane, int width, int height, BYTE* outPlane)
{
	BYTE* dstp;
	BYTE* segp;
	BYTE symbol;
	int i, j, k;
	int cSegments;
	int nRunLength;
	int cRawBytes;
	BYTE* rawValues;
	int outPlaneSize;
	int nSequenceLength;

	cSegments = 0;
	outPlaneSize = width * height;

	if (!outPlane)
		outPlane = malloc(outPlaneSize);

	dstp = outPlane;

	for (i = 0; i < height; i++)
	{
		cRawBytes = 0;
		nRunLength = 0;
		nSequenceLength = 0;
		rawValues = &inPlane[i * width];

		for (j = 0; j <= width; j++)
		{
			if ((!nSequenceLength) && (j != width))
			{
				symbol = inPlane[(i * width) + j];
				nSequenceLength = 1;
			}
			else
			{
				if ((j != width) && (inPlane[(i * width) + j] == symbol))
				{
					nSequenceLength++;
				}
				else
				{
					if (nSequenceLength > 3)
					{
						cRawBytes += 1;
						nRunLength = nSequenceLength - 1;

						printf("RAW[");

						for (k = 0; k < cRawBytes; k++)
						{
							printf("0x%02X%s", rawValues[k],
									((k + 1) == cRawBytes) ? "" : ", ");
						}

						printf("] RUN[%d]\n", nRunLength);

						segp = dstp;

						if (((segp - outPlane) + cRawBytes + 1) > outPlaneSize)
						{
							printf("overflow: %d > %d\n",
									((dstp - outPlane) + cRawBytes + 1),
									outPlaneSize);
							return NULL;
						}

						*dstp = PLANAR_CONTROL_BYTE(nRunLength, cRawBytes);
						dstp++;

						CopyMemory(dstp, rawValues, cRawBytes);
						dstp += cRawBytes;

						printf("Segment %d\n", ++cSegments);
						winpr_HexDump(segp, dstp - segp);

						rawValues = &inPlane[(i * width) + j];
						cRawBytes = 0;
					}
					else
					{
						cRawBytes += nSequenceLength;

						if (j == width)
						{
							nRunLength = 0;

							printf("RAW[");

							for (k = 0; k < cRawBytes; k++)
							{
								printf("0x%02X%s", rawValues[k],
										((k + 1) == cRawBytes) ? "" : ", ");
							}

							printf("] RUN[%d]\n", nRunLength);

							segp = dstp;

							if (((segp - outPlane) + cRawBytes + 1) > outPlaneSize)
							{
								printf("overflow: %d > %d\n",
										((dstp - outPlane) + cRawBytes + 1),
										outPlaneSize);
								return NULL;
							}

							*dstp = PLANAR_CONTROL_BYTE(nRunLength, cRawBytes);
							dstp++;

							CopyMemory(dstp, rawValues, cRawBytes);
							dstp += cRawBytes;

							printf("Segment %d\n", ++cSegments);
							winpr_HexDump(segp, dstp - segp);
						}
					}

					if (j != width)
					{
						symbol = inPlane[(i * width) + j];
						nSequenceLength = 1;
					}
				}
			}
		}

		printf("---\n");
	}

	printf("\n");

	return outPlane;
}

int freerdp_bitmap_planar_compress_scanlines_rle(BYTE* inPlanes[5], int width, int height, BYTE* outPlanes[5])
{
	freerdp_bitmap_planar_compress_plane_rle(inPlanes[0], width, height, outPlanes[0]);
	freerdp_bitmap_planar_compress_plane_rle(inPlanes[1], width, height, outPlanes[1]);
	freerdp_bitmap_planar_compress_plane_rle(inPlanes[2], width, height, outPlanes[2]);
	freerdp_bitmap_planar_compress_plane_rle(inPlanes[3], width, height, outPlanes[3]);

	return 0;
}

BYTE* freerdp_bitmap_planar_delta_encode_scanlines(BYTE* inPlane, int width, int height, BYTE* outPlane)
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
	freerdp_bitmap_planar_delta_encode_scanlines(inPlanes[0], width, height, outPlanes[0]);
	freerdp_bitmap_planar_delta_encode_scanlines(inPlanes[1], width, height, outPlanes[1]);
	freerdp_bitmap_planar_delta_encode_scanlines(inPlanes[2], width, height, outPlanes[2]);
	freerdp_bitmap_planar_delta_encode_scanlines(inPlanes[3], width, height, outPlanes[3]);

	return 0;
}

BYTE* freerdp_bitmap_compress_planar(BYTE* data, UINT32 format, int width, int height, int scanline, BYTE* dstData, int* dstSize)
{
	int size;
	BYTE* dstp;
	int planeSize;
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

	freerdp_bitmap_planar_compress_scanlines_rle(deltaPlanes, width, height, rlePlanes);

	if (!dstData)
	{
		size = 2;

		if (!(FormatHeader & PLANAR_FORMAT_HEADER_NA))
			size += planeSize;

		size += (planeSize * 3);

		dstData = malloc(size);
		*dstSize = size;
	}

	dstp = dstData;

	*dstp = FormatHeader; /* FormatHeader */
	dstp++;

	/* AlphaPlane */

	if (!(FormatHeader & PLANAR_FORMAT_HEADER_NA))
	{
		CopyMemory(dstp, planes[0], planeSize); /* Alpha */
		dstp += planeSize;
	}

	/* LumaOrRedPlane */

	CopyMemory(dstp, planes[1], planeSize); /* Red */
	dstp += planeSize;

	/* OrangeChromaOrGreenPlane */

	CopyMemory(dstp, planes[2], planeSize); /* Green */
	dstp += planeSize;

	/* GreenChromeOrBluePlane */

	CopyMemory(dstp, planes[3], planeSize); /* Blue */
	dstp += planeSize;

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
