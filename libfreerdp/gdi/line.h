/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Line Functions
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

#ifndef FREERDP_LIB_GDI_LINE_H
#define FREERDP_LIB_GDI_LINE_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL BOOL gdi_LineTo(HGDI_DC hdc, UINT32 nXEnd, UINT32 nYEnd);
	FREERDP_LOCAL BOOL gdi_PolylineTo(HGDI_DC hdc, GDI_POINT* lppt, DWORD cCount);
	FREERDP_LOCAL BOOL gdi_Polyline(HGDI_DC hdc, GDI_POINT* lppt, UINT32 cPoints);
	FREERDP_LOCAL BOOL gdi_PolyPolyline(HGDI_DC hdc, GDI_POINT* lppt, UINT32* lpdwPolyPoints,
	                                    DWORD cCount);
	FREERDP_LOCAL BOOL gdi_MoveToEx(HGDI_DC hdc, UINT32 X, UINT32 Y, HGDI_POINT lpPoint);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_GDI_LINE_H */
