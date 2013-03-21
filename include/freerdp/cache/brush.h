/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Brush Cache
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

#ifndef FREERDP_BRUSH_CACHE_H
#define FREERDP_BRUSH_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/update.h>

#include <winpr/stream.h>

typedef struct _BRUSH_ENTRY BRUSH_ENTRY;
typedef struct rdp_brush_cache rdpBrushCache;

#include <freerdp/cache/cache.h>

struct _BRUSH_ENTRY
{
	UINT32 bpp;
	void* entry;
};

struct rdp_brush_cache
{
	pPatBlt PatBlt; /* 0 */
	pCacheBrush CacheBrush; /* 1 */
	pPolygonSC PolygonSC; /* 2 */
	pPolygonCB PolygonCB; /* 3 */
	UINT32 paddingA[16 - 4]; /* 4 */

	UINT32 maxEntries; /* 16 */
	UINT32 maxMonoEntries; /* 17 */
	BRUSH_ENTRY* entries; /* 18 */
	BRUSH_ENTRY* monoEntries; /* 19 */
	UINT32 paddingB[32 - 20]; /* 20 */

	/* internal */

	rdpSettings* settings;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API void* brush_cache_get(rdpBrushCache* brush, UINT32 index, UINT32* bpp);
FREERDP_API void brush_cache_put(rdpBrushCache* brush, UINT32 index, void* entry, UINT32 bpp);

FREERDP_API void brush_cache_register_callbacks(rdpUpdate* update);

FREERDP_API rdpBrushCache* brush_cache_new(rdpSettings* settings);
FREERDP_API void brush_cache_free(rdpBrushCache* brush);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_BRUSH_CACHE_H */
