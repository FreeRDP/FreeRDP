/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Graphics Pipeline
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Thincast Technologies GmbH
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
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

#ifndef FREERDP_CLIENT_X11_GFX_H
#define FREERDP_CLIENT_X11_GFX_H

#include "xf_client.h"
#include "xfreerdp.h"

#include <freerdp/gdi/gfx.h>

struct xf_gfx_surface
{
	gdiGfxSurface gdi;
	BYTE* stage;
	UINT32 stageScanline;
	XImage* image;
};
typedef struct xf_gfx_surface xfGfxSurface;

UINT xf_OutputExpose(xfContext* xfc, UINT32 x, UINT32 y, UINT32 width, UINT32 height);

void xf_graphics_pipeline_init(xfContext* xfc, RdpgfxClientContext* gfx);

void xf_graphics_pipeline_uninit(xfContext* xfc, RdpgfxClientContext* gfx);

#endif /* FREERDP_CLIENT_X11_GFX_H */
