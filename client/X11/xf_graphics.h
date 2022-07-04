/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Graphical Objects
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_X11_GRAPHICS_H
#define FREERDP_CLIENT_X11_GRAPHICS_H

#include "xf_client.h"
#include "xfreerdp.h"

BOOL xf_register_pointer(rdpGraphics* graphics);
BOOL xf_register_graphics(rdpGraphics* graphics);

BOOL xf_decode_color(xfContext* xfc, const UINT32 srcColor, XColor* color);
UINT32 xf_get_local_color_format(xfContext* xfc, BOOL aligned);

BOOL xf_pointer_update_scale(xfContext* xfc);

#endif /* FREERDP_CLIENT_X11_GRAPHICS_H */
