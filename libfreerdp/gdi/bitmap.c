/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Bitmap Functions
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

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>

#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/log.h>
#include <freerdp/gdi/shape.h>

#include "brush.h"
#include "clipping.h"

#define TAG FREERDP_TAG("gdi.bitmap")

/**
 * Get pixel at the given coordinates.\n
 * @msdn{dd144909}
 * @param hdc device context
 * @param nXPos pixel x position
 * @param nYPos pixel y position
 * @return pixel color
 */

INLINE UINT32 gdi_GetPixel(HGDI_DC hdc, UINT32 nXPos, UINT32 nYPos)
{
	HGDI_BITMAP hBmp = (HGDI_BITMAP) hdc->selectedObject;
	BYTE* data = &(hBmp->data[(nYPos * hBmp->scanline) + nXPos * GetBytesPerPixel(
	                              hBmp->format)]);
	return ReadColor(data, hBmp->format);
}

INLINE BYTE* gdi_GetPointer(HGDI_BITMAP hBmp, UINT32 X, UINT32 Y)
{
	UINT32 bpp = GetBytesPerPixel(hBmp->format);
	return &hBmp->data[(Y * hBmp->width * bpp) + X * bpp];
}

/**
 * Set pixel at the given coordinates.\n
 * @msdn{dd145078}
 * @param hdc device context
 * @param X pixel x position
 * @param Y pixel y position
 * @param crColor new pixel color
 * @return
 */

static INLINE UINT32 gdi_SetPixelBmp(HGDI_BITMAP hBmp, UINT32 X, UINT32 Y,
                                     UINT32 crColor)
{
	BYTE* p = &hBmp->data[(Y * hBmp->scanline) + X * GetBytesPerPixel(
	                          hBmp->format)];
	WriteColor(p, hBmp->format, crColor);
	return crColor;
}

INLINE UINT32 gdi_SetPixel(HGDI_DC hdc, UINT32 X, UINT32 Y, UINT32 crColor)
{
	HGDI_BITMAP hBmp = (HGDI_BITMAP) hdc->selectedObject;
	return gdi_SetPixelBmp(hBmp, X, Y, crColor);
}

/**
 * Create a new bitmap with the given width, height, color format and pixel buffer.\n
 * @msdn{dd183485}
 * @param nWidth width
 * @param nHeight height
 * @param cBitsPerPixel bits per pixel
 * @param data pixel buffer
 * @param fkt_free The function used for deallocation of the buffer, NULL for none.
 * @return new bitmap
 */

HGDI_BITMAP gdi_CreateBitmap(UINT32 nWidth, UINT32 nHeight, UINT32 format,
                             BYTE* data)
{
	return gdi_CreateBitmapEx(nWidth, nHeight, format, 0, data, _aligned_free);
}

HGDI_BITMAP gdi_CreateBitmapEx(UINT32 nWidth, UINT32 nHeight, UINT32 format,
                               UINT32 stride, BYTE* data, void (*fkt_free)(void*))
{
	HGDI_BITMAP hBitmap = (HGDI_BITMAP) calloc(1, sizeof(GDI_BITMAP));

	if (!hBitmap)
		return NULL;

	hBitmap->objectType = GDIOBJECT_BITMAP;
	hBitmap->format = format;

	if (stride > 0)
		hBitmap->scanline = stride;
	else
		hBitmap->scanline = nWidth * GetBytesPerPixel(hBitmap->format);

	hBitmap->width = nWidth;
	hBitmap->height = nHeight;
	hBitmap->data = data;
	hBitmap->free = fkt_free;
	return hBitmap;
}

/**
 * Create a new bitmap of the given width and height compatible with the current device context.\n
 * @msdn{dd183488}
 * @param hdc device context
 * @param nWidth width
 * @param nHeight height
 * @return new bitmap
 */

HGDI_BITMAP gdi_CreateCompatibleBitmap(HGDI_DC hdc, UINT32 nWidth,
                                       UINT32 nHeight)
{
	HGDI_BITMAP hBitmap = (HGDI_BITMAP) calloc(1, sizeof(GDI_BITMAP));

	if (!hBitmap)
		return NULL;

	hBitmap->objectType = GDIOBJECT_BITMAP;
	hBitmap->format = hdc->format;
	hBitmap->width = nWidth;
	hBitmap->height = nHeight;
	hBitmap->data = _aligned_malloc(nWidth * nHeight * GetBytesPerPixel(
	                                    hBitmap->format), 16);
	hBitmap->free = _aligned_free;

	if (!hBitmap->data)
	{
		free(hBitmap);
		return NULL;
	}

	hBitmap->scanline = nWidth * GetBytesPerPixel(hBitmap->format);
	return hBitmap;
}

static BOOL BitBlt_SRCCOPY(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                           UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                           UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	HGDI_BITMAP hSrcBmp;
	HGDI_BITMAP hDstBmp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	if ((nHeight == 0) || (nWidth == 0))
		return TRUE;

	hSrcBmp = (HGDI_BITMAP) hdcSrc->selectedObject;
	hDstBmp = (HGDI_BITMAP) hdcDest->selectedObject;

	if (!hDstBmp || !hSrcBmp)
		return FALSE;

	return freerdp_image_copy(hDstBmp->data, hDstBmp->format, hDstBmp->scanline,
	                          nXDest, nYDest, nWidth, nHeight,
	                          hSrcBmp->data, hSrcBmp->format, hSrcBmp->scanline,
	                          nXSrc, nYSrc, palette);
}

static BOOL BitBlt_NOTSRCCOPY(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                              UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                              UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color = ReadColor(srcp, hdcSrc->format);
				color = ConvertColor(color, hdcSrc->format, hdcDest->format,
				                     palette);
				color = ~color;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCERASE(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                            UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                            UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	/* SDna */
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				UINT32 color;
				colorA = ConvertColor(colorA, hdcSrc->format, hdcDest->format,
				                      palette);
				color = ~colorA & colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_NOTSRCERASE(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                               UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                               UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	/* DSon */
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				colorA = ConvertColor(colorA, hdcSrc->format,
				                      hdcDest->format, palette);
				color = ~colorA | colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCINVERT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                             UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                             UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	/* DSx */
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				colorA = ConvertColor(colorA, hdcSrc->format,
				                      hdcDest->format, palette);
				color = colorA ^ colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCAND(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                          UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                          UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	/* DSa */
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				colorA = ConvertColor(colorA, hdcSrc->format,
				                      hdcDest->format, palette);
				color = colorA & colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCPAINT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                            UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                            UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	/* DSo */
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				colorA = ConvertColor(colorA, hdcSrc->format,
				                      hdcDest->format, palette);
				color = colorA | colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSPDxax(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                           UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                           UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						dstColor = (colorB ^ colorA) & (color ^ colorB);
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 dstColor;
						UINT32 color = ReadColor(patp, hdcDest->format);
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						dstColor = (colorB ^ colorA) & (color ^ colorB);
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_DPno(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor;
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = ~colorB | color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (patp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						dstColor = ~colorB | colorA;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_DPo(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                       UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                       UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor;
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB | color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (patp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB | colorA;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_Pn(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                      UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                      UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor = ~color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor;
						UINT32 color = ReadColor(patp, hdcDest->format);
						dstColor = ~color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_DPon(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor;
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB | ~color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (patp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB | ~colorA;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_DPan(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor;
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB & ~color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB & ~colorA;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_SDno(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 dstColor;
				UINT32 colorA = ReadColor(srcp, hdcDest->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				colorA = ConvertColor(colorA, hdcSrc->format,
				                      hdcDest->format, palette);
				dstColor = ~colorA | colorB;
				WriteColor(dstp, hdcDest->format, dstColor);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DPna(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor;
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = ~colorB & color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (patp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = ~colorB & colorA;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_PDno(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor;
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB | ~color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (patp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB | ~colorA;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_PDna(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (dstp)
					{
						UINT32 dstColor;
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB & ~color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (patp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						dstColor = colorB & ~colorA;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_DSPDxox(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                           UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                           UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y, color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->textColor;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						dstColor = (colorA ^ colorB) | (color ^ colorB);
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp && patp)
					{
						UINT32 color;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						UINT32 colorC = ReadColor(patp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (colorA ^ colorB) | (colorB ^ colorC);
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_PSDPxax(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                           UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                           UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y, colorC;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			colorC = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 color;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (colorA ^ colorC) & (colorB ^ colorC);
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 color;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						UINT32 colorC = ReadColor(patp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (colorA ^ colorC) & (colorB ^ colorC);
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_PSDnox(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                          UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                          UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y, colorA;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			colorA = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 color;
						UINT32 colorB = ReadColor(srcp, hdcSrc->format);
						UINT32 colorC = ReadColor(dstp, hdcDest->format);
						colorB = ConvertColor(colorB, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (~colorA | colorB) ^ colorC;
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp && patp)
					{
						UINT32 color;
						UINT32 colorB = ReadColor(srcp, hdcSrc->format);
						UINT32 colorC = ReadColor(dstp, hdcDest->format);
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						colorB = ConvertColor(colorB, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (~colorA | colorB) ^ colorC;
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_PDSona(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                          UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                          UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y, colorA;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			colorA = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 color;
						UINT32 colorC = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						colorC = ConvertColor(colorC, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (colorA | ~colorB) & colorC;
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp && patp)
					{
						UINT32 color;
						UINT32 colorC = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						UINT32 colorA = ReadColor(patp, hdcDest->format);
						colorC = ConvertColor(colorC, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (colorA | ~colorB) & colorC;
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_DPSDonox(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                            UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                            UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y, colorB;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			colorB = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 color;
						UINT32 colorC = ReadColor(srcp, hdcSrc->format);
						UINT32 colorA = ReadColor(dstp, hdcDest->format);
						colorC = ConvertColor(colorC, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (colorA | ~colorB | colorC) ^ colorA;
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp && patp)
					{
						UINT32 color;
						UINT32 colorC = ReadColor(srcp, hdcSrc->format);
						UINT32 colorA = ReadColor(dstp, hdcDest->format);
						UINT32 colorB = ReadColor(patp, hdcDest->format);
						colorC = ConvertColor(colorC, hdcSrc->format,
						                      hdcDest->format, palette);
						color = (colorA | ~colorB | colorC) ^ colorA;
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_SPDSxax(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                           UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                           UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y, color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	/* D = S ^ (P & (D ^ S)) */
	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 colorD;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						colorD = (colorA ^ color) & (colorB ^ colorA);
						WriteColor(dstp, hdcDest->format, colorD);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp && patp)
					{
						UINT32 colorD;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(dstp, hdcDest->format);
						UINT32 color = ReadColor(patp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						colorD = (colorA ^ color) & (colorB ^ colorA);
						WriteColor(dstp, hdcDest->format, colorD);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_SPna(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y, colorB;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			colorB = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 color;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						color = ~colorA & colorB;
						color = ConvertColor(color, hdcSrc->format, hdcDest->format, palette);
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc + x, nYSrc + y);
					const BYTE* patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

					if (srcp && patp && dstp)
					{
						UINT32 color;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(patp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						color = ~colorA & colorB;
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_DSna(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				colorA = ConvertColor(colorA, hdcSrc->format,
				                      hdcDest->format, palette);
				color = colorA & ~colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSxn(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				colorA = ConvertColor(colorA, hdcSrc->format,
				                      hdcDest->format, palette);
				color = ~colorA ^ colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSan(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                        UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                        UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				colorA = ConvertColor(colorA, hdcSrc->format,
				                      hdcDest->format, palette);
				color = ~colorA & colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_D(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                     UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                     UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(
			                       hdcDest, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
			                 hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color = ReadColor(srcp, hdcSrc->format);
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_MERGECOPY(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                             UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                             UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;
	UINT32 color;

	/* PSa */
	if (!hdcDest || !hdcSrc)
		return FALSE;

	switch (gdi_GetBrushStyle(hdcDest))
	{
		case GDI_BS_SOLID:
			color = hdcDest->brush->color;

			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && dstp)
					{
						UINT32 dstColor;
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						dstColor = color = colorA & color;
						WriteColor(dstp, hdcDest->format, dstColor);
					}
				}
			}

			break;

		default:
			for (y = 0; y < nHeight; y++)
			{
				for (x = 0; x < nWidth; x++)
				{
					const BYTE* srcp = gdi_get_bitmap_pointer(
					                       hdcSrc, nXSrc + x, nYSrc + y);
					const BYTE* patp = gdi_get_brush_pointer(
					                       hdcDest, nXDest + x, nYDest + y);
					BYTE* dstp = gdi_get_bitmap_pointer(
					                 hdcDest, nXDest + x, nYDest + y);

					if (srcp && patp && dstp)
					{
						UINT32 colorA = ReadColor(srcp, hdcSrc->format);
						UINT32 colorB = ReadColor(patp, hdcDest->format);
						colorA = ConvertColor(colorA, hdcSrc->format,
						                      hdcDest->format, palette);
						color = colorA & colorB;
						WriteColor(dstp, hdcDest->format, color);
					}
				}
			}

			break;
	}

	return TRUE;
}

static BOOL BitBlt_MERGEPAINT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                              UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                              UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette)
{
	UINT32 x, y;

	/* DSno */
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth; x++)
		{
			const BYTE* srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				UINT32 color;
				colorA = ConvertColor(colorA, hdcSrc->format, hdcDest->format,
				                      palette);
				color = colorA | ~colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

/**
 * Perform a bit blit operation on the given pixel buffers.\n
 * @msdn{dd183370}
 * @param hdcDest destination device context
 * @param nXDest destination x1
 * @param nYDest destination y1
 * @param nWidth width
 * @param nHeight height
 * @param hdcSrc source device context
 * @param nXSrc source x1
 * @param nYSrc source y1
 * @param rop raster operation code
 * @return 0 on failure, non-zero otherwise
 */
BOOL gdi_BitBlt(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                UINT32 nXSrc, UINT32 nYSrc, DWORD rop, const gdiPalette* palette)
{
	if (!hdcDest)
		return FALSE;

	if (gdi_PatBlt(hdcDest, nXDest, nYDest, nWidth, nHeight, rop,
	               hdcSrc, nXSrc, nYSrc))
		return TRUE;

	if (hdcSrc != NULL)
	{
		if (!gdi_ClipCoords(hdcDest, &nXDest, &nYDest, &nWidth, &nHeight, &nXSrc,
		                    &nYSrc))
			return TRUE;
	}
	else
	{
		if (!gdi_ClipCoords(hdcDest, &nXDest, &nYDest, &nWidth, &nHeight, NULL, NULL))
			return TRUE;
	}

	if (!gdi_InvalidateRegion(hdcDest, nXDest, nYDest, nWidth, nHeight))
		return FALSE;

	switch (rop)
	{
		case GDI_SRCCOPY:
			return BitBlt_SRCCOPY(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                      hdcSrc, nXSrc, nYSrc, palette);

		case GDI_SPna:
			return BitBlt_SPna(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DSna:
			return BitBlt_DSna(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DSPDxax:
			return BitBlt_DSPDxax(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                      hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DSPDxox:
			return BitBlt_DSPDxox(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                      hdcSrc, nXSrc, nYSrc, palette);

		case GDI_PSDPxax:
			return BitBlt_PSDPxax(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                      hdcSrc, nXSrc, nYSrc, palette);

		case GDI_SPDSxax:
			return BitBlt_SPDSxax(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                      hdcSrc, nXSrc, nYSrc, palette);

		case GDI_NOTSRCCOPY:
			return BitBlt_NOTSRCCOPY(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                         hdcSrc, nXSrc, nYSrc, palette);

		case GDI_SRCERASE:
			return BitBlt_SRCERASE(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                       hdcSrc, nXSrc, nYSrc, palette);

		case GDI_NOTSRCERASE:
			return BitBlt_NOTSRCERASE(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                          hdcSrc, nXSrc, nYSrc, palette);

		case GDI_SRCINVERT:
			return BitBlt_SRCINVERT(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                        hdcSrc, nXSrc, nYSrc, palette);

		case GDI_SRCAND:
			return BitBlt_SRCAND(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                     hdcSrc, nXSrc, nYSrc, palette);

		case GDI_SRCPAINT:
			return BitBlt_SRCPAINT(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                       hdcSrc, nXSrc, nYSrc, palette);

		case GDI_MERGECOPY:
			return BitBlt_MERGECOPY(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                        hdcSrc, nXSrc, nYSrc, palette);

		case GDI_MERGEPAINT:
			return BitBlt_MERGEPAINT(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                         hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DPno:
			return BitBlt_DPno(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DPna:
			return BitBlt_DPna(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_SDno:
			return BitBlt_SDno(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_PDno:
			return BitBlt_PDno(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_PDna:
			return BitBlt_PDna(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DPo:
			return BitBlt_DPo(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                  hdcSrc, nXSrc, nYSrc, palette);

		case GDI_Pn:
			return BitBlt_Pn(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                 hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DPon:
			return BitBlt_DPon(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DPan:
			return BitBlt_DPan(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DSxn:
			return BitBlt_DSxn(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DSan:
			return BitBlt_DSan(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                   hdcSrc, nXSrc, nYSrc, palette);

		case GDI_PSDnox:
			return BitBlt_PSDnox(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                     hdcSrc, nXSrc, nYSrc, palette);

		case GDI_PDSona:
			return BitBlt_PDSona(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                     hdcSrc, nXSrc, nYSrc, palette);

		case GDI_DPSDonox:
			return BitBlt_DPSDonox(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                       hdcSrc, nXSrc, nYSrc, palette);

		case GDI_D:
			return BitBlt_D(hdcDest, nXDest, nYDest, nWidth, nHeight,
			                hdcSrc, nXSrc, nYSrc, palette);

		default:
			WLog_ERR(TAG,  "BitBlt: unknown rop: %s", gdi_rop_to_string(rop));
			return FALSE;
	}
}
