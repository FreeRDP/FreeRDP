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
extern "C" {
#endif

FREERDP_API HGDI_RGN gdi_CreateRectRgn(UINT32 nLeftRect, UINT32 nTopRect,
                                       UINT32 nRightRect, UINT32 nBottomRect);
FREERDP_API HGDI_RECT gdi_CreateRect(UINT32 xLeft, UINT32 yTop,
                                     UINT32 xRight, UINT32 yBottom);
FREERDP_API void gdi_RectToRgn(HGDI_RECT rect, HGDI_RGN rgn);
FREERDP_API void gdi_CRectToRgn(UINT32 left, UINT32 top,
                                UINT32 right, UINT32 bottom, HGDI_RGN rgn);
FREERDP_API void gdi_RectToCRgn(const HGDI_RECT rect, UINT32* x, UINT32* y,
                                UINT32* w, UINT32* h);
FREERDP_API void gdi_CRectToCRgn(UINT32 left, UINT32 top,
                                 UINT32 right, UINT32 bottom,
                                 UINT32* x, UINT32* y, UINT32* w, UINT32* h);
FREERDP_API void gdi_RgnToRect(HGDI_RGN rgn, HGDI_RECT rect);
FREERDP_API void gdi_CRgnToRect(INT64 x, INT64 y, UINT32 w, UINT32 h, HGDI_RECT rect);
FREERDP_API void gdi_RgnToCRect(HGDI_RGN rgn, UINT32* left,
                                UINT32* top, UINT32* right, UINT32* bottom);
FREERDP_API void gdi_CRgnToCRect(UINT32 x, UINT32 y, UINT32 w, UINT32 h,
                                 UINT32* left, UINT32* top, UINT32* right, UINT32* bottom);
FREERDP_API BOOL gdi_CopyOverlap(UINT32 x, UINT32 y, UINT32 width, UINT32 height,
                                 UINT32 srcx, UINT32 srcy);
FREERDP_API BOOL gdi_SetRect(HGDI_RECT rc, UINT32 xLeft, UINT32 yTop,
                             UINT32 xRight, UINT32 yBottom);
FREERDP_API BOOL gdi_SetRgn(HGDI_RGN hRgn, UINT32 nXLeft, UINT32 nYLeft,
                            UINT32 nWidth, UINT32 nHeight);
FREERDP_API BOOL gdi_SetRectRgn(HGDI_RGN hRgn, UINT32 nLeftRect, UINT32 nTopRect,
                                UINT32 nRightRect, UINT32 nBottomRect);
FREERDP_API BOOL gdi_EqualRgn(HGDI_RGN hSrcRgn1, HGDI_RGN hSrcRgn2);
FREERDP_API BOOL gdi_CopyRect(HGDI_RECT dst, HGDI_RECT src);
FREERDP_API BOOL gdi_PtInRect(HGDI_RECT rc, UINT32 x, UINT32 y);
FREERDP_API BOOL gdi_InvalidateRegion(HGDI_DC hdc, UINT32 x, UINT32 y,
                                      UINT32 w, UINT32 h);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GDI_REGION_H */
