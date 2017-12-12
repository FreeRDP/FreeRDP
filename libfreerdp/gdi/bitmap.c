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
#include "../gdi/gdi.h"

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

static BOOL op_not(UINT32* stack, UINT32* stackp)
{
	if (!stack || !stackp)
		return FALSE;

	if (*stackp < 1)
		return FALSE;

	stack[(*stackp) - 1] = ~stack[(*stackp) - 1];
	return TRUE;
}

static BOOL op_and(UINT32* stack, UINT32* stackp)
{
	if (!stack || !stackp)
		return FALSE;

	if (*stackp < 2)
		return FALSE;

	(*stackp)--;
	stack[(*stackp) - 1] &= stack[(*stackp)];
	return TRUE;
}

static BOOL op_or(UINT32* stack, UINT32* stackp)
{
	if (!stack || !stackp)
		return FALSE;

	if (*stackp < 2)
		return FALSE;

	(*stackp)--;
	stack[(*stackp) - 1] |= stack[(*stackp)];
	return TRUE;
}

static BOOL op_xor(UINT32* stack, UINT32* stackp)
{
	if (!stack || !stackp)
		return FALSE;

	if (*stackp < 2)
		return FALSE;

	(*stackp)--;
	stack[(*stackp) - 1] ^= stack[(*stackp)];
	return TRUE;
}

static UINT32 process_rop(UINT32 src, UINT32 dst, UINT32 pat, const char* rop,
                          UINT32 format)
{
	DWORD stack[10] = { 0 };
	DWORD stackp = 0;

	while (*rop != '\0')
	{
		char op = *rop++;

		switch (op)
		{
			case '0':
				stack[stackp++] = FreeRDPGetColor(format, 0, 0, 0, 0xFF);
				break;

			case '1':
				stack[stackp++] = FreeRDPGetColor(format, 0xFF, 0xFF, 0xFF, 0xFF);
				break;

			case 'D':
				stack[stackp++] = dst;
				break;

			case 'S':
				stack[stackp++] = src;
				break;

			case 'P':
				stack[stackp++] = pat;
				break;

			case 'x':
				op_xor(stack, &stackp);
				break;

			case 'a':
				op_and(stack, &stackp);
				break;

			case 'o':
				op_or(stack, &stackp);
				break;

			case 'n':
				op_not(stack, &stackp);
				break;

			default:
				break;
		}
	}

	return stack[0];
}

static INLINE BOOL BitBlt_write(HGDI_DC hdcDest, HGDI_DC hdcSrc, UINT32 nXDest,
                                UINT32 nYDest, UINT32 nXSrc, UINT32 nYSrc, UINT32 x, UINT32 y,
                                BOOL useSrc, BOOL usePat, UINT32 style,
                                const char* rop, const gdiPalette* palette)
{
	UINT32 dstColor;
	UINT32 colorA;
	UINT32 colorB = 0;
	UINT32 colorC = 0;
	BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, nXDest + x, nYDest + y);

	if (!dstp)
	{
		WLog_ERR(TAG, "dstp=%p", (void*) dstp);
		return FALSE;
	}

	colorA = ReadColor(dstp, hdcDest->format);

	if (useSrc)
	{
		const BYTE* srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc + x, nYSrc + y);

		if (!srcp)
		{
			WLog_ERR(TAG, "srcp=%p", (void*) srcp);
			return FALSE;
		}

		colorC = ReadColor(srcp, hdcSrc->format);
		colorC = FreeRDPConvertColor(colorC, hdcSrc->format, hdcDest->format, palette);
	}

	if (usePat)
	{
		switch (style)
		{
			case GDI_BS_SOLID:
				colorB = hdcDest->brush->color;
				break;

			case GDI_BS_HATCHED:
			case GDI_BS_PATTERN:
				{
					const BYTE* patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);

					if (!patp)
					{
						WLog_ERR(TAG, "patp=%p", (void*) patp);
						return FALSE;
					}

					colorB = ReadColor(patp, hdcDest->format);
				}
				break;

			default:
				break;
		}
	}

	dstColor = process_rop(colorC, colorA, colorB, rop, hdcDest->format);
	return WriteColor(dstp, hdcDest->format, dstColor);
}

static BOOL BitBlt_process(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
                           UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
                           UINT32 nXSrc, UINT32 nYSrc, const char* rop, const gdiPalette* palette)
{
	INT64 x, y;
	UINT32 style = 0;
	BOOL useSrc = FALSE;
	BOOL usePat = FALSE;
	const char* iter = rop;

	while (*iter != '\0')
	{
		switch (*iter++)
		{
			case 'P':
				usePat = TRUE;
				break;

			case 'S':
				useSrc = TRUE;
				break;

			default:
				break;
		}
	}

	if (!hdcDest)
		return FALSE;

	if (useSrc && !hdcSrc)
		return FALSE;

	if (usePat)
	{
		style = gdi_GetBrushStyle(hdcDest);

		switch (style)
		{
			case GDI_BS_SOLID:
			case GDI_BS_HATCHED:
			case GDI_BS_PATTERN:
				break;

			default:
				WLog_ERR(TAG, "Invalid brush!!");
				return FALSE;
		}
	}

	if ((nXDest > nXSrc) && (nYDest > nYSrc))
	{
		for (y = (INT64)nHeight - 1; y >= 0; y--)
		{
			for (x = (INT64)nWidth - 1; x >= 0; x--)
			{
				if (!BitBlt_write(hdcDest, hdcSrc, nXDest, nYDest,
				                  nXSrc, nYSrc, x, y, useSrc,
				                  usePat, style, rop, palette))
					return FALSE;
			}
		}
	}
	else if (nXDest > nXSrc)
	{
		for (y = 0; y < nHeight; y++)
		{
			for (x = (INT64)nWidth - 1; x >= 0; x--)
			{
				if (!BitBlt_write(hdcDest, hdcSrc, nXDest, nYDest,
				                  nXSrc, nYSrc, x, y, useSrc,
				                  usePat, style, rop, palette))
					return FALSE;
			}
		}
	}
	else if (nYDest > nYSrc)
	{
		for (y = (INT64)nHeight - 1; y >= 0; y--)
		{
			for (x = 0; x < nWidth; x++)
			{
				if (!BitBlt_write(hdcDest, hdcSrc, nXDest, nYDest,
				                  nXSrc, nYSrc, x, y, useSrc,
				                  usePat, style, rop, palette))
					return FALSE;
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			for (x = 0; x < nWidth; x++)
			{
				if (!BitBlt_write(hdcDest, hdcSrc, nXDest, nYDest,
				                  nXSrc, nYSrc, x, y, useSrc,
				                  usePat, style, rop, palette))
					return FALSE;
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
	HGDI_BITMAP hSrcBmp, hDstBmp;

	if (!hdcDest)
		return FALSE;

	if (!gdi_ClipCoords(hdcDest, &nXDest, &nYDest, &nWidth, &nHeight, &nXSrc,
	                    &nYSrc))
		return TRUE;

	/* Check which ROP should be performed.
	 * Some specific ROP are used heavily and are resource intensive,
	 * add optimized versions for these here.
	 *
	 * For all others fall back to the generic implementation.
	 */
	switch (rop)
	{
		case GDI_SRCCOPY:
			hSrcBmp = (HGDI_BITMAP) hdcSrc->selectedObject;
			hDstBmp = (HGDI_BITMAP) hdcDest->selectedObject;

			if (!freerdp_image_copy(hDstBmp->data, hDstBmp->format, hDstBmp->scanline,
			                        nXDest, nYDest, nWidth, nHeight,
			                        hSrcBmp->data, hSrcBmp->format, hSrcBmp->scanline, nXSrc, nYSrc, palette, FREERDP_FLIP_NONE))
				return FALSE;

			break;

		case GDI_DSTCOPY:
			hSrcBmp = (HGDI_BITMAP) hdcDest->selectedObject;
			hDstBmp = (HGDI_BITMAP) hdcDest->selectedObject;

			if (!freerdp_image_copy(hDstBmp->data, hDstBmp->format, hDstBmp->scanline,
			                        nXDest, nYDest, nWidth, nHeight,
			                        hSrcBmp->data, hSrcBmp->format, hSrcBmp->scanline, nXSrc, nYSrc, palette, FREERDP_FLIP_NONE))
				return FALSE;

			break;

		default:
			if (!BitBlt_process(hdcDest, nXDest, nYDest,
			                    nWidth, nHeight, hdcSrc,
			                    nXSrc, nYSrc, gdi_rop_to_string(rop), palette))
				return FALSE;

			break;
	}

	if (!gdi_InvalidateRegion(hdcDest, nXDest, nYDest, nWidth, nHeight))
		return FALSE;

	return TRUE;
}
