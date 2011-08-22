/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Clipping Functions
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

#ifndef __GDI_CLIPPING_H
#define __GDI_CLIPPING_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

FREERDP_API int gdi_SetClipRgn(HGDI_DC hdc, int nXLeft, int nYLeft, int nWidth, int nHeight);
FREERDP_API HGDI_RGN gdi_GetClipRgn(HGDI_DC hdc);
FREERDP_API int gdi_SetNullClipRgn(HGDI_DC hdc);
FREERDP_API int gdi_ClipCoords(HGDI_DC hdc, int *x, int *y, int *w, int *h, int *srcx, int *srcy);

#endif /* __GDI_CLIPPING_H */
