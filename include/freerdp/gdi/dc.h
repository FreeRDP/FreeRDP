/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Device Context Functions
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

#ifndef FREERDP_GDI_DC_H
#define FREERDP_GDI_DC_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
 extern "C" {
#endif

FREERDP_API HGDI_DC gdi_GetDC(void);
FREERDP_API HGDI_DC gdi_CreateDC(UINT32 format);
FREERDP_API HGDI_DC gdi_CreateCompatibleDC(HGDI_DC hdc);
FREERDP_API HGDIOBJECT gdi_SelectObject(HGDI_DC hdc, HGDIOBJECT hgdiobject);
FREERDP_API BOOL gdi_DeleteObject(HGDIOBJECT hgdiobject);
FREERDP_API BOOL gdi_DeleteDC(HGDI_DC hdc);

#ifdef __cplusplus
 }
#endif

#endif /* FREERDP_GDI_DC_H */
