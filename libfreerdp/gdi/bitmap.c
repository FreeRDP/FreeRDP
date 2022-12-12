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

#include <freerdp/config.h>

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
 * Get pixel at the given coordinates. msdn{dd144909}
 * @param hdc device context
 * @param nXPos pixel x position
 * @param nYPos pixel y position
 * @return pixel color
 */

UINT32 gdi_GetPixel(HGDI_DC hdc, UINT32 nXPos, UINT32 nYPos)
{
	HGDI_BITMAP hBmp = (HGDI_BITMAP)hdc->selectedObject;
	BYTE* data =
	    &(hBmp->data[(nYPos * hBmp->scanline) + nXPos * FreeRDPGetBytesPerPixel(hBmp->format)]);
	return FreeRDPReadColor(data, hBmp->format);
}

BYTE* gdi_GetPointer(HGDI_BITMAP hBmp, UINT32 X, UINT32 Y)
{
	UINT32 bpp = FreeRDPGetBytesPerPixel(hBmp->format);
	return &hBmp->data[(Y * hBmp->width * bpp) + X * bpp];
}

/**
 * Set pixel at the given coordinates. msdn{dd145078}
 *
 * @param hBmp device context
 * @param X pixel x position
 * @param Y pixel y position
 * @param crColor new pixel color
 * @return the color written
 */

static INLINE UINT32 gdi_SetPixelBmp(HGDI_BITMAP hBmp, UINT32 X, UINT32 Y, UINT32 crColor)
{
	BYTE* p = &hBmp->data[(Y * hBmp->scanline) + X * FreeRDPGetBytesPerPixel(hBmp->format)];
	FreeRDPWriteColor(p, hBmp->format, crColor);
	return crColor;
}

UINT32 gdi_SetPixel(HGDI_DC hdc, UINT32 X, UINT32 Y, UINT32 crColor)
{
	HGDI_BITMAP hBmp = (HGDI_BITMAP)hdc->selectedObject;
	return gdi_SetPixelBmp(hBmp, X, Y, crColor);
}

/**
 * Create a new bitmap with the given width, height, color format and pixel buffer. msdn{dd183485}
 *
 * @param nWidth width
 * @param nHeight height
 * @param format the color format used
 * @param data pixel buffer
 * @return new bitmap
 */

HGDI_BITMAP gdi_CreateBitmap(UINT32 nWidth, UINT32 nHeight, UINT32 format, BYTE* data)
{
	return gdi_CreateBitmapEx(nWidth, nHeight, format, 0, data, winpr_aligned_free);
}

/**
 * Create a new bitmap with the given width, height, color format and pixel buffer. msdn{dd183485}
 *
 * @param nWidth width
 * @param nHeight height
 * @param format the color format used
 * @param data pixel buffer
 * @param fkt_free The function used for deallocation of the buffer, NULL for none.
 * @return new bitmap
 */

HGDI_BITMAP gdi_CreateBitmapEx(UINT32 nWidth, UINT32 nHeight, UINT32 format, UINT32 stride,
                               BYTE* data, void (*fkt_free)(void*))
{
	HGDI_BITMAP hBitmap = (HGDI_BITMAP)calloc(1, sizeof(GDI_BITMAP));

	if (!hBitmap)
		return NULL;

	hBitmap->objectType = GDIOBJECT_BITMAP;
	hBitmap->format = format;

	if (stride > 0)
		hBitmap->scanline = stride;
	else
		hBitmap->scanline = nWidth * FreeRDPGetBytesPerPixel(hBitmap->format);

	hBitmap->width = nWidth;
	hBitmap->height = nHeight;
	hBitmap->data = data;
	hBitmap->free = fkt_free;
	return hBitmap;
}

/**
 * Create a new bitmap of the given width and height compatible with the current device context.
 * msdn{dd183488}
 *
 * @param hdc device context
 * @param nWidth width
 * @param nHeight height
 *
 * @return new bitmap
 */

HGDI_BITMAP gdi_CreateCompatibleBitmap(HGDI_DC hdc, UINT32 nWidth, UINT32 nHeight)
{
	HGDI_BITMAP hBitmap = (HGDI_BITMAP)calloc(1, sizeof(GDI_BITMAP));

	if (!hBitmap)
		return NULL;

	hBitmap->objectType = GDIOBJECT_BITMAP;
	hBitmap->format = hdc->format;
	hBitmap->width = nWidth;
	hBitmap->height = nHeight;
	hBitmap->data = winpr_aligned_malloc(
	    nWidth * nHeight * FreeRDPGetBytesPerPixel(hBitmap->format) * 1ULL, 16);
	hBitmap->free = winpr_aligned_free;

	if (!hBitmap->data)
	{
		free(hBitmap);
		return NULL;
	}

	hBitmap->scanline = nWidth * FreeRDPGetBytesPerPixel(hBitmap->format);
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

static UINT32 process_rop(UINT32 src, UINT32 dst, UINT32 pat, const char* rop, UINT32 format)
{
	UINT32 stack[10] = { 0 };
	UINT32 stackp = 0;

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

static INLINE BOOL BitBlt_write(HGDI_DC hdcDest, HGDI_DC hdcSrc, INT32 nXDest, INT32 nYDest,
                                INT32 nXSrc, INT32 nYSrc, INT32 x, INT32 y, BOOL useSrc,
                                BOOL usePat, UINT32 style, const char* rop,
                                const gdiPalette* palette)
{
	UINT32 dstColor;
	UINT32 colorA;
	UINT32 colorB = 0;
	UINT32 colorC = 0;
	const INT32 dstX = nXDest + x;
	const INT32 dstY = nYDest + y;
	BYTE* dstp = gdi_get_bitmap_pointer(hdcDest, dstX, dstY);

	if (!dstp)
	{
		WLog_ERR(TAG, "dstp=%p", (const void*)dstp);
		return FALSE;
	}

	colorA = FreeRDPReadColor(dstp, hdcDest->format);

	if (useSrc)
	{
		const BYTE* srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc + x, nYSrc + y);

		if (!srcp)
		{
			WLog_ERR(TAG, "srcp=%p", (const void*)srcp);
			return FALSE;
		}

		colorC = FreeRDPReadColor(srcp, hdcSrc->format);
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
					WLog_ERR(TAG, "patp=%p", (const void*)patp);
					return FALSE;
				}

				colorB = FreeRDPReadColor(patp, hdcDest->format);
			}
			break;

			default:
				break;
		}
	}

	dstColor = process_rop(colorC, colorA, colorB, rop, hdcDest->format);
	return FreeRDPWriteColor(dstp, hdcDest->format, dstColor);
}

static BOOL adjust_src_coordinates(HGDI_DC hdcSrc, INT32 nWidth, INT32 nHeight, INT32* px,
                                   INT32* py)
{
	HGDI_BITMAP hSrcBmp;
	INT32 nXSrc, nYSrc;

	if (!hdcSrc || (nWidth < 0) || (nHeight < 0) || !px || !py)
		return FALSE;

	hSrcBmp = (HGDI_BITMAP)hdcSrc->selectedObject;
	nXSrc = *px;
	nYSrc = *py;

	if (!hSrcBmp)
		return FALSE;

	if (nYSrc < 0)
	{
		nYSrc = 0;
		nHeight = nHeight + nYSrc;
	}

	if ((nXSrc) < 0)
	{
		nXSrc = 0;
		nWidth = nWidth + nXSrc;
	}

	if (hSrcBmp->width < (nXSrc + nWidth))
		nXSrc = hSrcBmp->width - nWidth;

	if (hSrcBmp->height < (nYSrc + nHeight))
		nYSrc = hSrcBmp->height - nHeight;

	if ((nXSrc < 0) || (nYSrc < 0))
		return FALSE;

	*px = nXSrc;
	*py = nYSrc;
	return TRUE;
}

static BOOL adjust_src_dst_coordinates(HGDI_DC hdcDest, INT32* pnXSrc, INT32* pnYSrc, INT32* pnXDst,
                                       INT32* pnYDst, INT32* pnWidth, INT32* pnHeight)
{
	HGDI_BITMAP hDstBmp;
	volatile INT32 diffX, diffY;
	volatile INT32 nXSrc, nYSrc;
	volatile INT32 nXDst, nYDst, nWidth, nHeight;

	if (!hdcDest || !pnXSrc || !pnYSrc || !pnXDst || !pnYDst || !pnWidth || !pnHeight)
		return FALSE;

	hDstBmp = (HGDI_BITMAP)hdcDest->selectedObject;
	nXSrc = *pnXSrc;
	nYSrc = *pnYSrc;
	nXDst = *pnXDst;
	nYDst = *pnYDst;
	nWidth = *pnWidth;
	nHeight = *pnHeight;

	if (!hDstBmp)
		return FALSE;

	if (nXDst < 0)
	{
		nXSrc -= nXDst;
		nWidth += nXDst;
		nXDst = 0;
	}

	if (nYDst < 0)
	{
		nYSrc -= nYDst;
		nHeight += nYDst;
		nYDst = 0;
	}

	diffX = hDstBmp->width - nXDst - nWidth;

	if (diffX < 0)
		nWidth += diffX;

	diffY = hDstBmp->height - nYDst - nHeight;

	if (diffY < 0)
		nHeight += diffY;

	if ((nXDst < 0) || (nYDst < 0) || (nWidth < 0) || (nHeight < 0))
	{
		nXDst = 0;
		nYDst = 0;
		nWidth = 0;
		nHeight = 0;
	}

	*pnXSrc = nXSrc;
	*pnYSrc = nYSrc;
	*pnXDst = nXDst;
	*pnYDst = nYDst;
	*pnWidth = nWidth;
	*pnHeight = nHeight;
	return TRUE;
}

static BOOL BitBlt_process(HGDI_DC hdcDest, INT32 nXDest, INT32 nYDest, INT32 nWidth, INT32 nHeight,
                           HGDI_DC hdcSrc, INT32 nXSrc, INT32 nYSrc, const char* rop,
                           const gdiPalette* palette)
{
	INT32 x, y;
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

	if (!adjust_src_dst_coordinates(hdcDest, &nXSrc, &nYSrc, &nXDest, &nYDest, &nWidth, &nHeight))
		return FALSE;

	if (useSrc && !hdcSrc)
		return FALSE;

	if (useSrc)
	{
		if (!adjust_src_coordinates(hdcSrc, nWidth, nHeight, &nXSrc, &nYSrc))
			return FALSE;
	}

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
		for (y = nHeight - 1; y >= 0; y--)
		{
			for (x = nWidth - 1; x >= 0; x--)
			{
				if (!BitBlt_write(hdcDest, hdcSrc, nXDest, nYDest, nXSrc, nYSrc, x, y, useSrc,
				                  usePat, style, rop, palette))
					return FALSE;
			}
		}
	}
	else if (nXDest > nXSrc)
	{
		for (y = 0; y < nHeight; y++)
		{
			for (x = nWidth - 1; x >= 0; x--)
			{
				if (!BitBlt_write(hdcDest, hdcSrc, nXDest, nYDest, nXSrc, nYSrc, x, y, useSrc,
				                  usePat, style, rop, palette))
					return FALSE;
			}
		}
	}
	else if (nYDest > nYSrc)
	{
		for (y = nHeight - 1; y >= 0; y--)
		{
			for (x = 0; x < nWidth; x++)
			{
				if (!BitBlt_write(hdcDest, hdcSrc, nXDest, nYDest, nXSrc, nYSrc, x, y, useSrc,
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
				if (!BitBlt_write(hdcDest, hdcSrc, nXDest, nYDest, nXSrc, nYSrc, x, y, useSrc,
				                  usePat, style, rop, palette))
					return FALSE;
			}
		}
	}

	return TRUE;
}

/**
 * Perform a bit blit operation on the given pixel buffers.
 * msdn{dd183370}
 *
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
BOOL gdi_BitBlt(HGDI_DC hdcDest, INT32 nXDest, INT32 nYDest, INT32 nWidth, INT32 nHeight,
                HGDI_DC hdcSrc, INT32 nXSrc, INT32 nYSrc, DWORD rop, const gdiPalette* palette)
{
	HGDI_BITMAP hSrcBmp, hDstBmp;

	if (!hdcDest)
		return FALSE;

	if (!gdi_ClipCoords(hdcDest, &nXDest, &nYDest, &nWidth, &nHeight, &nXSrc, &nYSrc))
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
			if (!hdcSrc)
				return FALSE;

			if (!adjust_src_dst_coordinates(hdcDest, &nXSrc, &nYSrc, &nXDest, &nYDest, &nWidth,
			                                &nHeight))
				return FALSE;

			if (!adjust_src_coordinates(hdcSrc, nWidth, nHeight, &nXSrc, &nYSrc))
				return FALSE;

			hSrcBmp = (HGDI_BITMAP)hdcSrc->selectedObject;
			hDstBmp = (HGDI_BITMAP)hdcDest->selectedObject;

			if (!hSrcBmp || !hDstBmp)
				return FALSE;

			if (!freerdp_image_copy(hDstBmp->data, hDstBmp->format, hDstBmp->scanline, nXDest,
			                        nYDest, nWidth, nHeight, hSrcBmp->data, hSrcBmp->format,
			                        hSrcBmp->scanline, nXSrc, nYSrc, palette, FREERDP_FLIP_NONE))
				return FALSE;

			break;

		case GDI_DSTCOPY:
			hSrcBmp = (HGDI_BITMAP)hdcDest->selectedObject;
			hDstBmp = (HGDI_BITMAP)hdcDest->selectedObject;

			if (!adjust_src_dst_coordinates(hdcDest, &nXSrc, &nYSrc, &nXDest, &nYDest, &nWidth,
			                                &nHeight))
				return FALSE;

			if (!adjust_src_coordinates(hdcDest, nWidth, nHeight, &nXSrc, &nYSrc))
				return FALSE;

			if (!hSrcBmp || !hDstBmp)
				return FALSE;

			if (!freerdp_image_copy(hDstBmp->data, hDstBmp->format, hDstBmp->scanline, nXDest,
			                        nYDest, nWidth, nHeight, hSrcBmp->data, hSrcBmp->format,
			                        hSrcBmp->scanline, nXSrc, nYSrc, palette, FREERDP_FLIP_NONE))
				return FALSE;

			break;

		default:
			if (!BitBlt_process(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc,
			                    gdi_rop_to_string(rop), palette))
				return FALSE;

			break;
	}

	if (!gdi_InvalidateRegion(hdcDest, nXDest, nYDest, nWidth, nHeight))
		return FALSE;

	return TRUE;
}
