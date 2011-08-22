/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Region Functions
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

#ifndef __GDI_REGION_H
#define __GDI_REGION_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

FREERDP_API HGDI_RGN gdi_CreateRectRgn(int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
FREERDP_API HGDI_RECT gdi_CreateRect(int xLeft, int yTop, int xRight, int yBottom);
FREERDP_API void gdi_RectToRgn(HGDI_RECT rect, HGDI_RGN rgn);
FREERDP_API void gdi_CRectToRgn(int left, int top, int right, int bottom, HGDI_RGN rgn);
FREERDP_API void gdi_RectToCRgn(HGDI_RECT rect, int *x, int *y, int *w, int *h);
FREERDP_API void gdi_CRectToCRgn(int left, int top, int right, int bottom, int *x, int *y, int *w, int *h);
FREERDP_API void gdi_RgnToRect(HGDI_RGN rgn, HGDI_RECT rect);
FREERDP_API void gdi_CRgnToRect(int x, int y, int w, int h, HGDI_RECT rect);
FREERDP_API void gdi_RgnToCRect(HGDI_RGN rgn, int *left, int *top, int *right, int *bottom);
FREERDP_API void gdi_CRgnToCRect(int x, int y, int w, int h, int *left, int *top, int *right, int *bottom);
FREERDP_API int gdi_CopyOverlap(int x, int y, int width, int height, int srcx, int srcy);
FREERDP_API int gdi_SetRect(HGDI_RECT rc, int xLeft, int yTop, int xRight, int yBottom);
FREERDP_API int gdi_SetRgn(HGDI_RGN hRgn, int nXLeft, int nYLeft, int nWidth, int nHeight);
FREERDP_API int gdi_SetRectRgn(HGDI_RGN hRgn, int nLeftRect, int nTopRect, int nRightRect, int nBottomRect);
FREERDP_API int gdi_EqualRgn(HGDI_RGN hSrcRgn1, HGDI_RGN hSrcRgn2);
FREERDP_API int gdi_CopyRect(HGDI_RECT dst, HGDI_RECT src);
FREERDP_API int gdi_PtInRect(HGDI_RECT rc, int x, int y);
FREERDP_API int gdi_InvalidateRegion(HGDI_DC hdc, int x, int y, int w, int h);

#endif /* __GDI_REGION_H */
