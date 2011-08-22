/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Pen Functions
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

#ifndef __GDI_PEN_H
#define __GDI_PEN_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

FREERDP_API HGDI_PEN gdi_CreatePen(int fnPenStyle, int nWidth, int crColor);
FREERDP_API uint8 gdi_GetPenColor_8bpp(HGDI_PEN pen);
FREERDP_API uint16 gdi_GetPenColor_16bpp(HGDI_PEN pen);
FREERDP_API uint32 gdi_GetPenColor_32bpp(HGDI_PEN pen);

#endif /* __GDI_PEN_H */
