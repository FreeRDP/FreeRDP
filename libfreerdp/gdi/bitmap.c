/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Bitmap Functions
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/gdi/32bpp.h>
#include <freerdp/gdi/16bpp.h>
#include <freerdp/gdi/8bpp.h>

#include <freerdp/gdi/bitmap.h>

p_BitBlt BitBlt_[5] =
{
	NULL,
	BitBlt_8bpp,
	BitBlt_16bpp,
	NULL,
	BitBlt_32bpp
};

/**
 * Get pixel at the given coordinates.\n
 * @msdn{dd144909}
 * @param hdc device context
 * @param nXPos pixel x position
 * @param nYPos pixel y position
 * @return pixel color
 */

INLINE GDI_COLOR gdi_GetPixel(HGDI_DC hdc, int nXPos, int nYPos)
{
	HGDI_BITMAP hBmp = (HGDI_BITMAP) hdc->selectedObject;
	GDI_COLOR* colorp = (GDI_COLOR*)&(hBmp->data[(nYPos * hBmp->width * hdc->bytesPerPixel) + nXPos * hdc->bytesPerPixel]);
	return (GDI_COLOR) *colorp;
}

INLINE BYTE gdi_GetPixel_8bpp(HGDI_BITMAP hBmp, int X, int Y)
{
	return *((BYTE*)&(hBmp->data[(Y * hBmp->width) + X]));
}

INLINE UINT16 gdi_GetPixel_16bpp(HGDI_BITMAP hBmp, int X, int Y)
{
	return *((UINT16*)&(hBmp->data[(Y * hBmp->width * 2) + X * 2]));
}

INLINE UINT32 gdi_GetPixel_32bpp(HGDI_BITMAP hBmp, int X, int Y)
{
	return *((UINT32*)&(hBmp->data[(Y * hBmp->width * 4) + X * 4]));
}

INLINE BYTE* gdi_GetPointer_8bpp(HGDI_BITMAP hBmp, int X, int Y)
{
	return ((BYTE*)&(hBmp->data[(Y * hBmp->width) + X]));
}

INLINE UINT16* gdi_GetPointer_16bpp(HGDI_BITMAP hBmp, int X, int Y)
{
	return ((UINT16*)&(hBmp->data[(Y * hBmp->width * 2) + X * 2]));
}

INLINE UINT32* gdi_GetPointer_32bpp(HGDI_BITMAP hBmp, int X, int Y)
{
	return ((UINT32*)&(hBmp->data[(Y * hBmp->width * 4) + X * 4]));
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

INLINE GDI_COLOR gdi_SetPixel(HGDI_DC hdc, int X, int Y, GDI_COLOR crColor)
{
	HGDI_BITMAP hBmp = (HGDI_BITMAP) hdc->selectedObject;
	*((GDI_COLOR*)&(hBmp->data[(Y * hBmp->width * hdc->bytesPerPixel) + X * hdc->bytesPerPixel])) = crColor;
	return crColor;
}

INLINE void gdi_SetPixel_8bpp(HGDI_BITMAP hBmp, int X, int Y, BYTE pixel)
{
	*((BYTE*)&(hBmp->data[(Y * hBmp->width) + X])) = pixel;
}

INLINE void gdi_SetPixel_16bpp(HGDI_BITMAP hBmp, int X, int Y, UINT16 pixel)
{
	*((UINT16*)&(hBmp->data[(Y * hBmp->width * 2) + X * 2])) = pixel;
}

INLINE void gdi_SetPixel_32bpp(HGDI_BITMAP hBmp, int X, int Y, UINT32 pixel)
{
	*((UINT32*)&(hBmp->data[(Y * hBmp->width * 4) + X * 4])) = pixel;
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

HGDI_BITMAP gdi_CreateBitmap(int nWidth, int nHeight, int cBitsPerPixel, BYTE* data)
{
	return gdi_CreateBitmapEx(nWidth, nHeight, cBitsPerPixel, data, _aligned_free);
}

HGDI_BITMAP gdi_CreateBitmapEx(int nWidth, int nHeight, int cBitsPerPixel, BYTE* data,
							void (*fkt_free)(void*))
{
	HGDI_BITMAP hBitmap = (HGDI_BITMAP) calloc(1, sizeof(GDI_BITMAP));

	if (!hBitmap)
		return NULL;

	hBitmap->objectType = GDIOBJECT_BITMAP;
	hBitmap->bitsPerPixel = cBitsPerPixel;
	hBitmap->bytesPerPixel = (cBitsPerPixel + 1) / 8;
	hBitmap->scanline = nWidth * hBitmap->bytesPerPixel;
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

HGDI_BITMAP gdi_CreateCompatibleBitmap(HGDI_DC hdc, int nWidth, int nHeight)
{
	HGDI_BITMAP hBitmap = (HGDI_BITMAP) calloc(1, sizeof(GDI_BITMAP));

	if (!hBitmap)
		return NULL;

	hBitmap->objectType = GDIOBJECT_BITMAP;
	hBitmap->bytesPerPixel = hdc->bytesPerPixel;
	hBitmap->bitsPerPixel = hdc->bitsPerPixel;
	hBitmap->width = nWidth;
	hBitmap->height = nHeight;
	hBitmap->data = _aligned_malloc(nWidth * nHeight * hBitmap->bytesPerPixel, 16);
	hBitmap->free = _aligned_free;
	if (!hBitmap->data)
	{
		free(hBitmap);
		return NULL;
	}
	hBitmap->scanline = nWidth * hBitmap->bytesPerPixel;

	return hBitmap;
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

BOOL gdi_BitBlt(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc, DWORD rop)
{
	p_BitBlt _BitBlt = BitBlt_[IBPP(hdcDest->bitsPerPixel)];

	if (_BitBlt == NULL)
		return FALSE;

	return _BitBlt(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc, rop);
}
