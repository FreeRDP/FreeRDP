/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI 32bpp Internal Buffer Routines
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
#include <freerdp/log.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>

#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/clipping.h>
#include <freerdp/gdi/drawing.h>

#include <freerdp/gdi/32bpp.h>

#define TAG FREERDP_TAG("gdi")

UINT32 gdi_get_color_32bpp(HGDI_DC hdc, GDI_COLOR color)
{
	UINT32 color32;
	BYTE a, r, g, b;

	a = 0xFF;
	GetRGB32(r, g, b, color);

	if (hdc->invert)
	{
		color32 = ABGR32(a, r, g, b);
	}
	else
	{
		color32 = ARGB32(a, r, g, b);
	}

	return color32;
}

BOOL FillRect_32bpp(HGDI_DC hdc, HGDI_RECT rect, HGDI_BRUSH hbr)
{
	int x, y;
	UINT32 *dstp;
	UINT32 color32;
	int nXDest, nYDest;
	int nWidth, nHeight;

	gdi_RectToCRgn(rect, &nXDest, &nYDest, &nWidth, &nHeight);
	
	if (!gdi_ClipCoords(hdc, &nXDest, &nYDest, &nWidth, &nHeight, NULL, NULL))
		return TRUE;

	color32 = gdi_get_color_32bpp(hdc, hbr->color);

	for (y = 0; y < nHeight; y++)
	{
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdc, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = color32;
				dstp++;
			}
		}
	}

	if (!gdi_InvalidateRegion(hdc, nXDest, nYDest, nWidth, nHeight))
		return FALSE;

	return TRUE;
}

static BOOL BitBlt_BLACKNESS_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	if (hdcDest->alpha)
	{
		int x, y;
		BYTE* dstp;

		for (y = 0; y < nHeight; y++)
		{
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp = 0;
					dstp++;

					*dstp = 0;
					dstp++;

					*dstp = 0;
					dstp++;

					*dstp = 0xFF;
					dstp++;
				}
			}
		}
	}
	else
	{
		int y;
		BYTE* dstp;

		for (y = 0; y < nHeight; y++)
		{
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
				memset(dstp, 0, nWidth * hdcDest->bytesPerPixel);
		}
	}

	return TRUE;
}

static BOOL BitBlt_WHITENESS_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int y;
	BYTE* dstp;
	
	for (y = 0; y < nHeight; y++)
	{
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
			memset(dstp, 0xFF, nWidth * hdcDest->bytesPerPixel);
	}

	return TRUE;
}

static BOOL BitBlt_SRCCOPY_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int y;
	BYTE* srcp;
	BYTE* dstp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	if ((hdcDest->selectedObject != hdcSrc->selectedObject) ||
	    !gdi_CopyOverlap(nXDest, nYDest, nWidth, nHeight, nXSrc, nYSrc))
	{
		for (y = 0; y < nHeight; y++)
		{
			srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (srcp != 0 && dstp != 0)
				memmove(dstp, srcp, nWidth * hdcDest->bytesPerPixel);
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
				memmove(dstp, srcp, nWidth * hdcDest->bytesPerPixel);
		}
	}
	else if (nYSrc > nYDest || nXSrc > nXDest)
	{
		/* copy up or left (top top bottom) */
		for (y = 0; y < nHeight; y++)
		{
			srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (srcp != 0 && dstp != 0)
				memmove(dstp, srcp, nWidth * hdcDest->bytesPerPixel);
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
				memmove(dstp, srcp, nWidth * hdcDest->bytesPerPixel);
		}
	}

	return TRUE;
}

static BOOL BitBlt_NOTSRCCOPY_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = ~(*srcp);
				srcp++;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSTINVERT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	UINT32* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = ~(*dstp);
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCERASE_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = *srcp & ~(*dstp);
				srcp++;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_NOTSRCERASE_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = ~(*srcp) & ~(*dstp);
				srcp++;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCINVERT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp ^= *srcp;
				srcp++;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCAND_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp &= *srcp;
				srcp++;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCPAINT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;
		
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp |= *srcp;
				srcp++;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSPDxax_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{	
	int x, y;
	UINT32* srcp;
	UINT32* dstp;
	UINT32* patp;
	BYTE* srcp8;
	UINT32 src32;
	UINT32 color32;
	HGDI_BITMAP hSrcBmp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	/* D = (S & P) | (~S & D) */

	color32 = gdi_get_color_32bpp(hdcDest, hdcDest->textColor);

	patp = (UINT32*) &color32;
	hSrcBmp = (HGDI_BITMAP) hdcSrc->selectedObject;

	if (hdcSrc->bytesPerPixel == 1)
	{
		/* DSPDxax, used to draw glyphs */

		srcp = (UINT32*) & src32;

		for (y = 0; y < nHeight; y++)
		{
			srcp8 = (BYTE*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*srcp = ((*srcp8) | (*srcp8 << 8) | (*srcp8 << 16) | (*srcp8 << 24));

					*dstp = (*srcp & *patp) | (~(*srcp) & *dstp);
					dstp++;

					srcp8++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp = (*srcp & *patp) | (~(*srcp) & *dstp);
					srcp++;
					dstp++;
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PSDPxax_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;
	UINT32* patp;
	UINT32 color32;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	/* D = (S & D) | (~S & P) */

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color32 = gdi_get_color_32bpp(hdcDest, hdcDest->brush->color);
		patp = (UINT32*) &color32;

		for (y = 0; y < nHeight; y++)
		{
			srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp = (*srcp & *dstp) | (~(*srcp) & *patp);
					srcp++;
					dstp++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
					*dstp = (*srcp & *dstp) | (~(*srcp) & *patp);
					srcp++;
					dstp++;
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SPDSxax_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;
	UINT32* patp;
	UINT32 color32;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	/* D = S ^ (P & (D ^ S)) */

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color32 = gdi_get_color_32bpp(hdcDest, hdcDest->brush->color);
		patp = (UINT32*) &color32;

		for (y = 0; y < nHeight; y++)
		{
			srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp = *srcp ^ (*patp & (*dstp ^ *srcp));
					srcp++;
					dstp++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
					*dstp = *srcp ^ (*patp & (*dstp ^ *srcp));
					srcp++;
					dstp++;
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SPna_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;
	UINT32* patp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
				
				*dstp = *srcp & ~(*patp);
				srcp++;
				dstp++;
			}
		}
	}
	
	return TRUE;
}

static BOOL BitBlt_DSna_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = ~(*srcp) & (*dstp);
				srcp++;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DPa_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	UINT32* dstp;
	UINT32* patp;

	for (y = 0; y < nHeight; y++)
	{
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);

				*dstp = *dstp & *patp;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PDxn_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	UINT32* dstp;
	UINT32* patp;

	for (y = 0; y < nHeight; y++)
	{
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);

				*dstp = *dstp ^ ~(*patp);
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_MERGECOPY_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;
	UINT32* patp;
	
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
				
				*dstp = *srcp & *patp;
				srcp++;
				dstp++;
			}
		}
	}
	
	return TRUE;
}

static BOOL BitBlt_MERGEPAINT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;
	
	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{				
				*dstp = ~(*srcp) | *dstp;
				srcp++;
				dstp++;
			}
		}
	}
	
	return TRUE;
}

static BOOL BitBlt_PATCOPY_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y, xOffset, yOffset;
	UINT32* dstp;
	UINT32* patp;
	UINT32 color32;

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color32 = gdi_get_color_32bpp(hdcDest, hdcDest->brush->color);

		for (y = 0; y < nHeight; y++)
		{
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp = color32;
					dstp++;
				}
			}
		}
	}
	else
	{
		if (hdcDest->brush->style == GDI_BS_HATCHED)
		{
			xOffset = 0;
			yOffset = 2; // +2 added after comparison to mstsc
		}
		else
		{
			xOffset = 0;
			yOffset = 0;
		}
		for (y = 0; y < nHeight; y++)
		{
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x + xOffset, nYDest + y + yOffset);
					*dstp = *patp;
					dstp++;
				}
			}
		}
	}
	
	return TRUE;
}

static BOOL BitBlt_PATINVERT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	UINT32* dstp;
	UINT32* patp;
	UINT32 color32;
		
	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color32 = gdi_get_color_32bpp(hdcDest, hdcDest->brush->color);

		for (y = 0; y < nHeight; y++)
		{
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp ^= color32;
					dstp++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
					*dstp = *patp ^ *dstp;
					dstp++;
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PATPAINT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	UINT32* srcp;
	UINT32* dstp;
	UINT32* patp;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (UINT32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (UINT32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (UINT32*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
				*dstp = *dstp | (*patp | ~(*srcp));
				srcp++;
				dstp++;
			}
		}
	}
	
	return TRUE;
}

BOOL BitBlt_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc, DWORD rop)
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
			return BitBlt_BLACKNESS_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_WHITENESS:
			return BitBlt_WHITENESS_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_SRCCOPY:
			return BitBlt_SRCCOPY_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SPna:
			return BitBlt_SPna_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSna:
			return BitBlt_DSna_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSPDxax:
			return BitBlt_DSPDxax_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_PSDPxax:
			return BitBlt_PSDPxax_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SPDSxax:
			return BitBlt_SPDSxax_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_NOTSRCCOPY:
			return BitBlt_NOTSRCCOPY_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_SRCERASE:
			return BitBlt_SRCERASE_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_NOTSRCERASE:
			return BitBlt_NOTSRCERASE_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCINVERT:
			return BitBlt_SRCINVERT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCAND:
			return BitBlt_SRCAND_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCPAINT:
			return BitBlt_SRCPAINT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_MERGECOPY:
			return BitBlt_MERGECOPY_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_MERGEPAINT:
			return BitBlt_MERGEPAINT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_PATCOPY:
			return BitBlt_PATCOPY_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_PATINVERT:
			return BitBlt_PATINVERT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_PATPAINT:
			return BitBlt_PATPAINT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
	}

	WLog_ERR(TAG,  "BitBlt: unknown rop: 0x%08X", rop);
	return FALSE;
}

BOOL PatBlt_32bpp(HGDI_DC hdc, int nXLeft, int nYLeft, int nWidth, int nHeight, DWORD rop)
{
	if (!gdi_ClipCoords(hdc, &nXLeft, &nYLeft, &nWidth, &nHeight, NULL, NULL))
		return TRUE;
	
	if (!gdi_InvalidateRegion(hdc, nXLeft, nYLeft, nWidth, nHeight))
		return FALSE;

	switch (rop)
	{
		case GDI_PATCOPY:
			return BitBlt_PATCOPY_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_PATINVERT:
			return BitBlt_PATINVERT_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_BLACKNESS:
			return BitBlt_BLACKNESS_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_WHITENESS:
			return BitBlt_WHITENESS_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_DPa:
			return BitBlt_DPa_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_PDxn:
			return BitBlt_PDxn_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		default:
			break;
	}

	WLog_ERR(TAG,  "PatBlt: unknown rop: 0x%08X", rop);
	return FALSE;
}

static INLINE void SetPixel_BLACK_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = 0 */
	*pixel = 0;
}

static INLINE void SetPixel_NOTMERGEPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = ~(D | P) */
	*pixel = ~(*pixel | *pen);
}

static INLINE void SetPixel_MASKNOTPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = D & ~P */
	*pixel &= ~(*pen);
}

static INLINE void SetPixel_NOTCOPYPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = ~P */
	*pixel = ~(*pen);
}

static INLINE void SetPixel_MASKPENNOT_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = P & ~D */
	*pixel = *pen & ~*pixel;
}

static INLINE void SetPixel_NOT_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = ~D */
	*pixel = ~(*pixel);
}

static INLINE void SetPixel_XORPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = D ^ P */
	*pixel = *pixel ^ *pen;
}

static INLINE void SetPixel_NOTMASKPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = ~(D & P) */
	*pixel = ~(*pixel & *pen);
}

static INLINE void SetPixel_MASKPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = D & P */
	*pixel &= *pen;
}

static INLINE void SetPixel_NOTXORPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = ~(D ^ P) */
	*pixel = ~(*pixel ^ *pen);
}

static INLINE void SetPixel_NOP_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = D */
}

static INLINE void SetPixel_MERGENOTPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = D | ~P */
	*pixel |= ~(*pen);
}

static INLINE void SetPixel_COPYPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = P */
	*pixel = *pen;
}

static INLINE void SetPixel_MERGEPENNOT_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = P | ~D */
	*pixel = *pen | ~(*pixel);
}

static INLINE void SetPixel_MERGEPEN_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = P | D */
	*pixel |= *pen;
}

static INLINE void SetPixel_WHITE_32bpp(UINT32* pixel, UINT32* pen)
{
	/* D = 1 */
	*pixel = 0xFFFFFF;
}

#define PIXEL_TYPE		UINT32
#define GDI_GET_POINTER		gdi_GetPointer_32bpp
#define GDI_GET_PEN_COLOR	gdi_GetPenColor_32bpp

#define LINE_TO			LineTo_BLACK_32bpp
#define SET_PIXEL_ROP2		SetPixel_BLACK_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTMERGEPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_NOTMERGEPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKNOTPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_MASKNOTPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTCOPYPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_NOTCOPYPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKPENNOT_32bpp
#define SET_PIXEL_ROP2		SetPixel_MASKPENNOT_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOT_32bpp
#define SET_PIXEL_ROP2		SetPixel_NOT_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_XORPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_XORPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTMASKPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_NOTMASKPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_MASKPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTXORPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_NOTXORPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOP_32bpp
#define SET_PIXEL_ROP2		SetPixel_NOP_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGENOTPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_MERGENOTPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_COPYPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_COPYPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGEPENNOT_32bpp
#define SET_PIXEL_ROP2		SetPixel_MERGEPENNOT_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGEPEN_32bpp
#define SET_PIXEL_ROP2		SetPixel_MERGEPEN_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_WHITE_32bpp
#define SET_PIXEL_ROP2		SetPixel_WHITE_32bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#undef PIXEL_TYPE
#undef GDI_GET_POINTER
#undef GDI_GET_PEN_COLOR

pLineTo_32bpp LineTo_ROP2_32bpp[32] =
{
	LineTo_BLACK_32bpp,
	LineTo_NOTMERGEPEN_32bpp,
	LineTo_MASKNOTPEN_32bpp,
	LineTo_NOTCOPYPEN_32bpp,
	LineTo_MASKPENNOT_32bpp,
	LineTo_NOT_32bpp,
	LineTo_XORPEN_32bpp,
	LineTo_NOTMASKPEN_32bpp,
	LineTo_MASKPEN_32bpp,
	LineTo_NOTXORPEN_32bpp,
	LineTo_NOP_32bpp,
	LineTo_MERGENOTPEN_32bpp,
	LineTo_COPYPEN_32bpp,
	LineTo_MERGEPENNOT_32bpp,
	LineTo_MERGEPEN_32bpp,
	LineTo_WHITE_32bpp
};

BOOL LineTo_32bpp(HGDI_DC hdc, int nXEnd, int nYEnd)
{
	pLineTo_32bpp _LineTo;
	int rop2 = gdi_GetROP2(hdc) - 1;

	_LineTo = LineTo_ROP2_32bpp[rop2];

	if (_LineTo == NULL)
		return FALSE;

	return _LineTo(hdc, nXEnd, nYEnd);
}
