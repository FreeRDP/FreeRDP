/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Brush Functions
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

/* GDI Brush Functions: http://msdn.microsoft.com/en-us/library/dd183395/ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/32bpp.h>
#include <freerdp/gdi/16bpp.h>
#include <freerdp/gdi/8bpp.h>

#include <freerdp/gdi/brush.h>

p_PatBlt PatBlt_[5] =
{
	NULL,
	PatBlt_8bpp,
	PatBlt_16bpp,
	NULL,
	PatBlt_32bpp
};

/**
 * Create a new solid brush.\n
 * @msdn{dd183518}
 * @param crColor brush color
 * @return new brush
 */

HGDI_BRUSH gdi_CreateSolidBrush(GDI_COLOR crColor)
{
	HGDI_BRUSH hBrush = (HGDI_BRUSH) malloc(sizeof(GDI_BRUSH));
	hBrush->objectType = GDIOBJECT_BRUSH;
	hBrush->style = GDI_BS_SOLID;
	hBrush->color = crColor;
	return hBrush;
}

/**
 * Create a new pattern brush.\n
 * @msdn{dd183508}
 * @param hbmp pattern bitmap
 * @return new brush
 */

HGDI_BRUSH gdi_CreatePatternBrush(HGDI_BITMAP hbmp)
{
	HGDI_BRUSH hBrush = (HGDI_BRUSH) malloc(sizeof(GDI_BRUSH));
	hBrush->objectType = GDIOBJECT_BRUSH;
	hBrush->style = GDI_BS_PATTERN;
	hBrush->pattern = hbmp;
	return hBrush;
}

/**
 * Perform a pattern blit operation on the given pixel buffer.\n
 * @msdn{dd162778}
 * @param hdc device context
 * @param nXLeft x1
 * @param nYLeft y1
 * @param nWidth width
 * @param nHeight height
 * @param rop raster operation code
 * @return 1 if successful, 0 otherwise
 */

int gdi_PatBlt(HGDI_DC hdc, int nXLeft, int nYLeft, int nWidth, int nHeight, int rop)
{
	p_PatBlt _PatBlt = PatBlt_[IBPP(hdc->bitsPerPixel)];

	if (_PatBlt != NULL)
		return _PatBlt(hdc, nXLeft, nYLeft, nWidth, nHeight, rop);
	else
		return 0;
}
