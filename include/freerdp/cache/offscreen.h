/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Offscreen Bitmap Cache
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

#ifndef __OFFSCREEN_CACHE_H
#define __OFFSCREEN_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

typedef struct rdp_offscreen_cache rdpOffscreenCache;

#include <freerdp/cache/cache.h>

struct rdp_offscreen_cache
{
	uint32 maxSize; /* 0 */
	uint32 maxEntries; /* 1 */
	rdpBitmap** entries; /* 2 */
	uint32 currentSurface; /* 3 */

	/* internal */

	rdpUpdate* update;
	rdpSettings* settings;
};

FREERDP_API rdpBitmap* offscreen_cache_get(rdpOffscreenCache* offscreen_cache, uint32 index);
FREERDP_API void offscreen_cache_put(rdpOffscreenCache* offscreen_cache, uint32 index, rdpBitmap* bitmap);
FREERDP_API void offscreen_cache_delete(rdpOffscreenCache* offscreen, uint32 index);

FREERDP_API void offscreen_cache_register_callbacks(rdpUpdate* update);

FREERDP_API rdpOffscreenCache* offscreen_cache_new(rdpSettings* settings);
FREERDP_API void offscreen_cache_free(rdpOffscreenCache* offscreen);

#endif /* __OFFSCREEN_CACHE_H */
