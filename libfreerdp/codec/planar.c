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

BYTE* freerdp_bitmap_compress_planar(BYTE* data, UINT32 format, int width, int height, int scanline, BYTE* dstData, int* dstSize)
{
	int size;
	BYTE* dstp;
	int planeSize;
	BYTE* planes[4];
	BYTE FormatHeader;
	BYTE* planesBuffer;

	FormatHeader = 0;
	FormatHeader |= PLANAR_FORMAT_HEADER_NA;

	planeSize = width * height;
	planesBuffer = malloc(planeSize * 4);
	planes[0] = &planesBuffer[planeSize * 0];
	planes[1] = &planesBuffer[planeSize * 1];
	planes[2] = &planesBuffer[planeSize * 2];
	planes[3] = &planesBuffer[planeSize * 3];

	freerdp_split_color_planes(data, format, width, height, scanline, planes);

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

	free(planesBuffer);

	return dstData;
}
