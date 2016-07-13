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

const char* gdi_rop_to_string(UINT32 code)
{
	static char buffer[1024];

	switch (code)
	{
		/* Binary Raster Operations (ROP2) */
		case GDI_R2_BLACK:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_BLACK [%08X]", code);
			break;

		case GDI_R2_NOTMERGEPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_NOTMERGEPEN [%08X]", code);
			break;

		case GDI_R2_MASKNOTPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_MASKNOTPEN [%08X]", code);
			break;

		case GDI_R2_NOTCOPYPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_NOTCOPYPEN [%08X]", code);
			break;

		case GDI_R2_MASKPENNOT:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_MASKPENNOT [%08X]", code);
			break;

		case GDI_R2_NOT:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_NOT [%08X]", code);
			break;

		case GDI_R2_XORPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_XORPEN [%08X]", code);
			break;

		case GDI_R2_NOTMASKPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_NOTMASKPEN [%08X]", code);
			break;

		case GDI_R2_MASKPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_MASKPEN [%08X]", code);
			break;

		case GDI_R2_NOTXORPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_NOTXORPEN [%08X]", code);
			break;

		case GDI_R2_NOP:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_NOP [%08X]", code);
			break;

		case GDI_R2_MERGENOTPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_MERGENOTPEN [%08X]", code);
			break;

		case GDI_R2_COPYPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_COPYPEN [%08X]", code);
			break;

		case GDI_R2_MERGEPENNOT:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_MERGEPENNOT [%08X]", code);
			break;

		case GDI_R2_MERGEPEN:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_MERGEPEN [%08X]", code);
			break;

		case GDI_R2_WHITE:
			_snprintf(buffer, sizeof(buffer), "GDI_R2_WHITE [%08X]", code);
			break;

		/* Ternary Raster Operations (ROP3) */
		case GDI_SRCCOPY:
			_snprintf(buffer, sizeof(buffer), "GDI_SRCCOPY [%08X]", code);
			break;

		case GDI_SRCPAINT:
			_snprintf(buffer, sizeof(buffer), "GDI_SRCPAINT [%08X]", code);
			break;

		case GDI_SRCAND	:
			_snprintf(buffer, sizeof(buffer), "GDI_SRCAND	 [%08X]", code);
			break;

		case GDI_SRCINVERT:
			_snprintf(buffer, sizeof(buffer), "GDI_SRCINVERT [%08X]", code);
			break;

		case GDI_SRCERASE:
			_snprintf(buffer, sizeof(buffer), "GDI_SRCERASE [%08X]", code);
			break;

		case GDI_NOTSRCCOPY:
			_snprintf(buffer, sizeof(buffer), "GDI_NOTSRCCOPY [%08X]", code);
			break;

		case GDI_NOTSRCERASE:
			_snprintf(buffer, sizeof(buffer), "GDI_NOTSRCERASE [%08X]", code);
			break;

		case GDI_MERGECOPY:
			_snprintf(buffer, sizeof(buffer), "GDI_MERGECOPY [%08X]", code);
			break;

		case GDI_MERGEPAINT:
			_snprintf(buffer, sizeof(buffer), "GDI_MERGEPAINT [%08X]", code);
			break;

		case GDI_PATCOPY:
			_snprintf(buffer, sizeof(buffer), "GDI_PATCOPY [%08X]", code);
			break;

		case GDI_PATPAINT:
			_snprintf(buffer, sizeof(buffer), "GDI_PATPAINT [%08X]", code);
			break;

		case GDI_PATINVERT:
			_snprintf(buffer, sizeof(buffer), "GDI_PATINVERT [%08X]", code);
			break;

		case GDI_DSTINVERT:
			_snprintf(buffer, sizeof(buffer), "GDI_DSTINVERT [%08X]", code);
			break;

		case GDI_BLACKNESS:
			_snprintf(buffer, sizeof(buffer), "GDI_BLACKNESS [%08X]", code);
			break;

		case GDI_WHITENESS:
			_snprintf(buffer, sizeof(buffer), "GDI_WHITENESS [%08X]", code);
			break;

		case GDI_DSPDxax:
			_snprintf(buffer, sizeof(buffer), "GDI_DSPDxax [%08X]", code);
			break;

		case GDI_PSDPxax:
			_snprintf(buffer, sizeof(buffer), "GDI_PSDPxax [%08X]", code);
			break;

		case GDI_SPna:
			_snprintf(buffer, sizeof(buffer), "GDI_SPna [%08X]", code);
			break;

		case GDI_DSna:
			_snprintf(buffer, sizeof(buffer), "GDI_DSna [%08X]", code);
			break;

		case GDI_DPa:
			_snprintf(buffer, sizeof(buffer), "GDI_DPa [%08X]", code);
			break;

		case GDI_PDxn:
			_snprintf(buffer, sizeof(buffer), "GDI_PDxn [%08X]", code);
			break;

		case GDI_DSxn:
			_snprintf(buffer, sizeof(buffer), "GDI_DSxn [%08X]", code);
			break;

		case GDI_PSDnox:
			_snprintf(buffer, sizeof(buffer), "GDI_PSDnox [%08X]", code);
			break;

		case GDI_PDSona:
			_snprintf(buffer, sizeof(buffer), "GDI_PDSona [%08X]", code);
			break;

		case GDI_DSPDxox:
			_snprintf(buffer, sizeof(buffer), "GDI_DSPDxox [%08X]", code);
			break;

		case GDI_DPSDonox:
			_snprintf(buffer, sizeof(buffer), "GDI_DPSDonox [%08X]", code);
			break;

		case GDI_SPDSxax:
			_snprintf(buffer, sizeof(buffer), "GDI_SPDSxax [%08X]", code);
			break;

		case GDI_DPon:
			_snprintf(buffer, sizeof(buffer), "GDI_DPon [%08X]", code);
			break;

		case GDI_DPna:
			_snprintf(buffer, sizeof(buffer), "GDI_DPna [%08X]", code);
			break;

		case GDI_Pn:
			_snprintf(buffer, sizeof(buffer), "GDI_Pn [%08X]", code);
			break;

		case GDI_PDna:
			_snprintf(buffer, sizeof(buffer), "GDI_PDna [%08X]", code);
			break;

		case GDI_DPan:
			_snprintf(buffer, sizeof(buffer), "GDI_DPan [%08X]", code);
			break;

		case GDI_DSan:
			_snprintf(buffer, sizeof(buffer), "GDI_DSan [%08X]", code);
			break;

		case GDI_D:
			_snprintf(buffer, sizeof(buffer), "GDI_D [%08X]", code);
			break;

		case GDI_DPno:
			_snprintf(buffer, sizeof(buffer), "GDI_DPno [%08X]", code);
			break;

		case GDI_SDno:
			_snprintf(buffer, sizeof(buffer), "GDI_SDno [%08X]", code);
			break;

		case GDI_PDno:
			_snprintf(buffer, sizeof(buffer), "GDI_PDno [%08X]", code);
			break;

		case GDI_DPo:
			_snprintf(buffer, sizeof(buffer), "GDI_DPo [%08X]", code);
			break;

		default:
			_snprintf(buffer, sizeof(buffer), "UNKNOWN [%02X]", code);
			break;
	}

	return buffer;
}

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
                UINT32 nWidth, UINT32 nHeight, DWORD rop,
                HGDI_DC hdcSrc, UINT32 nXSrc, UINT32 nYSrc)
{
	if (!gdi_ClipCoords(hdc, &nXLeft, &nYLeft, &nWidth, &nHeight, NULL, NULL))
		return TRUE;

	if (!gdi_InvalidateRegion(hdc, nXLeft, nYLeft, nWidth, nHeight))
		return FALSE;

	switch (rop)
	{
		case GDI_PATPAINT:
			return BitBlt_PATPAINT(hdc, nXLeft, nYLeft, nWidth, nHeight,
			                       hdcSrc, nXSrc, nYSrc);

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
