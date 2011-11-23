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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/cache/palette.h>

void update_gdi_cache_color_table(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cache_color_table)
{
	rdpCache* cache = context->cache;
	palette_cache_put(cache->palette, cache_color_table->cacheIndex, (void*) cache_color_table->colorTable);
}

void* palette_cache_get(rdpPaletteCache* palette_cache, uint32 index)
{
	void* entry;

	if (index > palette_cache->maxEntries)
	{
		printf("invalid color table index: 0x%04X\n", index);
		return NULL;
	}

	entry = palette_cache->entries[index].entry;

	if (entry == NULL)
	{
		printf("invalid color table at index: 0x%04X\n", index);
		return NULL;
	}

	return entry;
}

void palette_cache_put(rdpPaletteCache* palette_cache, uint32 index, void* entry)
{
	if (index > palette_cache->maxEntries)
	{
		printf("invalid color table index: 0x%04X\n", index);
		return;
	}

	palette_cache->entries[index].entry = entry;
}

void palette_cache_register_callbacks(rdpUpdate* update)
{
	update->secondary->CacheColorTable = update_gdi_cache_color_table;
}

rdpPaletteCache* palette_cache_new(rdpSettings* settings)
{
	rdpPaletteCache* palette_cache;

	palette_cache = (rdpPaletteCache*) xzalloc(sizeof(rdpPaletteCache));

	if (palette_cache != NULL)
	{
		palette_cache->settings = settings;
		palette_cache->maxEntries = 6;
		palette_cache->entries = (PALETTE_TABLE_ENTRY*) xzalloc(sizeof(PALETTE_TABLE_ENTRY) * palette_cache->maxEntries);
	}

	return palette_cache;
}

void palette_cache_free(rdpPaletteCache* palette_cache)
{
	if (palette_cache != NULL)
	{
		xfree(palette_cache->entries);
		xfree(palette_cache);
	}
}

