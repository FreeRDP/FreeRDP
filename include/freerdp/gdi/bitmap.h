/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Bitmap Functions
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

#ifndef FREERDP_GDI_BITMAP_H
#define FREERDP_GDI_BITMAP_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
 extern "C" {
#endif

FREERDP_API GDI_COLOR gdi_GetPixel(HGDI_DC hdc, int nXPos, int nYPos);
FREERDP_API GDI_COLOR gdi_SetPixel(HGDI_DC hdc, int X, int Y, GDI_COLOR crColor);
FREERDP_API BYTE gdi_GetPixel_8bpp(HGDI_BITMAP hBmp, int X, int Y);
FREERDP_API UINT16 gdi_GetPixel_16bpp(HGDI_BITMAP hBmp, int X, int Y);
FREERDP_API UINT32 gdi_GetPixel_32bpp(HGDI_BITMAP hBmp, int X, int Y);
FREERDP_API BYTE* gdi_GetPointer_8bpp(HGDI_BITMAP hBmp, int X, int Y);
FREERDP_API UINT16* gdi_GetPointer_16bpp(HGDI_BITMAP hBmp, int X, int Y);
FREERDP_API UINT32* gdi_GetPointer_32bpp(HGDI_BITMAP hBmp, int X, int Y);
FREERDP_API void gdi_SetPixel_8bpp(HGDI_BITMAP hBmp, int X, int Y, BYTE pixel);
FREERDP_API void gdi_SetPixel_16bpp(HGDI_BITMAP hBmp, int X, int Y, UINT16 pixel);
FREERDP_API void gdi_SetPixel_32bpp(HGDI_BITMAP hBmp, int X, int Y, UINT32 pixel);
FREERDP_API HGDI_BITMAP gdi_CreateBitmap(int nWidth, int nHeight, int cBitsPerPixel,
										BYTE* data);
FREERDP_API HGDI_BITMAP gdi_CreateBitmapEx(int nWidth, int nHeight, int cBitsPerPixel,
										BYTE* data, void (*fkt_free)(void*));
FREERDP_API HGDI_BITMAP gdi_CreateCompatibleBitmap(HGDI_DC hdc, int nWidth, int nHeight);
FREERDP_API BOOL gdi_BitBlt(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc, DWORD rop);

typedef BOOL (*p_BitBlt)(HGDI_DC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HGDI_DC hdcSrc, int nXSrc, int nYSrc, DWORD rop);

#ifdef __cplusplus
 }
#endif

#endif /* FREERDP_GDI_BITMAP_H */
