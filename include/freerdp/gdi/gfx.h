/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Graphics Pipeline
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

#ifndef FREERDP_GDI_GFX_H
#define FREERDP_GDI_GFX_H

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>

struct gdi_gfx_surface
{
	UINT16 surfaceId;
	rdpCodecs* codecs;
	UINT32 width;
	UINT32 height;
	BOOL alpha;
	BYTE* data;
	int scanline;
	UINT32 format;
	BOOL outputMapped;
	UINT32 outputOriginX;
	UINT32 outputOriginY;
	REGION16 invalidRegion;
};
typedef struct gdi_gfx_surface gdiGfxSurface;

struct gdi_gfx_cache_entry
{
	UINT64 cacheKey;
	UINT32 width;
	UINT32 height;
	BOOL alpha;
	BYTE* data;
	int scanline;
	UINT32 format;
};
typedef struct gdi_gfx_cache_entry gdiGfxCacheEntry;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API void gdi_graphics_pipeline_init(rdpGdi* gdi, RdpgfxClientContext* gfx);
FREERDP_API void gdi_graphics_pipeline_uninit(rdpGdi* gdi, RdpgfxClientContext* gfx);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GDI_GFX_H */

