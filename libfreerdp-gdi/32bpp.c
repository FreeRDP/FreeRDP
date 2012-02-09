/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>

#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/clipping.h>
#include <freerdp/gdi/drawing.h>

#include <freerdp/gdi/32bpp.h>

uint32 gdi_get_color_32bpp(HGDI_DC hdc, GDI_COLOR color)
{
	uint32 color32;
	uint8 a, r, g, b;

	a = 0xFF;
	GetBGR32(r, g, b, color);

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

int FillRect_32bpp(HGDI_DC hdc, HGDI_RECT rect, HGDI_BRUSH hbr)
{
	int x, y;
	uint32 *dstp;
	uint32 color32;
	int nXDest, nYDest;
	int nWidth, nHeight;

	gdi_RectToCRgn(rect, &nXDest, &nYDest, &nWidth, &nHeight);
	
	if (gdi_ClipCoords(hdc, &nXDest, &nYDest, &nWidth, &nHeight, NULL, NULL) == 0)
		return 0;

	color32 = gdi_get_color_32bpp(hdc, hbr->color);

	for (y = 0; y < nHeight; y++)
	{
		dstp = (uint32*) gdi_get_bitmap_pointer(hdc, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = color32;
				dstp++;
			}
		}
	}

	gdi_InvalidateRegion(hdc, nXDest, nYDest, nWidth, nHeight);
	return 0;
}

static int BitBlt_BLACKNESS_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	if (hdcDest->alpha)
	{
		int x, y;
		uint8* dstp;

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
		uint8* dstp;

		for (y = 0; y < nHeight; y++)
		{
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
				memset(dstp, 0, nWidth * hdcDest->bytesPerPixel);
		}
	}

	return 0;
}

static int BitBlt_WHITENESS_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int y;
	uint8* dstp;
	
	for (y = 0; y < nHeight; y++)
	{
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
			memset(dstp, 0xFF, nWidth * hdcDest->bytesPerPixel);
	}

	return 0;
}

static int BitBlt_SRCCOPY_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int y;
	uint8* srcp;
	uint8* dstp;

	if ((hdcDest->selectedObject != hdcSrc->selectedObject) ||
	    gdi_CopyOverlap(nXDest, nYDest, nWidth, nHeight, nXSrc, nYSrc) == 0)
	{
		for (y = 0; y < nHeight; y++)
		{
			srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (srcp != 0 && dstp != 0)
				memcpy(dstp, srcp, nWidth * hdcDest->bytesPerPixel);
		}

		return 0;
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

	return 0;
}

static int BitBlt_NOTSRCCOPY_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

	return 0;
}

static int BitBlt_DSTINVERT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	uint32* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = ~(*dstp);
				dstp++;
			}
		}
	}

	return 0;
}

static int BitBlt_SRCERASE_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

	return 0;
}

static int BitBlt_NOTSRCERASE_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

	return 0;
}

static int BitBlt_SRCINVERT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

	return 0;
}

static int BitBlt_SRCAND_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

	return 0;
}

static int BitBlt_SRCPAINT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

	return 0;
}

static int BitBlt_DSPDxax_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{	
	int x, y;
	uint8* srcp;
	uint8* dstp;
	uint8* patp;
	uint32 color32;
	HGDI_BITMAP hSrcBmp;

	/* D = (S & P) | (~S & D) */
	/* DSPDxax, used to draw glyphs */

	color32 = gdi_get_color_32bpp(hdcDest, hdcDest->textColor);

	hSrcBmp = (HGDI_BITMAP) hdcSrc->selectedObject;
	srcp = hSrcBmp->data;

	if (hdcSrc->bytesPerPixel != 1)
	{
		printf("BitBlt_DSPDxax expects 1 bpp, unimplemented for %d\n", hdcSrc->bytesPerPixel);
		return 0;
	}
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint8*) &color32;

				*dstp = (*srcp & *patp) | (~(*srcp) & *dstp);
				dstp++;
				patp++;

				*dstp = (*srcp & *patp) | (~(*srcp) & *dstp);
				dstp++;
				patp++;

				*dstp = (*srcp & *patp) | (~(*srcp) & *dstp);
				dstp += 2;
				srcp++;
			}
		}
	}

	return 0;
}

static int BitBlt_SPna_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
	uint32* patp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint32*) gdi_get_brush_pointer(hdcDest, x, y);
				
				*dstp = *srcp & ~(*patp);
				srcp++;
				dstp++;
			}
		}
	}
	
	return 0;
}

static int BitBlt_DSna_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

	return 0;
}

static int BitBlt_PDxn_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	uint32* dstp;
	uint32* patp;

	for (y = 0; y < nHeight; y++)
	{
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint32*) gdi_get_brush_pointer(hdcDest, x, y);

				*dstp = *dstp ^ ~(*patp);
				dstp++;
			}
		}
	}

	return 0;
}

static int BitBlt_MERGECOPY_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
	uint32* patp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint32*) gdi_get_brush_pointer(hdcDest, x, y);
				
				*dstp = *srcp & *patp;
				srcp++;
				dstp++;
			}
		}
	}
	
	return 0;
}

static int BitBlt_MERGEPAINT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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
	
	return 0;
}

static int BitBlt_PATCOPY_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	uint32* dstp;
	uint32* patp;
	uint32 color32;

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color32 = gdi_get_color_32bpp(hdcDest, hdcDest->brush->color);

		for (y = 0; y < nHeight; y++)
		{
			dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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
		for (y = 0; y < nHeight; y++)
		{
			dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (uint32*) gdi_get_brush_pointer(hdcDest, x, y);
					*dstp = *patp;
					dstp++;
				}
			}
		}
	}
	
	return 0;
}

static int BitBlt_PATINVERT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	uint32* dstp;
	uint32* patp;
	uint32 color32;
		
	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color32 = gdi_get_color_32bpp(hdcDest, hdcDest->brush->color);

		for (y = 0; y < nHeight; y++)
		{
			dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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
			dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (uint32*) gdi_get_brush_pointer(hdcDest, x, y);
					*dstp = *patp ^ *dstp;
					dstp++;
				}
			}
		}
	}
	
	return 0;
}

static int BitBlt_PATPAINT_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint32* srcp;
	uint32* dstp;
	uint32* patp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint32*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint32*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint32*) gdi_get_brush_pointer(hdcDest, x, y);
				*dstp = *dstp | (*patp | ~(*srcp));
				srcp++;
				dstp++;
			}
		}
	}
	
	return 0;
}

int BitBlt_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc, int rop)
{
	if (hdcSrc != NULL)
	{
		if (gdi_ClipCoords(hdcDest, &nXDest, &nYDest, &nWidth, &nHeight, &nXSrc, &nYSrc) == 0)
			return 0;
	}
	else
	{
		if (gdi_ClipCoords(hdcDest, &nXDest, &nYDest, &nWidth, &nHeight, NULL, NULL) == 0)
			return 0;
	}
	
	gdi_InvalidateRegion(hdcDest, nXDest, nYDest, nWidth, nHeight);
	
	switch (rop)
	{
		case GDI_BLACKNESS:
			return BitBlt_BLACKNESS_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_WHITENESS:
			return BitBlt_WHITENESS_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_SRCCOPY:
			return BitBlt_SRCCOPY_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_SPna:
			return BitBlt_SPna_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_DSna:
			return BitBlt_DSna_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_DSPDxax:
			return BitBlt_DSPDxax_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;
			
		case GDI_NOTSRCCOPY:
			return BitBlt_NOTSRCCOPY_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_SRCERASE:
			return BitBlt_SRCERASE_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_NOTSRCERASE:
			return BitBlt_NOTSRCERASE_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_SRCINVERT:
			return BitBlt_SRCINVERT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_SRCAND:
			return BitBlt_SRCAND_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_SRCPAINT:
			return BitBlt_SRCPAINT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_MERGECOPY:
			return BitBlt_MERGECOPY_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_MERGEPAINT:
			return BitBlt_MERGEPAINT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_PATCOPY:
			return BitBlt_PATCOPY_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_PATINVERT:
			return BitBlt_PATINVERT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_PATPAINT:
			return BitBlt_PATPAINT_32bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;
	}
	
	printf("BitBlt: unknown rop: 0x%08X\n", rop);
	return 1;
}

int PatBlt_32bpp(HGDI_DC hdc, int nXLeft, int nYLeft, int nWidth, int nHeight, int rop)
{
	if (gdi_ClipCoords(hdc, &nXLeft, &nYLeft, &nWidth, &nHeight, NULL, NULL) == 0)
		return 0;
	
	gdi_InvalidateRegion(hdc, nXLeft, nYLeft, nWidth, nHeight);

	switch (rop)
	{
		case GDI_PATCOPY:
			return BitBlt_PATCOPY_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		case GDI_PATINVERT:
			return BitBlt_PATINVERT_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;
			
		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		case GDI_BLACKNESS:
			return BitBlt_BLACKNESS_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		case GDI_WHITENESS:
			return BitBlt_WHITENESS_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		case GDI_PDxn:
			return BitBlt_PDxn_32bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		default:
			break;
	}
	
	printf("PatBlt: unknown rop: 0x%08X\n", rop);

	return 1;
}

INLINE void SetPixel_BLACK_32bpp(uint32* pixel, uint32* pen)
{
	/* D = 0 */
	*pixel = 0;
}

INLINE void SetPixel_NOTMERGEPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = ~(D | P) */
	*pixel = ~(*pixel | *pen);
}

INLINE void SetPixel_MASKNOTPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = D & ~P */
	*pixel &= ~(*pen);
}

INLINE void SetPixel_NOTCOPYPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = ~P */
	*pixel = ~(*pen);
}

INLINE void SetPixel_MASKPENNOT_32bpp(uint32* pixel, uint32* pen)
{
	/* D = P & ~D */
	*pixel = *pen & ~*pixel;
}

INLINE void SetPixel_NOT_32bpp(uint32* pixel, uint32* pen)
{
	/* D = ~D */
	*pixel = ~(*pixel);
}

INLINE void SetPixel_XORPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = D ^ P */
	*pixel = *pixel ^ *pen;
}

INLINE void SetPixel_NOTMASKPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = ~(D & P) */
	*pixel = ~(*pixel & *pen);
}

INLINE void SetPixel_MASKPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = D & P */
	*pixel &= *pen;
}

INLINE void SetPixel_NOTXORPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = ~(D ^ P) */
	*pixel = ~(*pixel ^ *pen);
}

INLINE void SetPixel_NOP_32bpp(uint32* pixel, uint32* pen)
{
	/* D = D */
}

INLINE void SetPixel_MERGENOTPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = D | ~P */
	*pixel |= ~(*pen);
}

INLINE void SetPixel_COPYPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = P */
	*pixel = *pen;
}

INLINE void SetPixel_MERGEPENNOT_32bpp(uint32* pixel, uint32* pen)
{
	/* D = P | ~D */
	*pixel = *pen | ~(*pixel);
}

INLINE void SetPixel_MERGEPEN_32bpp(uint32* pixel, uint32* pen)
{
	/* D = P | D */
	*pixel |= *pen;
}

INLINE void SetPixel_WHITE_32bpp(uint32* pixel, uint32* pen)
{
	/* D = 1 */
	*pixel = 0xFFFFFF;
}

#define PIXEL_TYPE		uint32
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

int LineTo_32bpp(HGDI_DC hdc, int nXEnd, int nYEnd)
{
	pLineTo_32bpp _LineTo;
	int rop2 = gdi_GetROP2(hdc) - 1;

	_LineTo = LineTo_ROP2_32bpp[rop2];

	if (_LineTo != NULL)
		return _LineTo(hdc, nXEnd, nYEnd);
	else
		return 0;
}
