/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI 16bpp Internal Buffer Routines
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

#include <freerdp/gdi/16bpp.h>

uint16 gdi_get_color_16bpp(HGDI_DC hdc, GDI_COLOR color)
{
	uint8 r, g, b;
	uint16 color16;

	GetBGR32(r, g, b, color);

	if (hdc->rgb555)
	{
		if (hdc->invert)
		{
			color16 = BGR15(r, g, b);
		}
		else
		{
			color16 = RGB15(r, g, b);
		}
	}
	else
	{
		if (hdc->invert)
		{
			color16 = BGR16(r, g, b);
		}
		else
		{
			color16 = RGB16(r, g, b);
		}
	}

	return color16;
}

int FillRect_16bpp(HGDI_DC hdc, HGDI_RECT rect, HGDI_BRUSH hbr)
{
	int x, y;
	uint16 *dstp;
	int nXDest, nYDest;
	int nWidth, nHeight;

	uint16 color16;
	
	gdi_RectToCRgn(rect, &nXDest, &nYDest, &nWidth, &nHeight);
	
	if (gdi_ClipCoords(hdc, &nXDest, &nYDest, &nWidth, &nHeight, NULL, NULL) == 0)
		return 0;

	color16 = gdi_get_color_16bpp(hdc, hbr->color);

	for (y = 0; y < nHeight; y++)
	{
		dstp = (uint16*) gdi_get_bitmap_pointer(hdc, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				*dstp = color16;
				dstp++;
			}
		}
	}

	gdi_InvalidateRegion(hdc, nXDest, nYDest, nWidth, nHeight);
	return 0;
}

static int BitBlt_BLACKNESS_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int y;
	uint8* dstp;

	for (y = 0; y < nHeight; y++)
	{
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
			memset(dstp, 0, nWidth * hdcDest->bytesPerPixel);
	}

	return 0;
}

static int BitBlt_WHITENESS_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
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

static int BitBlt_SRCCOPY_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
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

static int BitBlt_NOTSRCCOPY_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static int BitBlt_DSTINVERT_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	uint16* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static int BitBlt_SRCERASE_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static int BitBlt_NOTSRCERASE_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static int BitBlt_SRCINVERT_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static int BitBlt_SRCAND_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static int BitBlt_SRCPAINT_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
		
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static int BitBlt_DSPDxax_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{	
	int x, y;
	uint8* srcp;
	uint16* dstp;
	uint16 src16;
	uint16 color16;
	HGDI_BITMAP hSrcBmp;

	/* D = (S & P) | (~S & D) */
	/* DSPDxax, used to draw glyphs */

	color16 = gdi_get_color_16bpp(hdcDest, hdcDest->textColor);

	hSrcBmp = (HGDI_BITMAP) hdcSrc->selectedObject;

	if (hdcSrc->bytesPerPixel != 1)
	{
		printf("BitBlt_DSPDxax expects 1 bpp, unimplemented for %d\n", hdcSrc->bytesPerPixel);
		return 0;
	}
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint8*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				src16 = (*srcp << 8) | *srcp;
				*dstp = (src16 & color16) | (~src16 & *dstp);
				srcp++;
				dstp++;
			}
		}
	}

	return 0;
}

static int BitBlt_SPna_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
	uint16* patp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint16*) gdi_get_brush_pointer(hdcDest, x, y);
				*dstp = *srcp & ~(*patp);
				srcp++;
				dstp++;
			}
		}
	}
	
	return 0;
}

static int BitBlt_PDxn_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	uint16* dstp;
	uint16* patp;

	for (y = 0; y < nHeight; y++)
	{
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint16*) gdi_get_brush_pointer(hdcDest, x, y);
				*dstp = *dstp ^ ~(*patp);
				dstp++;
			}
		}
	}

	return 0;
}

static int BitBlt_DSna_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;

	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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


static int BitBlt_MERGECOPY_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
	uint16* patp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint16*) gdi_get_brush_pointer(hdcDest, x, y);
				*dstp = *srcp & *patp;
				srcp++;
				dstp++;
			}
		}
	}
	
	return 0;
}

static int BitBlt_MERGEPAINT_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static int BitBlt_PATCOPY_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	uint16* dstp;
	uint16* patp;
	uint16 color16;

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color16 = gdi_get_color_16bpp(hdcDest, hdcDest->brush->color);

		for (y = 0; y < nHeight; y++)
		{
			dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp = color16;
					dstp++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (uint16*) gdi_get_brush_pointer(hdcDest, x, y);
					*dstp = *patp;
					dstp++;
				}
			}
		}
	}

	return 0;
}

static int BitBlt_PATINVERT_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	uint16* dstp;
	uint16* patp;
	uint16 color16;

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color16 = gdi_get_color_16bpp(hdcDest, hdcDest->brush->color);

		for (y = 0; y < nHeight; y++)
		{
			dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp ^= color16;
					dstp++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (uint16*) gdi_get_brush_pointer(hdcDest, x, y);
					*dstp = *patp ^ *dstp;
					dstp++;
				}
			}
		}
	}
	
	return 0;
}

static int BitBlt_PATPAINT_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	uint16* srcp;
	uint16* dstp;
	uint16* patp;
	
	for (y = 0; y < nHeight; y++)
	{
		srcp = (uint16*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = (uint16*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = (uint16*) gdi_get_brush_pointer(hdcDest, x, y);
				*dstp = *dstp | (*patp | ~(*srcp));
				srcp++;
				dstp++;
			}
		}
	}
	
	return 0;
}

int BitBlt_16bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc, int rop)
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
			return BitBlt_BLACKNESS_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_WHITENESS:
			return BitBlt_WHITENESS_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_SRCCOPY:
			return BitBlt_SRCCOPY_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_SPna:
			return BitBlt_SPna_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_DSna:
			return BitBlt_DSna_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_DSPDxax:
			return BitBlt_DSPDxax_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;
			
		case GDI_NOTSRCCOPY:
			return BitBlt_NOTSRCCOPY_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_SRCERASE:
			return BitBlt_SRCERASE_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_NOTSRCERASE:
			return BitBlt_NOTSRCERASE_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_SRCINVERT:
			return BitBlt_SRCINVERT_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_SRCAND:
			return BitBlt_SRCAND_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_SRCPAINT:
			return BitBlt_SRCPAINT_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_MERGECOPY:
			return BitBlt_MERGECOPY_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_MERGEPAINT:
			return BitBlt_MERGEPAINT_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;

		case GDI_PATCOPY:
			return BitBlt_PATCOPY_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_PATINVERT:
			return BitBlt_PATINVERT_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);
			break;

		case GDI_PATPAINT:
			return BitBlt_PATPAINT_16bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
			break;
	}
	
	printf("BitBlt: unknown rop: 0x%08X\n", rop);
	return 1;
}

int PatBlt_16bpp(HGDI_DC hdc, int nXLeft, int nYLeft, int nWidth, int nHeight, int rop)
{
	if (gdi_ClipCoords(hdc, &nXLeft, &nYLeft, &nWidth, &nHeight, NULL, NULL) == 0)
		return 0;
	
	gdi_InvalidateRegion(hdc, nXLeft, nYLeft, nWidth, nHeight);

	switch (rop)
	{
		case GDI_PATCOPY:
			return BitBlt_PATCOPY_16bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		case GDI_PATINVERT:
			return BitBlt_PATINVERT_16bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;
			
		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT_16bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		case GDI_BLACKNESS:
			return BitBlt_BLACKNESS_16bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		case GDI_WHITENESS:
			return BitBlt_WHITENESS_16bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		case GDI_PDxn:
			return BitBlt_PDxn_16bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);
			break;

		default:
			break;
	}
	
	printf("PatBlt: unknown rop: 0x%08X\n", rop);

	return 1;
}

INLINE void SetPixel_BLACK_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = 0 */
	*pixel = 0;
}

INLINE void SetPixel_NOTMERGEPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = ~(D | P) */
	*pixel = ~(*pixel | *pen);
}

INLINE void SetPixel_MASKNOTPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = D & ~P */
	*pixel &= ~(*pen);
}

INLINE void SetPixel_NOTCOPYPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = ~P */
	*pixel = ~(*pen);
}

INLINE void SetPixel_MASKPENNOT_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = P & ~D */
	*pixel = *pen & ~*pixel;
}

INLINE void SetPixel_NOT_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = ~D */
	*pixel = ~(*pixel);
}

INLINE void SetPixel_XORPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = D ^ P */
	*pixel = *pixel ^ *pen;
}

INLINE void SetPixel_NOTMASKPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = ~(D & P) */
	*pixel = ~(*pixel & *pen);
}

INLINE void SetPixel_MASKPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = D & P */
	*pixel &= *pen;
}

INLINE void SetPixel_NOTXORPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = ~(D ^ P) */
	*pixel = ~(*pixel ^ *pen);
}

INLINE void SetPixel_NOP_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = D */
}

INLINE void SetPixel_MERGENOTPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = D | ~P */
	*pixel |= ~(*pen);
}

INLINE void SetPixel_COPYPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = P */
	*pixel = *pen;
}

INLINE void SetPixel_MERGEPENNOT_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = P | ~D */
	*pixel = *pen | ~(*pixel);
}

INLINE void SetPixel_MERGEPEN_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = P | D */
	*pixel |= *pen;
}

INLINE void SetPixel_WHITE_16bpp(uint16 *pixel, uint16 *pen)
{
	/* D = 1 */
	*pixel = 0xFFFF;
}

#define PIXEL_TYPE		uint16
#define GDI_GET_POINTER		gdi_GetPointer_16bpp
#define GDI_GET_PEN_COLOR	gdi_GetPenColor_16bpp

#define LINE_TO			LineTo_BLACK_16bpp
#define SET_PIXEL_ROP2		SetPixel_BLACK_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTMERGEPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_NOTMERGEPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKNOTPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_MASKNOTPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTCOPYPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_NOTCOPYPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKPENNOT_16bpp
#define SET_PIXEL_ROP2		SetPixel_MASKPENNOT_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOT_16bpp
#define SET_PIXEL_ROP2		SetPixel_NOT_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_XORPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_XORPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTMASKPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_NOTMASKPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_MASKPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTXORPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_NOTXORPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOP_16bpp
#define SET_PIXEL_ROP2		SetPixel_NOP_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGENOTPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_MERGENOTPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_COPYPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_COPYPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGEPENNOT_16bpp
#define SET_PIXEL_ROP2		SetPixel_MERGEPENNOT_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGEPEN_16bpp
#define SET_PIXEL_ROP2		SetPixel_MERGEPEN_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_WHITE_16bpp
#define SET_PIXEL_ROP2		SetPixel_WHITE_16bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#undef PIXEL_TYPE
#undef GDI_GET_POINTER
#undef GDI_GET_PEN_COLOR

pLineTo_16bpp LineTo_ROP2_16bpp[32] =
{
	LineTo_BLACK_16bpp,
	LineTo_NOTMERGEPEN_16bpp,
	LineTo_MASKNOTPEN_16bpp,
	LineTo_NOTCOPYPEN_16bpp,
	LineTo_MASKPENNOT_16bpp,
	LineTo_NOT_16bpp,
	LineTo_XORPEN_16bpp,
	LineTo_NOTMASKPEN_16bpp,
	LineTo_MASKPEN_16bpp,
	LineTo_NOTXORPEN_16bpp,
	LineTo_NOP_16bpp,
	LineTo_MERGENOTPEN_16bpp,
	LineTo_COPYPEN_16bpp,
	LineTo_MERGEPENNOT_16bpp,
	LineTo_MERGEPEN_16bpp,
	LineTo_WHITE_16bpp
};

int LineTo_16bpp(HGDI_DC hdc, int nXEnd, int nYEnd)
{
	pLineTo_16bpp _LineTo;
	int rop2 = gdi_GetROP2(hdc) - 1;

	_LineTo = LineTo_ROP2_16bpp[rop2];

	if (_LineTo != NULL)
		return _LineTo(hdc, nXEnd, nYEnd);
	else
		return 0;
}
