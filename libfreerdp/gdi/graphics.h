/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphical Objects
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_GDI_GRAPHICS_H
#define FREERDP_LIB_GDI_GRAPHICS_H

#include <freerdp/gdi/gdi.h>
#include <freerdp/graphics.h>
#include <freerdp/api.h>

FREERDP_LOCAL HGDI_BITMAP gdi_create_bitmap(rdpGdi* gdi, UINT32 width, UINT32 height, UINT32 format,
                                            BYTE* data);

FREERDP_LOCAL BOOL gdi_register_graphics(rdpGraphics* graphics);

#endif /* FREERDP_LIB_GDI_GRAPHICS_H */
