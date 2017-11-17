/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Shape Functions
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/shape.h>

#include <freerdp/log.h>

#include "clipping.h"
#include "../gdi/gdi.h"

#define TAG FREERDP_TAG("gdi.shape")

static void Ellipse_Bresenham(HGDI_DC hdc, int x1, int y1, int x2, int y2)
{
	INT32 e, e2;
	INT32 dx, dy;
	INT32 a, b, c;
	a = (x1 < x2) ? x2 - x1 : x1 - x2;
	b = (y1 < y2) ? y2 - y1 : y1 - y2;
	c = b & 1;
	dx = 4 * (1 - a) * b * b;
	dy = 4 * (c + 1) * a * a;
	e = dx + dy + c * a * a;

	if (x1 > x2)
	{
		x1 = x2;
		x2 += a;
	}

	if (y1 > y2)
		y1 = y2;

	y1 += (b + 1) / 2;
	y2 = y1 - c;
	a *= 8 * a;
	c = 8 * b * b;

	do
	{
		gdi_SetPixel(hdc, x2, y1, 0);
		gdi_SetPixel(hdc, x1, y1, 0);
		gdi_SetPixel(hdc, x1, y2, 0);
		gdi_SetPixel(hdc, x2, y2, 0);
		e2 = 2 * e;

		if (e2 >= dx)
		{
			x1++;
			x2--;
			e += dx += c;
		}

		if (e2 <= dy)
		{
			y1++;
			y2--;
			e += dy += a;
		}
	}
	while (x1 <= x2);

	while (y1 - y2 < b)
	{
		gdi_SetPixel(hdc, x1 - 1, ++y1, 0);
		gdi_SetPixel(hdc, x1 - 1, --y2, 0);
	}
}

/**
 * Draw an ellipse
 * @msdn{dd162510}
 * @param hdc device context
 * @param nLeftRect x1
 * @param nTopRect y1
 * @param nRightRect x2
 * @param nBottomRect y2
 * @return nonzero if successful, 0 otherwise
 */
BOOL gdi_Ellipse(HGDI_DC hdc, int nLeftRect, int nTopRect, int nRightRect,
                 int nBottomRect)
{
	Ellipse_Bresenham(hdc, nLeftRect, nTopRect, nRightRect, nBottomRect);
	return TRUE;
}

/**
 * Fill a rectangle with the given brush.\n
 * @msdn{dd162719}
 * @param hdc device context
 * @param rect rectangle
 * @param hbr brush
 * @return nonzero if successful, 0 otherwise
 */

BOOL gdi_FillRect(HGDI_DC hdc, const HGDI_RECT rect, HGDI_BRUSH hbr)
{
	UINT32 x, y;
	UINT32 color, dstColor;
	BOOL monochrome = FALSE;
	UINT32 nXDest, nYDest;
	UINT32 nWidth, nHeight;
	const BYTE* srcp;
	DWORD formatSize;
	gdi_RectToCRgn(rect, &nXDest, &nYDest, &nWidth, &nHeight);

	if (!hdc || !hbr)
		return FALSE;

	if (!gdi_ClipCoords(hdc, &nXDest, &nYDest, &nWidth, &nHeight, NULL, NULL))
		return TRUE;

	switch (hbr->style)
	{
		case GDI_BS_SOLID:
			color = hbr->color;

			for (x = 0; x < nWidth; x++)
			{
				BYTE* dstp = gdi_get_bitmap_pointer(hdc, nXDest + x,
				                                    nYDest);

				if (dstp)
					WriteColor(dstp, hdc->format, color);
			}

			srcp = gdi_get_bitmap_pointer(hdc, nXDest, nYDest);
			formatSize = GetBytesPerPixel(hdc->format);

			for (y = 1; y < nHeight; y++)
			{
				BYTE* dstp = gdi_get_bitmap_pointer(hdc, nXDest, nYDest + y);
				memcpy(dstp, srcp, nWidth * formatSize);
			}

			break;

		case GDI_BS_HATCHED:
		case GDI_BS_PATTERN:
			monochrome = (hbr->pattern->format == PIXEL_FORMAT_MONO);
			formatSize = GetBytesPerPixel(hbr->pattern->format);

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const UINT32 yOffset = ((nYDest + y) * hbr->pattern->width %
					                        hbr->pattern->height) * formatSize;
					const UINT32 xOffset = ((nXDest + x) % hbr->pattern->width) * formatSize;
					const BYTE* patp = &hbr->pattern->data[yOffset + xOffset];
					BYTE* dstp = gdi_get_bitmap_pointer(hdc, nXDest + x,
					                                    nYDest + y);

					if (!patp)
						return FALSE;

					if (monochrome)
					{
						if (*patp == 0)
							dstColor = hdc->bkColor;
						else
							dstColor = hdc->textColor;
					}
					else
					{
						dstColor = ReadColor(patp, hbr->pattern->format);
						dstColor = ConvertColor(dstColor, hbr->pattern->format, hdc->format, NULL);
					}

					if (dstp)
						WriteColor(dstp, hdc->format, dstColor);
				}
			}

			break;

		default:
			break;
	}

	if (!gdi_InvalidateRegion(hdc, nXDest, nYDest, nWidth, nHeight))
		return FALSE;

	return TRUE;
}


/**
 * Draw a polygon
 * @msdn{dd162814}
 * @param hdc device context
 * @param lpPoints array of points
 * @param nCount number of points
 * @return nonzero if successful, 0 otherwise
 */
BOOL gdi_Polygon(HGDI_DC hdc, GDI_POINT* lpPoints, int nCount)
{
	WLog_ERR(TAG, "Not implemented!");
	return FALSE;
}

/**
 * Draw a series of closed polygons
 * @msdn{dd162818}
 * @param hdc device context
 * @param lpPoints array of series of points
 * @param lpPolyCounts array of number of points in each series
 * @param nCount count of number of points in lpPolyCounts
 * @return nonzero if successful, 0 otherwise
 */
BOOL gdi_PolyPolygon(HGDI_DC hdc, GDI_POINT* lpPoints, int* lpPolyCounts,
                     int nCount)
{
	WLog_ERR(TAG, "Not implemented!");
	return FALSE;
}

BOOL gdi_Rectangle(HGDI_DC hdc, UINT32 nXDst, UINT32 nYDst, UINT32 nWidth,
                   UINT32 nHeight)
{
	UINT32 x, y;
	UINT32 color;

	if (!gdi_ClipCoords(hdc, &nXDst, &nYDst, &nWidth, &nHeight, NULL, NULL))
		return TRUE;

	color = hdc->textColor;

	for (y = 0; y < nHeight; y++)
	{
		BYTE* dstLeft = gdi_get_bitmap_pointer(hdc, nXDst,
		                                       nYDst + y);
		BYTE* dstRight = gdi_get_bitmap_pointer(hdc, nXDst + nWidth - 1,
		                                        nYDst + y);

		if (dstLeft)
			WriteColor(dstLeft, hdc->format, color);

		if (dstRight)
			WriteColor(dstRight, hdc->format, color);
	}

	for (x = 0; x < nWidth; x++)
	{
		BYTE* dstTop = gdi_get_bitmap_pointer(hdc, nXDst + x,
		                                      nYDst);
		BYTE* dstBottom = gdi_get_bitmap_pointer(hdc, nXDst + x,
		                  nYDst + nHeight - 1);

		if (dstTop)
			WriteColor(dstTop, hdc->format, color);

		if (dstBottom)
			WriteColor(dstBottom, hdc->format, color);
	}

	return FALSE;
}
