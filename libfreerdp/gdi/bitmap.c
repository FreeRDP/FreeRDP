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
			   UINT32 nXSrc, UINT32 nYSrc)
{
	INT32 y;
	BYTE* srcp;
	BYTE* dstp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	if (hdcDest->format != hdcSrc->format)
	{
		WLog_ERR(TAG, "Function does not support color conversion!");
		return FALSE;
	}

	if ((nHeight == 0) || (nWidth == 0))
		return TRUE;

	if ((hdcDest->selectedObject != hdcSrc->selectedObject) ||
	    !gdi_CopyOverlap(nXDest, nYDest, nWidth, nHeight, nXSrc, nYSrc))
	{
		for (y = 0; y < nHeight; y++)
		{
			srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (srcp != 0 && dstp != 0)
				memmove(dstp, srcp, nWidth * GetBytesPerPixel(hdcDest->format));
		}

		return TRUE;
	}

	if (nYSrc < nYDest)
	{
		/* copy down (bottom to top) */
		for (y = nHeight - 1; y >= 0; y--)
		{
			srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (srcp != 0 && dstp != 0)
				memmove(dstp, srcp, nWidth * GetBytesPerPixel(hdcDest->format));
		}
	}
	else
	{
		/* copy straight right */
		for (y = 0; y < nHeight; y++)
		{
			srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (srcp != 0 && dstp != 0)
				memmove(dstp, srcp, nWidth * GetBytesPerPixel(hdcDest->format));
		}
	}

	return TRUE;
}

static BOOL BitBlt_NOTSRCCOPY(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
			const BYTE* srcp = gdi_get_bitmap_pointer(
						   hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
					     hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 color = ReadColor(srcp, hdcSrc->format);
				color = ConvertColor(color, hdcSrc->format, hdcDest->format, NULL);
				color = ~color;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCERASE(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
			const BYTE* srcp = gdi_get_bitmap_pointer(
						   hdcSrc, nXSrc + x, nYSrc + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
					     hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				UINT32 color;

				colorA = ConvertColor(colorA, hdcSrc->format, hdcDest->format, NULL);
				color = colorA & ~colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_NOTSRCERASE(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
						      hdcDest->format, NULL);

				color = ~colorA & ~colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCINVERT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
						      hdcDest->format, NULL);

				color = colorA ^ colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCAND(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
						      hdcDest->format, NULL);

				color = colorA & colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCPAINT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
						      hdcDest->format, NULL);

				color = colorA | colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSPDxax(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			   UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
			   UINT32 nXSrc, UINT32 nYSrc)
{
	UINT32 x, y;
	UINT32 color;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	/* D = (S & P) | (~S & D) */
	color = hdcDest->textColor;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x<nWidth; x++)
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
						      hdcDest->format, NULL);

				color = (colorA & color) | (~colorA & colorB);
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PSDPxax(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			   UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
			   UINT32 nXSrc, UINT32 nYSrc)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	/* D = (S & D) | (~S & P) */
	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		UINT32 colorC = hdcDest->brush->color;

		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x<nWidth; x++)
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
							      hdcDest->format, NULL);

					color = (colorA & colorB) | (~colorA & colorC);
					WriteColor(dstp, hdcDest->format, color);
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x<nWidth; x++)
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
							      hdcDest->format, NULL);

					color = (colorA & colorB) | (~colorA & colorC);
					WriteColor(dstp, hdcDest->format, color);
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SPDSxax(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			   UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
			   UINT32 nXSrc, UINT32 nYSrc)
{
	UINT32 x, y;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	/* D = S ^ (P & (D ^ S)) */
	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		UINT32 color = hdcDest->brush->color;

		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x<nWidth; x++)
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
							      hdcDest->format, NULL);

					colorD = colorA ^ (color & (colorB ^ colorA));
					WriteColor(dstp, hdcDest->format, color);
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x<nWidth; x++)
			{
				const BYTE* srcp = gdi_get_bitmap_pointer(
							   hdcSrc, nXSrc + x, nYSrc + y);
				const BYTE* patp = gdi_get_brush_pointer(
							   hdcDest, nXDest + x, nYDest + y);
				BYTE* dstp = gdi_get_bitmap_pointer(
						     hdcDest, nXDest + x, nYDest + y);

				if (srcp && dstp)
				{
					UINT32 colorD;
					UINT32 colorA = ReadColor(srcp, hdcSrc->format);
					UINT32 colorB = ReadColor(dstp, hdcDest->format);
					UINT32 color = ReadColor(patp, hdcDest->format);

					colorA = ConvertColor(colorA, hdcSrc->format,
							      hdcDest->format, NULL);

					colorD = colorA ^ (color & (colorB ^ colorA));
					WriteColor(dstp, hdcDest->format, color);
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SPna(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(patp, hdcDest->format);

				colorA = ConvertColor(colorA, hdcSrc->format,
						      hdcDest->format, NULL);
				color = colorA & ~colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSna(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
						      hdcDest->format, NULL);

				color = ~colorA & colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}



static BOOL BitBlt_MERGECOPY(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
			const BYTE* srcp = gdi_get_bitmap_pointer(
						   hdcSrc, nXSrc + x, nYSrc + y);
			const BYTE* patp = gdi_get_brush_pointer(
						   hdcDest, nXDest + x, nYDest + y);
			BYTE* dstp = gdi_get_bitmap_pointer(
					     hdcDest, nXDest + x, nYDest + y);

			if (srcp && patp && dstp)
			{
				UINT32 color;
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(patp, hdcDest->format);

				colorA = ConvertColor(colorA, hdcSrc->format,
						      hdcDest->format, NULL);
				color = colorA & colorB;
				WriteColor(dstp, hdcDest->format, color);
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_MERGEPAINT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
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
			BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

			if (srcp && dstp)
			{
				UINT32 colorA = ReadColor(srcp, hdcSrc->format);
				UINT32 colorB = ReadColor(dstp, hdcDest->format);
				UINT32 color;

				colorA = ConvertColor(colorA, hdcSrc->format, hdcDest->format, NULL);
				color = ~colorA | colorB;
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
		  UINT32 nXSrc, UINT32 nYSrc, DWORD rop)
{
	if (!hdcDest)
		return FALSE;

	if (hdcSrc != NULL)
	{
		if (!gdi_ClipCoords(hdcDest, &nXDest, &nYDest, &nWidth, &nHeight, &nXSrc, &nYSrc))
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
		case GDI_BLACKNESS:
			return BitBlt_BLACKNESS(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_WHITENESS:
			return BitBlt_WHITENESS(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_SRCCOPY:
			return BitBlt_SRCCOPY(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SPna:
			return BitBlt_SPna(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSna:
			return BitBlt_DSna(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSPDxax:
			return BitBlt_DSPDxax(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_PSDPxax:
			return BitBlt_PSDPxax(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SPDSxax:
			return BitBlt_SPDSxax(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_NOTSRCCOPY:
			return BitBlt_NOTSRCCOPY(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_SRCERASE:
			return BitBlt_SRCERASE(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_NOTSRCERASE:
			return BitBlt_NOTSRCERASE(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCINVERT:
			return BitBlt_SRCINVERT(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCAND:
			return BitBlt_SRCAND(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCPAINT:
			return BitBlt_SRCPAINT(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_MERGECOPY:
			return BitBlt_MERGECOPY(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_MERGEPAINT:
			return BitBlt_MERGEPAINT(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_PATCOPY:
			return BitBlt_PATCOPY(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_PATINVERT:
			return BitBlt_PATINVERT(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_PATPAINT:
			return BitBlt_PATPAINT(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
	}

	WLog_ERR(TAG,  "BitBlt: unknown rop: 0x%08X", rop);
	return FALSE;
}
