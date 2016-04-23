/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Brush Functions
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

/* GDI Brush Functions: http://msdn.microsoft.com/en-us/library/dd183395/ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/region.h>
#include <freerdp/log.h>

#include "clipping.h"

#define TAG FREERDP_TAG("gdi.brush")

/**
 * Create a new solid brush.\n
 * @msdn{dd183518}
 * @param crColor brush color
 * @return new brush
 */

HGDI_BRUSH gdi_CreateSolidBrush(UINT32 crColor)
{
	HGDI_BRUSH hBrush = (HGDI_BRUSH) calloc(1, sizeof(GDI_BRUSH));

	if (!hBrush)
		return NULL;

	hBrush->objectType = GDIOBJECT_BRUSH;
	hBrush->style = GDI_BS_SOLID;
	hBrush->color = crColor;
	return hBrush;
}

/**
 * Create a new pattern brush.\n
 * @msdn{dd183508}
 * @param hbmp pattern bitmap
 * @return new brush
 */

HGDI_BRUSH gdi_CreatePatternBrush(HGDI_BITMAP hbmp)
{
	HGDI_BRUSH hBrush = (HGDI_BRUSH) calloc(1, sizeof(GDI_BRUSH));

	if (!hBrush)
		return NULL;

	hBrush->objectType = GDIOBJECT_BRUSH;
	hBrush->style = GDI_BS_PATTERN;
	hBrush->pattern = hbmp;
	return hBrush;
}

HGDI_BRUSH gdi_CreateHatchBrush(HGDI_BITMAP hbmp)
{
	HGDI_BRUSH hBrush = (HGDI_BRUSH) calloc(1, sizeof(GDI_BRUSH));

	if (!hBrush)
		return NULL;

	hBrush->objectType = GDIOBJECT_BRUSH;
	hBrush->style = GDI_BS_HATCHED;
	hBrush->pattern = hbmp;
	return hBrush;
}

/**
 * Perform a pattern blit operation on the given pixel buffer.\n
 * @msdn{dd162778}
 * @param hdc device context
 * @param nXLeft x1
 * @param nYLeft y1
 * @param nWidth width
 * @param nHeight height
 * @param rop raster operation code
 * @return nonzero if successful, 0 otherwise
 */
static BOOL BitBlt_DPa(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                       UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
			BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

			if (dstp && patp)
			{
				UINT32 color = ReadColor(patp, hdcDest->format);
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PDxn(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
			BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

			if (dstp && patp)
			{
				UINT32 colorA = ReadColor(dstp, hdcDest->format);
				UINT32 colorB = ReadColor(patp, hdcDest->format);
				UINT32 color = colorA ^ ~colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PATINVERT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                             UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y;

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		UINT32 color = hdcDest->brush->color;

		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x < nWidth; x++)
			{
				BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

				if (dstp)
				{
					color ^= color;
					WriteColor(dstp, hdcDest->format, color);
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x < nWidth; x++)
			{
				const BYTE* patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
				BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

				if (patp && dstp)
				{
					UINT32 colorA = ReadColor(patp, hdcDest->format);
					UINT32 colorB = ReadColor(dstp, hdcDest->format);
					UINT32 color = colorA ^ colorB;
					WriteColor(dstp, hdcDest->format, color);
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PATPAINT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                            UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                            UINT32 nXSrc, UINT32 nYSrc)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc + x, nYSrc + y);
			const BYTE* patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
			BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

			if (srcp && patp && dstp)
			{
				UINT32 colorA = ReadColor(dstp, hdcDest->format);
				UINT32 colorB = ReadColor(patp, hdcDest->format);
				UINT32 colorC = ReadColor(srcp, hdcDest->format);
				UINT32 color = colorA | colorB | ~colorC;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PATCOPY(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                           UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y, xOffset, yOffset;

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		UINT32 color = hdcDest->brush->color;

		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x < nWidth; x++)
			{
				BYTE* dstp = gdi_get_bitmap_pointer(
				                 hdcDest, nXDest + x, nYDest + y);

				if (dstp)
					WriteColor(dstp, hdcDest->format, color);
			}
		}
	}
	else
	{
		if (hdcDest->brush->style == GDI_BS_HATCHED)
		{
			xOffset = 0;
			yOffset = 2; /* +2 added after comparison to mstsc */
		}
		else
		{
			xOffset = 0;
			yOffset = 0;
		}

		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x < nWidth; x++)
			{
				const BYTE* patp = gdi_get_brush_pointer(
				                       hdcDest, nXDest + x + xOffset,
				                       nYDest + y + yOffset);
				BYTE* dstp = gdi_get_bitmap_pointer(
				                 hdcDest, nXDest + x,
				                 nYDest + y);

				if (patp && dstp)
				{
					UINT32 color = ReadColor(patp, hdcDest->format);
					WriteColor(dstp, hdcDest->format, color);
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSTINVERT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                             UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (dstp)
			{
				UINT32 color = ReadColor(dstp, hdcDest->format);
				color = ~color;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_BLACKNESS(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                             UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y;
	UINT32 color = GetColor(hdcDest->format, 0, 0, 0, 0xFF);

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

			if (dstp)
				WriteColor(dstp, hdcDest->format, color);
		}
	}

	return TRUE;
}

static BOOL BitBlt_WHITENESS(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                             UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y;
	UINT32 color = GetColor(hdcDest->format, 0, 0, 0, 0);

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

			if (dstp)
				WriteColor(dstp, hdcDest->format, color);
		}
	}

	return TRUE;
}

BOOL gdi_PatBlt(HGDI_DC hdc, UINT32 nXLeft, UINT32 nYLeft,
                UINT32 nWidth, UINT32 nHeight, DWORD rop)
{
	if (!gdi_ClipCoords(hdc, &nXLeft, &nYLeft, &nWidth, &nHeight, NULL, NULL))
		return TRUE;

	if (!gdi_InvalidateRegion(hdc, nXLeft, nYLeft, nWidth, nHeight))
		return FALSE;

	switch (rop)
	{
		case GDI_PATCOPY:
			return BitBlt_PATCOPY(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_PATINVERT:
			return BitBlt_PATINVERT(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_BLACKNESS:
			return BitBlt_BLACKNESS(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_WHITENESS:
			return BitBlt_WHITENESS(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_DPa:
			return BitBlt_DPa(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_PDxn:
			return BitBlt_PDxn(hdc, nXLeft, nYLeft, nWidth, nHeight);

		default:
			break;
	}

	return FALSE;
}
