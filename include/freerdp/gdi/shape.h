/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Shape Functions
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

#ifndef FREERDP_GDI_SHAPE_H
#define FREERDP_GDI_SHAPE_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL gdi_Ellipse(HGDI_DC hdc, int nLeftRect, int nTopRect, int nRightRect,
	                             int nBottomRect);
	FREERDP_API BOOL gdi_FillRect(HGDI_DC hdc, const HGDI_RECT rect, HGDI_BRUSH hbr);
	FREERDP_API BOOL gdi_Polygon(HGDI_DC hdc, GDI_POINT* lpPoints, int nCount);
	FREERDP_API BOOL gdi_PolyPolygon(HGDI_DC hdc, GDI_POINT* lpPoints, int* lpPolyCounts,
	                                 int nCount);
	FREERDP_API BOOL gdi_Rectangle(HGDI_DC hdc, INT32 nXDst, INT32 nYDst, INT32 nWidth,
	                               INT32 nHeight);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GDI_SHAPE_H */
