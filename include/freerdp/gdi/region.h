/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Region Functions
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#ifndef FREERDP_GDI_REGION_H
#define FREERDP_GDI_REGION_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API HGDI_RGN gdi_CreateRectRgn(INT32 nLeftRect, INT32 nTopRect, INT32 nRightRect,
	                                       INT32 nBottomRect);
	FREERDP_API HGDI_RECT gdi_CreateRect(INT32 xLeft, INT32 yTop, INT32 xRight, INT32 yBottom);
	FREERDP_API BOOL gdi_RectToRgn(const HGDI_RECT rect, HGDI_RGN rgn);
	FREERDP_API BOOL gdi_CRectToRgn(INT32 left, INT32 top, INT32 right, INT32 bottom, HGDI_RGN rgn);
	FREERDP_API BOOL gdi_RectToCRgn(const HGDI_RECT rect, INT32* x, INT32* y, INT32* w, INT32* h);
	FREERDP_API BOOL gdi_CRectToCRgn(INT32 left, INT32 top, INT32 right, INT32 bottom, INT32* x,
	                                 INT32* y, INT32* w, INT32* h);
	FREERDP_API BOOL gdi_RgnToRect(const HGDI_RGN rgn, HGDI_RECT rect);
	FREERDP_API BOOL gdi_CRgnToRect(INT64 x, INT64 y, INT32 w, INT32 h, HGDI_RECT rect);
	FREERDP_API BOOL gdi_RgnToCRect(const HGDI_RGN rgn, INT32* left, INT32* top, INT32* right,
	                                INT32* bottom);
	FREERDP_API BOOL gdi_CRgnToCRect(INT32 x, INT32 y, INT32 w, INT32 h, INT32* left, INT32* top,
	                                 INT32* right, INT32* bottom);
	FREERDP_API BOOL gdi_CopyOverlap(INT32 x, INT32 y, INT32 width, INT32 height, INT32 srcx,
	                                 INT32 srcy);
	FREERDP_API BOOL gdi_SetRect(HGDI_RECT rc, INT32 xLeft, INT32 yTop, INT32 xRight,
	                             INT32 yBottom);
	FREERDP_API BOOL gdi_SetRgn(HGDI_RGN hRgn, INT32 nXLeft, INT32 nYLeft, INT32 nWidth,
	                            INT32 nHeight);
	FREERDP_API BOOL gdi_SetRectRgn(HGDI_RGN hRgn, INT32 nLeftRect, INT32 nTopRect,
	                                INT32 nRightRect, INT32 nBottomRect);
	FREERDP_API BOOL gdi_EqualRgn(const HGDI_RGN hSrcRgn1, const HGDI_RGN hSrcRgn2);
	FREERDP_API BOOL gdi_CopyRect(HGDI_RECT dst, const HGDI_RECT src);
	FREERDP_API BOOL gdi_PtInRect(const HGDI_RECT rc, INT32 x, INT32 y);
	FREERDP_API BOOL gdi_InvalidateRegion(HGDI_DC hdc, INT32 x, INT32 y, INT32 w, INT32 h);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GDI_REGION_H */
