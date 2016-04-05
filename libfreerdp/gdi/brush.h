/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Brush Functions
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

#ifndef FREERDP_GDI_BRUSH_H
#define FREERDP_GDI_BRUSH_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
extern "C" {
#endif

HGDI_BRUSH gdi_CreateSolidBrush(UINT32 crColor);
HGDI_BRUSH gdi_CreatePatternBrush(HGDI_BITMAP hbmp);
HGDI_BRUSH gdi_CreateHatchBrush(HGDI_BITMAP hbmp);
BOOL gdi_PatBlt(HGDI_DC hdc, UINT32 nXLeft, UINT32 nYLeft,
				UINT32 nWidth, UINT32 nHeight, DWORD rop);

BOOL BitBlt_PATPAINT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			    UINT32 nWidth, UINT32 nHeight, HGDI_DC hdcSrc,
			    UINT32 nXSrc, UINT32 nYSrc);
BOOL BitBlt_PATINVERT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			     UINT32 nWidth, UINT32 nHeight);
BOOL BitBlt_BLACKNESS(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			     UINT32 nWidth, UINT32 nHeight);
BOOL BitBlt_WHITENESS(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			     UINT32 nWidth, UINT32 nHeight);
BOOL BitBlt_PATCOPY(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			   UINT32 nWidth, UINT32 nHeight);
BOOL BitBlt_DSTINVERT(HGDI_DC hdcDest, UINT32 nXDest, UINT32 nYDest,
			     UINT32 nWidth, UINT32 nHeight);

#ifdef __cplusplus
}
#endif

typedef BOOL (*p_PatBlt)(HGDI_DC hdc, UINT32 nXLeft, UINT32 nYLeft,
						 UINT32 nWidth, UINT32 nHeight, DWORD rop);

#endif /* FREERDP_GDI_BRUSH_H */
