/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Graphics Pipeline
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __XF_GRAPHICS_PIPELINE_H
#define __XF_GRAPHICS_PIPELINE_H

#include "xf_client.h"
#include "xfreerdp.h"

#include <freerdp/gdi/gfx.h>

struct xf_gfx_surface
{
	UINT16 surfaceId;
	rdpCodecs* codecs;
	UINT32 width;
	UINT32 height;
	BOOL alpha;
	BYTE* data;
	BYTE* stage;
	XImage* image;
	int scanline;
	int stageStep;
	UINT32 format;
	BOOL outputMapped;
	UINT32 outputOriginX;
	UINT32 outputOriginY;
	REGION16 invalidRegion;
};
typedef struct xf_gfx_surface xfGfxSurface;

struct xf_gfx_cache_entry
{
	UINT64 cacheKey;
	UINT32 width;
	UINT32 height;
	BOOL alpha;
	BYTE* data;
	int scanline;
	UINT32 format;
};
typedef struct xf_gfx_cache_entry xfGfxCacheEntry;

int xf_OutputExpose(xfContext* xfc, int x, int y, int width, int height);

void xf_graphics_pipeline_init(xfContext* xfc, RdpgfxClientContext* gfx);
void xf_graphics_pipeline_uninit(xfContext* xfc, RdpgfxClientContext* gfx);

#endif /* __XF_GRAPHICS_PIPELINE_H */
