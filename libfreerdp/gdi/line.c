/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Line Functions
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
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/region.h>

#include "drawing.h"
#include "clipping.h"
#include "line.h"

/**
 * Draw a line from the current position to the given position.\n
 * @msdn{dd145029}
 * @param hdc device context
 * @param nXEnd ending x position
 * @param nYEnd ending y position
 * @return nonzero if successful, 0 otherwise
 */
static BOOL gdi_rop_color(UINT32 rop, BYTE* pixelPtr, UINT32 pen, UINT32 format)
{
	const UINT32 srcPixel = ReadColor(pixelPtr, format);
	UINT32 dstPixel;

	switch (rop)
	{
		case GDI_R2_BLACK: /* LineTo_BLACK */
			dstPixel = FreeRDPGetColor(format, 0, 0, 0, 0xFF);
			break;

		case GDI_R2_NOTMERGEPEN: /* LineTo_NOTMERGEPEN */
			dstPixel = ~(srcPixel | pen);
			break;

		case GDI_R2_MASKNOTPEN: /* LineTo_MASKNOTPEN */
			dstPixel = srcPixel & ~pen;
			break;

		case GDI_R2_NOTCOPYPEN: /* LineTo_NOTCOPYPEN */
			dstPixel = ~pen;
			break;

		case GDI_R2_MASKPENNOT: /* LineTo_MASKPENNOT */
			dstPixel = pen & ~srcPixel;
			break;

		case GDI_R2_NOT: /* LineTo_NOT */
			dstPixel = ~srcPixel;
			break;

		case GDI_R2_XORPEN: /* LineTo_XORPEN */
			dstPixel = srcPixel ^ pen;
			break;

		case GDI_R2_NOTMASKPEN: /* LineTo_NOTMASKPEN */
			dstPixel = ~(srcPixel & pen);
			break;

		case GDI_R2_MASKPEN: /* LineTo_MASKPEN */
			dstPixel = srcPixel & pen;
			break;

		case GDI_R2_NOTXORPEN: /* LineTo_NOTXORPEN */
			dstPixel = ~(srcPixel ^ pen);
			break;

		case GDI_R2_NOP: /* LineTo_NOP */
			dstPixel = srcPixel;
			break;

		case GDI_R2_MERGENOTPEN: /* LineTo_MERGENOTPEN */
			dstPixel = srcPixel | ~pen;
			break;

		case GDI_R2_COPYPEN: /* LineTo_COPYPEN */
			dstPixel = pen;
			break;

		case GDI_R2_MERGEPENNOT: /* LineTo_MERGEPENNOT */
			dstPixel = srcPixel | ~pen;
			break;

		case GDI_R2_MERGEPEN: /* LineTo_MERGEPEN */
			dstPixel = srcPixel | pen;
			break;

		case GDI_R2_WHITE: /* LineTo_WHITE */
			dstPixel = FreeRDPGetColor(format, 0xFF, 0xFF, 0xFF, 0xFF);
			break;

		default:
			return FALSE;
	}

	return WriteColor(pixelPtr, format, dstPixel);
}

BOOL gdi_LineTo(HGDI_DC hdc, UINT32 nXEnd, UINT32 nYEnd)
{
	INT32 x, y;
	INT32 x1, y1;
	INT32 x2, y2;
	INT32 e, e2;
	INT32 dx, dy;
	INT32 sx, sy;
	INT32 bx1, by1;
	INT32 bx2, by2;
	HGDI_BITMAP bmp;
	UINT32 pen;
	UINT32 rop2 = gdi_GetROP2(hdc);
	x1 = hdc->pen->posX;
	y1 = hdc->pen->posY;
	x2 = nXEnd;
	y2 = nYEnd;
	dx = (x1 > x2) ? x1 - x2 : x2 - x1;
	dy = (y1 > y2) ? y1 - y2 : y2 - y1;
	sx = (x1 < x2) ? 1 : -1;
	sy = (y1 < y2) ? 1 : -1;
	e = dx - dy;
	x = x1;
	y = y1;
	bmp = (HGDI_BITMAP) hdc->selectedObject;

	if (hdc->clip->null)
	{
		bx1 = (x1 < x2) ? x1 : x2;
		by1 = (y1 < y2) ? y1 : y2;
		bx2 = (x1 > x2) ? x1 : x2;
		by2 = (y1 > y2) ? y1 : y2;
	}
	else
	{
		bx1 = hdc->clip->x;
		by1 = hdc->clip->y;
		bx2 = bx1 + hdc->clip->w - 1;
		by2 = by1 + hdc->clip->h - 1;
	}

	bx1 = MAX(bx1, 0);
	by1 = MAX(by1, 0);
	bx2 = MIN(bx2, bmp->width - 1);
	by2 = MIN(by2, bmp->height - 1);

	if (!gdi_InvalidateRegion(hdc, bx1, by1, bx2 - bx1 + 1, by2 - by1 + 1))
		return FALSE;

	pen = gdi_GetPenColor(hdc->pen, bmp->format);

	while (1)
	{
		if (!(x == x2 && y == y2))
		{
			if ((x >= bx1 && x <= bx2) && (y >= by1 && y <= by2))
			{
				BYTE* pixel = gdi_GetPointer(bmp, x, y);
				gdi_rop_color(rop2, pixel, pen, bmp->format);
			}
		}
		else
		{
			break;
		}

		e2 = 2 * e;

		if (e2 > -dy)
		{
			e -= dy;
			x += sx;
		}

		if (e2 < dx)
		{
			e += dx;
			y += sy;
		}
	}

	return TRUE;
}

/**
 * Draw one or more straight lines
 * @param hdc device context
 * @param lppt array of points
 * @param cCount number of points
 * @return nonzero on success, 0 otherwise
 */
BOOL gdi_PolylineTo(HGDI_DC hdc, GDI_POINT* lppt, DWORD cCount)
{
	DWORD i;

	for (i = 0; i < cCount; i++)
	{
		if (!gdi_LineTo(hdc, lppt[i].x, lppt[i].y))
			return FALSE;

		if (!gdi_MoveToEx(hdc, lppt[i].x, lppt[i].y, NULL))
			return FALSE;
	}

	return TRUE;
}

/**
 * Draw one or more straight lines
 * @param hdc device context
 * @param lppt array of points
 * @param cPoints number of points
 * @return nonzero on success, 0 otherwise
 */
BOOL gdi_Polyline(HGDI_DC hdc, GDI_POINT* lppt, UINT32 cPoints)
{
	if (cPoints > 0)
	{
		UINT32 i;
		GDI_POINT pt;

		if (!gdi_MoveToEx(hdc, lppt[0].x, lppt[0].y, &pt))
			return FALSE;

		for (i = 0; i < cPoints; i++)
		{
			if (!gdi_LineTo(hdc, lppt[i].x, lppt[i].y))
				return FALSE;

			if (!gdi_MoveToEx(hdc, lppt[i].x, lppt[i].y, NULL))
				return FALSE;
		}

		if (!gdi_MoveToEx(hdc, pt.x, pt.y, NULL))
			return FALSE;
	}

	return TRUE;
}

/**
 * Draw multiple series of connected line segments
 * @param hdc device context
 * @param lppt array of points
 * @param lpdwPolyPoints array of numbers of points per series
 * @param cCount count of entries in lpdwPolyPoints
 * @return nonzero on success, 0 otherwise
 */
BOOL gdi_PolyPolyline(HGDI_DC hdc, GDI_POINT* lppt, UINT32* lpdwPolyPoints,
                      DWORD cCount)
{
	UINT32 cPoints;
	DWORD i, j = 0;

	for (i = 0; i < cCount; i++)
	{
		cPoints = lpdwPolyPoints[i];

		if (!gdi_Polyline(hdc, &lppt[j], cPoints))
			return FALSE;

		j += cPoints;
	}

	return TRUE;
}

/**
 * Move pen from the current device context to a new position.
 * @param hdc device context
 * @param X x position
 * @param Y y position
 * @return nonzero on success, 0 otherwise
 */

BOOL gdi_MoveToEx(HGDI_DC hdc, UINT32 X, UINT32 Y, HGDI_POINT lpPoint)
{
	if (lpPoint != NULL)
	{
		lpPoint->x = hdc->pen->posX;
		lpPoint->y = hdc->pen->posY;
	}

	hdc->pen->posX = X;
	hdc->pen->posY = Y;
	return TRUE;
}
