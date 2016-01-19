/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI 8bpp Internal Buffer Routines
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

#include <freerdp/log.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/clipping.h>
#include <freerdp/gdi/drawing.h>

#include <freerdp/gdi/8bpp.h>

#define TAG FREERDP_TAG("gdi")

BYTE gdi_get_color_8bpp(HGDI_DC hdc, GDI_COLOR color)
{
	/* TODO: Implement 8bpp gdi_get_color_8bpp() */
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return 1;
}

BOOL FillRect_8bpp(HGDI_DC hdc, HGDI_RECT rect, HGDI_BRUSH hbr)
{
	/* TODO: Implement 8bpp FillRect() */
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return TRUE;
}

static BOOL BitBlt_BLACKNESS_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int y;
	BYTE* dstp;

	for (y = 0; y < nHeight; y++)
	{
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
			memset(dstp, 0, nWidth * hdcDest->bytesPerPixel);
	}

	return TRUE;
}

static BOOL BitBlt_WHITENESS_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
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

static BOOL BitBlt_SRCCOPY_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int y;
	BYTE* srcp;
	BYTE* dstp;

	if (!hdcSrc || !hdcDest)
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

static BOOL BitBlt_NOTSRCCOPY_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static BOOL BitBlt_DSTINVERT_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
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
				*dstp = ~(*dstp);
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SRCERASE_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
		
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static BOOL BitBlt_NOTSRCERASE_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
		
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static BOOL BitBlt_SRCINVERT_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
		
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static BOOL BitBlt_SRCAND_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
		
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static BOOL BitBlt_SRCPAINT_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
		
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static BOOL BitBlt_DSPDxax_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	if (!hdcSrc || !hdcDest)
		return FALSE;

	/* TODO: Implement 8bpp DSPDxax BitBlt */
	WLog_ERR(TAG, "%s: not implemented", __FUNCTION__);
	return TRUE;
}

static BOOL BitBlt_PSDPxax_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	BYTE* patp;
	BYTE color8;

	if (!hdcSrc || !hdcDest)
		return FALSE;

	/* D = (S & D) | (~S & P) */

	if (hdcDest->brush->style == GDI_BS_SOLID)
	{
		color8 = gdi_get_color_8bpp(hdcDest, hdcDest->brush->color);
		patp = (BYTE*) &color8;

		for (y = 0; y < nHeight; y++)
		{
			srcp = (BYTE*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = (BYTE*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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
			srcp = (BYTE*) gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
			dstp = (BYTE*) gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = (BYTE*) gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
					*dstp = (*srcp & *dstp) | (~(*srcp) & *patp);
					srcp++;
					dstp++;
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_SPna_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	BYTE* patp;
	
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);		

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
				
				*dstp = *srcp & ~(*patp);
				patp++;
				srcp++;
				dstp++;
			}
		}
	}
	
	return TRUE;
}

static BOOL BitBlt_DPa_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	BYTE* dstp;
	BYTE* patp;

	for (y = 0; y < nHeight; y++)
	{
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);

				*dstp = *dstp & *patp;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PDxn_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	BYTE* dstp;
	BYTE* patp;

	for (y = 0; y < nHeight; y++)
	{
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);

				*dstp = *dstp ^ ~(*patp);
				patp++;
				dstp++;
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_DSna_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;

	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static BOOL BitBlt_MERGECOPY_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	BYTE* patp;
	
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
				
				*dstp = *srcp & *patp;
				patp++;
				srcp++;
				dstp++;
			}
		}
	}
	
	return TRUE;
}

static BOOL BitBlt_MERGEPAINT_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

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

static BOOL BitBlt_PATCOPY_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y, xOffset, yOffset;
	BYTE* dstp;
	BYTE* patp;
	BYTE palIndex;

	if(hdcDest->brush->style == GDI_BS_SOLID)
	{
		palIndex = ((hdcDest->brush->color >> 16) & 0xFF);
		for (y = 0; y < nHeight; y++)
		{
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);
			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp = palIndex;
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
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = gdi_get_brush_pointer(hdcDest, nXDest + x + xOffset, nYDest + y + yOffset);

					*dstp = *patp;
					patp++;
					dstp++;
				}
			}
		}
	}

	return TRUE;
}

static BOOL BitBlt_PATINVERT_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight)
{
	int x, y;
	BYTE* dstp;
	BYTE* patp;
	BYTE palIndex;

	if(hdcDest->brush->style == GDI_BS_SOLID)
	{
		palIndex = ((hdcDest->brush->color >> 16) & 0xFF);
		for (y = 0; y < nHeight; y++)
		{
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);
			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					*dstp ^= palIndex;
					dstp++;
				}
			}
		}
	}
	else
	{
		for (y = 0; y < nHeight; y++)
		{
			dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

			if (dstp != 0)
			{
				for (x = 0; x < nWidth; x++)
				{
					patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);

					*dstp = *patp ^ *dstp;
					patp++;
					dstp++;
				}
			}
		}
	}
	
	return TRUE;
}

static BOOL BitBlt_PATPAINT_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* srcp;
	BYTE* dstp;
	BYTE* patp;
	
	if (!hdcSrc || !hdcDest)
		return FALSE;

	for (y = 0; y < nHeight; y++)
	{
		srcp = gdi_get_bitmap_pointer(hdcSrc, nXSrc, nYSrc + y);
		dstp = gdi_get_bitmap_pointer(hdcDest, nXDest, nYDest + y);

		if (dstp != 0)
		{
			for (x = 0; x < nWidth; x++)
			{
				patp = gdi_get_brush_pointer(hdcDest, nXDest + x, nYDest + y);
				
				*dstp = *dstp | (*patp | ~(*srcp));
				patp++;
				srcp++;
				dstp++;
			}
		}
	}
	
	return TRUE;
}

BOOL BitBlt_8bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc, DWORD rop)
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
			return BitBlt_BLACKNESS_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_WHITENESS:
			return BitBlt_WHITENESS_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_SRCCOPY:
			return BitBlt_SRCCOPY_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SPna:
			return BitBlt_SPna_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSna:
			return BitBlt_DSna_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSPDxax:
			return BitBlt_DSPDxax_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_PSDPxax:
			return BitBlt_PSDPxax_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_NOTSRCCOPY:
			return BitBlt_NOTSRCCOPY_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_SRCERASE:
			return BitBlt_SRCERASE_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_NOTSRCERASE:
			return BitBlt_NOTSRCERASE_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCINVERT:
			return BitBlt_SRCINVERT_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCAND:
			return BitBlt_SRCAND_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_SRCPAINT:
			return BitBlt_SRCPAINT_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_MERGECOPY:
			return BitBlt_MERGECOPY_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_MERGEPAINT:
			return BitBlt_MERGEPAINT_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);

		case GDI_PATCOPY:
			return BitBlt_PATCOPY_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_PATINVERT:
			return BitBlt_PATINVERT_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight);

		case GDI_PATPAINT:
			return BitBlt_PATPAINT_8bpp(hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSrc, nXSrc, nYSrc);
	}

	WLog_ERR(TAG,  "BitBlt: unknown rop: 0x%08X", rop);
	return FALSE;
}

BOOL PatBlt_8bpp(HGDI_DC hdc, int nXLeft, int nYLeft, int nWidth, int nHeight, DWORD rop)
{
	if (!gdi_ClipCoords(hdc, &nXLeft, &nYLeft, &nWidth, &nHeight, NULL, NULL))
		return TRUE;
	
	if (!gdi_InvalidateRegion(hdc, nXLeft, nYLeft, nWidth, nHeight))
		return FALSE;

	switch (rop)
	{
		case GDI_PATCOPY:
			return BitBlt_PATCOPY_8bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_PATINVERT:
			return BitBlt_PATINVERT_8bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_DSTINVERT:
			return BitBlt_DSTINVERT_8bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_BLACKNESS:
			return BitBlt_BLACKNESS_8bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_WHITENESS:
			return BitBlt_WHITENESS_8bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_DPa:
			return BitBlt_DPa_8bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		case GDI_PDxn:
			return BitBlt_PDxn_8bpp(hdc, nXLeft, nYLeft, nWidth, nHeight);

		default:
			break;
	}

	WLog_ERR(TAG,  "PatBlt: unknown rop: 0x%08X", rop);
	return 1;
}

static INLINE void SetPixel_BLACK_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = 0 */
	*pixel = 0;
}

static INLINE void SetPixel_NOTMERGEPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = ~(D | P) */
	*pixel = ~(*pixel | *pen);
}

static INLINE void SetPixel_MASKNOTPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = D & ~P */
	*pixel &= ~(*pen);
}

static INLINE void SetPixel_NOTCOPYPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = ~P */
	*pixel = ~(*pen);
}

static INLINE void SetPixel_MASKPENNOT_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = P & ~D */
	*pixel = *pen & ~*pixel;
}

static INLINE void SetPixel_NOT_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = ~D */
	*pixel = ~(*pixel);
}

static INLINE void SetPixel_XORPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = D ^ P */
	*pixel = *pixel ^ *pen;
}

static INLINE void SetPixel_NOTMASKPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = ~(D & P) */
	*pixel = ~(*pixel & *pen);
}

static INLINE void SetPixel_MASKPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = D & P */
	*pixel &= *pen;
}

static INLINE void SetPixel_NOTXORPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = ~(D ^ P) */
	*pixel = ~(*pixel ^ *pen);
}

static INLINE void SetPixel_NOP_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = D */
}

static INLINE void SetPixel_MERGENOTPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = D | ~P */
	*pixel |= ~(*pen);
}

static INLINE void SetPixel_COPYPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = P */
	*pixel = *pen;
}

static INLINE void SetPixel_MERGEPENNOT_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = P | ~D */
	*pixel = *pen | ~(*pixel);
}

static INLINE void SetPixel_MERGEPEN_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = P | D */
	*pixel |= *pen;
}

static INLINE void SetPixel_WHITE_8bpp(BYTE* pixel, BYTE* pen)
{
	/* D = 1 */
	*pixel = 0xFF;
}

#define PIXEL_TYPE		BYTE
#define GDI_GET_POINTER		gdi_GetPointer_8bpp
#define GDI_GET_PEN_COLOR	gdi_GetPenColor_8bpp

#define LINE_TO			LineTo_BLACK_8bpp
#define SET_PIXEL_ROP2		SetPixel_BLACK_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTMERGEPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_NOTMERGEPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKNOTPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_MASKNOTPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTCOPYPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_NOTCOPYPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKPENNOT_8bpp
#define SET_PIXEL_ROP2		SetPixel_MASKPENNOT_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOT_8bpp
#define SET_PIXEL_ROP2		SetPixel_NOT_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_XORPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_XORPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTMASKPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_NOTMASKPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MASKPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_MASKPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOTXORPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_NOTXORPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_NOP_8bpp
#define SET_PIXEL_ROP2		SetPixel_NOP_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGENOTPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_MERGENOTPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_COPYPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_COPYPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGEPENNOT_8bpp
#define SET_PIXEL_ROP2		SetPixel_MERGEPENNOT_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_MERGEPEN_8bpp
#define SET_PIXEL_ROP2		SetPixel_MERGEPEN_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#define LINE_TO			LineTo_WHITE_8bpp
#define SET_PIXEL_ROP2		SetPixel_WHITE_8bpp
#include "include/line.c"
#undef LINE_TO
#undef SET_PIXEL_ROP2

#undef PIXEL_TYPE
#undef GDI_GET_POINTER
#undef GDI_GET_PEN_COLOR

pLineTo_8bpp LineTo_ROP2_8bpp[32] =
{
	LineTo_BLACK_8bpp,
	LineTo_NOTMERGEPEN_8bpp,
	LineTo_MASKNOTPEN_8bpp,
	LineTo_NOTCOPYPEN_8bpp,
	LineTo_MASKPENNOT_8bpp,
	LineTo_NOT_8bpp,
	LineTo_XORPEN_8bpp,
	LineTo_NOTMASKPEN_8bpp,
	LineTo_MASKPEN_8bpp,
	LineTo_NOTXORPEN_8bpp,
	LineTo_NOP_8bpp,
	LineTo_MERGENOTPEN_8bpp,
	LineTo_COPYPEN_8bpp,
	LineTo_MERGEPENNOT_8bpp,
	LineTo_MERGEPEN_8bpp,
	LineTo_WHITE_8bpp
};

BOOL LineTo_8bpp(HGDI_DC hdc, int nXEnd, int nYEnd)
{
	pLineTo_8bpp _LineTo;
	int rop2 = gdi_GetROP2(hdc) - 1;

	_LineTo = LineTo_ROP2_8bpp[rop2];

	if (_LineTo == NULL)
		return FALSE;

	return _LineTo(hdc, nXEnd, nYEnd);
}
