/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Library
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_LIB_GDI_CORE_H
#define FREERDP_LIB_GDI_CORE_H

#include "graphics.h"
#include "brush.h"

#include <freerdp/api.h>

FREERDP_LOCAL BOOL gdi_bitmap_update(rdpContext* context, const BITMAP_UPDATE* bitmapUpdate);

FREERDP_LOCAL gdiBitmap* gdi_bitmap_new_ex(rdpGdi* gdi, int width, int height, int bpp, BYTE* data);
FREERDP_LOCAL void gdi_bitmap_free_ex(gdiBitmap* gdi_bmp);

static INLINE BYTE* gdi_get_bitmap_pointer(HGDI_DC hdcBmp, INT32 x, INT32 y)
{
	BYTE* p;
	HGDI_BITMAP hBmp = (HGDI_BITMAP)hdcBmp->selectedObject;

	if ((x >= 0) && (y >= 0) && (x < hBmp->width) && (y < hBmp->height))
	{
		p = hBmp->data + (y * hBmp->scanline) + (x * GetBytesPerPixel(hdcBmp->format));
		return p;
	}
	else
	{
		WLog_ERR(FREERDP_TAG("gdi"),
		         "gdi_get_bitmap_pointer: requesting invalid pointer: (%" PRIu32 ",%" PRIu32
		         ") in %" PRIu32 "x%" PRIu32 "",
		         x, y, hBmp->width, hBmp->height);
		return 0;
	}
}

/**
 * Get current color in brush bitmap according to dest coordinates.\n
 * @msdn{dd183396}
 * @param x dest x-coordinate
 * @param y dest y-coordinate
 * @return color
 */
static INLINE BYTE* gdi_get_brush_pointer(HGDI_DC hdcBrush, UINT32 x, UINT32 y)
{
	BYTE* p;
	UINT32 brushStyle = gdi_GetBrushStyle(hdcBrush);

	switch (brushStyle)
	{
		case GDI_BS_PATTERN:
		case GDI_BS_HATCHED:
		{
			HGDI_BITMAP hBmpBrush = hdcBrush->brush->pattern;
			/* According to @msdn{dd183396}, the system always positions a brush bitmap
			 * at the brush origin and copy across the client area.
			 * Calculate the offset of the mapped pixel in the brush bitmap according to
			 * brush origin and dest coordinates */
			x = (x + hBmpBrush->width - (hdcBrush->brush->nXOrg % hBmpBrush->width)) %
			    hBmpBrush->width;
			y = (y + hBmpBrush->height - (hdcBrush->brush->nYOrg % hBmpBrush->height)) %
			    hBmpBrush->height;
			p = hBmpBrush->data + (y * hBmpBrush->scanline) +
			    (x * GetBytesPerPixel(hBmpBrush->format));
			return p;
		}
		break;

		default:
			break;
	}

	p = (BYTE*)&(hdcBrush->textColor);
	return p;
}

#endif /* FREERDP_LIB_GDI_CORE_H */
