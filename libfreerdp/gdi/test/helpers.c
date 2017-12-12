/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Library Tests
 *
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "helpers.h"

HGDI_BITMAP test_convert_to_bitmap(const BYTE* src, UINT32 SrcFormat,
                                   UINT32 SrcStride,
                                   UINT32 xSrc, UINT32 ySrc, UINT32 DstFormat, UINT32 DstStride,
                                   UINT32 xDst, UINT32 yDst, UINT32 nWidth, UINT32 nHeight,
                                   const gdiPalette* hPalette)
{
	HGDI_BITMAP bmp;
	BYTE* data;

	if (DstStride == 0)
		DstStride = nWidth * GetBytesPerPixel(DstFormat);

	data = _aligned_malloc(DstStride * nHeight, 16);

	if (!data)
		return NULL;

	if (!freerdp_image_copy(data, DstFormat, DstStride, xDst, yDst, nWidth, nHeight,
	                        src, SrcFormat, SrcStride, xSrc, ySrc, hPalette, FREERDP_FLIP_NONE))
	{
		_aligned_free(data);
		return NULL;
	}

	bmp = gdi_CreateBitmap(nWidth, nHeight, DstFormat, data);

	if (!bmp)
	{
		_aligned_free(data);
		return NULL;
	}

	return bmp;
}


static void test_dump_data(unsigned char* p, int len, int width,
                           const char* name)
{
	unsigned char* line = p;
	int i, thisline, offset = 0;
	printf("\n%s[%d][%d]:\n", name, len / width, width);

	while (offset < len)
	{
		printf("%04x ", offset);
		thisline = len - offset;

		if (thisline > width)
			thisline = width;

		for (i = 0; i < thisline; i++)
			printf("%02x ", line[i]);

		for (; i < width; i++)
			printf("   ");

		printf("\n");
		offset += thisline;
		line += thisline;
	}

	printf("\n");
	fflush(stdout);
}

void test_dump_bitmap(HGDI_BITMAP hBmp, const char* name)
{
	UINT32 stride = hBmp->width * GetBytesPerPixel(hBmp->format);
	test_dump_data(hBmp->data, hBmp->height * stride, stride, name);
}

static BOOL CompareBitmaps(HGDI_BITMAP hBmp1, HGDI_BITMAP hBmp2,
                           const gdiPalette* palette)
{
	UINT32 x, y;
	const BYTE* p1 = hBmp1->data;
	const BYTE* p2 = hBmp2->data;
	UINT32 colorA, colorB;
	UINT32 minw = (hBmp1->width < hBmp2->width) ? hBmp1->width : hBmp2->width;
	UINT32 minh = (hBmp1->height < hBmp2->height) ? hBmp1->height : hBmp2->height;

	for (y = 0; y < minh; y++)
	{
		for (x = 0; x < minw; x++)
		{
			colorA = ReadColor(p1, hBmp1->format);
			colorB = ReadColor(p2, hBmp2->format);
			p1 += GetBytesPerPixel(hBmp1->format);
			p2 += GetBytesPerPixel(hBmp2->format);

			if (hBmp1->format != hBmp2->format)
				colorB = FreeRDPConvertColor(colorB, hBmp2->format, hBmp1->format, palette);

			if (colorA != colorB)
				return FALSE;
		}
	}

	return TRUE;
}

BOOL test_assert_bitmaps_equal(HGDI_BITMAP hBmpActual,
                               HGDI_BITMAP hBmpExpected,
                               const char* name,
                               const gdiPalette* palette)
{
	BOOL bitmapsEqual = CompareBitmaps(hBmpActual, hBmpExpected, palette);

	if (!bitmapsEqual)
	{
		printf("Testing ROP %s [%s|%s]\n", name,
		       FreeRDPGetColorFormatName(hBmpActual->format),
		       FreeRDPGetColorFormatName(hBmpExpected->format));
		test_dump_bitmap(hBmpActual, "Actual");
		test_dump_bitmap(hBmpExpected, "Expected");
		fflush(stdout);
		fflush(stderr);
	}

	return bitmapsEqual;
}
