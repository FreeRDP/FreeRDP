/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Clipping Functions
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

#ifndef FREERDP_GDI_CLIPPING_H
#define FREERDP_GDI_CLIPPING_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_LOCAL BOOL gdi_SetClipRgn(HGDI_DC hdc, UINT32 nXLeft, UINT32 nYLeft,
                                  UINT32 nWidth, UINT32 nHeight);
FREERDP_LOCAL HGDI_RGN gdi_GetClipRgn(HGDI_DC hdc);
FREERDP_LOCAL BOOL gdi_SetNullClipRgn(HGDI_DC hdc);
FREERDP_LOCAL BOOL gdi_ClipCoords(HGDI_DC hdc, UINT32* x, UINT32* y,
                                  UINT32* w, UINT32* h, UINT32* srcx, UINT32* srcy);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GDI_CLIPPING_H */
