/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Bitmap Functions
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

#ifndef FREERDP_GDI_BITMAP_H
#define FREERDP_GDI_BITMAP_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API UINT32 gdi_GetPixel(HGDI_DC hdc, UINT32 nXPos, UINT32 nYPos);
	FREERDP_API UINT32 gdi_SetPixel(HGDI_DC hdc, UINT32 X, UINT32 Y, UINT32 crColor);
	FREERDP_API BYTE* gdi_GetPointer(HGDI_BITMAP hBmp, UINT32 X, UINT32 Y);

	FREERDP_API HGDI_BITMAP gdi_CreateBitmap(UINT32 nWidth, UINT32 nHeight, UINT32 format,
	                                         BYTE* data);
	FREERDP_API HGDI_BITMAP gdi_CreateBitmapEx(UINT32 nWidth, UINT32 nHeight, UINT32 format,
	                                           UINT32 stride, BYTE* data, void (*fkt_free)(void*));
	FREERDP_API HGDI_BITMAP gdi_CreateCompatibleBitmap(HGDI_DC hdc, UINT32 nWidth, UINT32 nHeight);

	FREERDP_API BOOL gdi_BitBlt(HGDI_DC hdcDest, INT32 nXDest, INT32 nYDest, INT32 nWidth,
	                            INT32 nHeight, HGDI_DC hdcSrc, INT32 nXSrc, INT32 nYSrc, DWORD rop,
	                            const gdiPalette* palette);

	typedef BOOL (*p_BitBlt)(HGDI_DC hdcDest, INT32 nXDest, INT32 nYDest, INT32 nWidth,
	                         INT32 nHeight, HGDI_DC hdcSrc, INT32 nXSrc, INT32 nYSrc, DWORD rop);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GDI_BITMAP_H */
