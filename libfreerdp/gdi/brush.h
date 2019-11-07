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

#ifndef FREERDP_LIB_GDI_BRUSH_H
#define FREERDP_LIB_GDI_BRUSH_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL const char* gdi_rop_to_string(UINT32 code);

	FREERDP_LOCAL HGDI_BRUSH gdi_CreateSolidBrush(UINT32 crColor);
	FREERDP_LOCAL HGDI_BRUSH gdi_CreatePatternBrush(HGDI_BITMAP hbmp);
	FREERDP_LOCAL HGDI_BRUSH gdi_CreateHatchBrush(HGDI_BITMAP hbmp);

	static INLINE UINT32 gdi_GetBrushStyle(HGDI_DC hdc)
	{
		if (!hdc || !hdc->brush)
			return GDI_BS_NULL;

		return hdc->brush->style;
	}

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_GDI_BRUSH_H */
