/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI 32bpp Internal Buffer Routines
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

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

typedef int (*pLineTo_32bpp)(HGDI_DC hdc, int nXEnd, int nYEnd);

FREERDP_API uint32 gdi_get_color_32bpp(HGDI_DC hdc, GDI_COLOR color);

FREERDP_API int FillRect_32bpp(HGDI_DC hdc, HGDI_RECT rect, HGDI_BRUSH hbr);
FREERDP_API int BitBlt_32bpp(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc, int rop);
FREERDP_API int PatBlt_32bpp(HGDI_DC hdc, int nXLeft, int nYLeft, int nWidth, int nHeight, int rop);
FREERDP_API int LineTo_32bpp(HGDI_DC hdc, int nXEnd, int nYEnd);
