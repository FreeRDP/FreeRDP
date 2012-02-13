/**
 * FreeRDP: A Remote Desktop Protocol Client
 * NineGrid Cache
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __NINE_GRID_CACHE_H
#define __NINE_GRID_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/update.h>
#include <freerdp/utils/stream.h>

typedef struct _NINE_GRID_ENTRY NINE_GRID_ENTRY;
typedef struct rdp_nine_grid_cache rdpNineGridCache;

#include <freerdp/cache/cache.h>

struct _NINE_GRID_ENTRY
{
	void* entry;
};

struct rdp_nine_grid_cache
{
	pDrawNineGrid DrawNineGrid; /* 0 */
	pMultiDrawNineGrid MultiDrawNineGrid; /* 1 */
	uint32 paddingA[16 - 2]; /* 2 */

	uint32 maxEntries; /* 16 */
	uint32 maxSize; /* 17 */
	NINE_GRID_ENTRY* entries; /* 18 */
	uint32 paddingB[32 - 19]; /* 19 */

	/* internal */

	rdpSettings* settings;
};

FREERDP_API void* nine_grid_cache_get(rdpNineGridCache* nine_grid, uint32 index);
FREERDP_API void nine_grid_cache_put(rdpNineGridCache* nine_grid, uint32 index, void* entry);

FREERDP_API void nine_grid_cache_register_callbacks(rdpUpdate* update);

FREERDP_API rdpNineGridCache* nine_grid_cache_new(rdpSettings* settings);
FREERDP_API void nine_grid_cache_free(rdpNineGridCache* nine_grid);

#endif /* __NINE_GRID_CACHE_H */
