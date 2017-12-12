/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
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

HGDI_PEN gdi_CreatePen(UINT32 fnPenStyle, UINT32 nWidth, UINT32 crColor,
			   UINT32 format, const gdiPalette* palette)
{
	HGDI_PEN hPen = (HGDI_PEN) calloc(1, sizeof(GDI_PEN));
	if (!hPen)
		return NULL;
	hPen->objectType = GDIOBJECT_PEN;
	hPen->style = fnPenStyle;
	hPen->color = crColor;
	hPen->width = nWidth;
	hPen->format = format;
	hPen->palette = palette;
	return hPen;
}

INLINE UINT32 gdi_GetPenColor(HGDI_PEN pen, UINT32 format)
{
	return FreeRDPConvertColor(pen->color, pen->format, format, pen->palette);
}
