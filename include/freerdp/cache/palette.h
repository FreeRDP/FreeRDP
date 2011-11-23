/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Palette (Color Table) Cache
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

#ifndef __PALETTE_CACHE_H
#define __PALETTE_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

typedef struct _PALETTE_TABLE_ENTRY PALETTE_TABLE_ENTRY;
typedef struct rdp_palette_cache rdpPaletteCache;

#include <freerdp/cache/cache.h>

struct _PALETTE_TABLE_ENTRY
{
	void* entry;
};

struct rdp_palette_cache
{
	uint32 maxEntries; /* 0 */
	PALETTE_TABLE_ENTRY* entries; /* 1 */

	/* internal */

	rdpSettings* settings;
};

FREERDP_API void* palette_cache_get(rdpPaletteCache* palette, uint32 index);
FREERDP_API void palette_cache_put(rdpPaletteCache* palette, uint32 index, void* entry);

FREERDP_API void palette_cache_register_callbacks(rdpUpdate* update);

FREERDP_API rdpPaletteCache* palette_cache_new(rdpSettings* settings);
FREERDP_API void palette_cache_free(rdpPaletteCache* palette_cache);

#endif /* __PALETTE_CACHE_H */
