/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Pen Functions
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

/* GDI Pen Functions: http://msdn.microsoft.com/en-us/library/dd162790 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/gdi/pen.h>

/**
 * Create a new pen.\n
 * @msdn{dd183509}
 * @param fnPenStyle pen style
 * @param nWidth pen width
 * @param crColor pen color
 * @return new pen
 */

HGDI_PEN gdi_CreatePen(int fnPenStyle, int nWidth, int crColor)
{
	HGDI_PEN hPen = (HGDI_PEN) malloc(sizeof(GDI_PEN));
	hPen->objectType = GDIOBJECT_PEN;
	hPen->style = fnPenStyle;
	hPen->color = crColor;
	hPen->width = nWidth;
	return hPen;
}

INLINE uint8 gdi_GetPenColor_8bpp(HGDI_PEN pen)
{
	/* TODO: implement conversion using palette */
	return 0xFF;
}

INLINE uint16 gdi_GetPenColor_16bpp(HGDI_PEN pen)
{
	uint16 p;
	int r, g, b;
	GetRGB32(r, g, b, pen->color);
 	RGB_888_565(r, g, b);
	p = RGB16(r, g, b);
	return p;
}

INLINE uint32 gdi_GetPenColor_32bpp(HGDI_PEN pen)
{
	return pen->color;
}
