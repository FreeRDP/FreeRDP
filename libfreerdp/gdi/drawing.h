/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Drawing Functions
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

#ifndef FREERDP_LIB_GDI_DRAWING_H
#define FREERDP_LIB_GDI_DRAWING_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL INT32 gdi_GetROP2(HGDI_DC hdc);
	FREERDP_LOCAL INT32 gdi_SetROP2(HGDI_DC hdc, INT32 fnDrawMode);
	FREERDP_LOCAL UINT32 gdi_GetBkColor(HGDI_DC hdc);
	FREERDP_LOCAL UINT32 gdi_SetBkColor(HGDI_DC hdc, UINT32 crColor);
	FREERDP_LOCAL UINT32 gdi_GetBkMode(HGDI_DC hdc);
	FREERDP_LOCAL INT32 gdi_SetBkMode(HGDI_DC hdc, INT32 iBkMode);
	FREERDP_LOCAL UINT32 gdi_SetTextColor(HGDI_DC hdc, UINT32 crColor);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_GDI_DRAWING_H */
