/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <winpr/crt.h>

#include <freerdp/cache/palette.h>

void update_gdi_cache_color_table(rdpContext* context, CACHE_COLOR_TABLE_ORDER* cache_color_table)
{
	UINT32* colorTable;
	rdpCache* cache = context->cache;

	colorTable = (UINT32*) malloc(sizeof(UINT32) * 256);
	CopyMemory(colorTable, cache_color_table->colorTable, sizeof(UINT32) * 256);

	palette_cache_put(cache->palette, cache_color_table->cacheIndex, (void*) colorTable);
}

void* palette_cache_get(rdpPaletteCache* palette_cache, UINT32 index)
{
	void* entry;

	if (index >= palette_cache->maxEntries)
	{
		fprintf(stderr, "invalid color table index: 0x%04X\n", index);
		return NULL;
	}

	entry = palette_cache->entries[index].entry;

	if (entry == NULL)
	{
		fprintf(stderr, "invalid color table at index: 0x%04X\n", index);
		return NULL;
	}

	return entry;
}

void palette_cache_put(rdpPaletteCache* palette_cache, UINT32 index, void* entry)
{
	if (index >= palette_cache->maxEntries)
	{
		fprintf(stderr, "invalid color table index: 0x%04X\n", index);
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

	palette_cache = (rdpPaletteCache*) malloc(sizeof(rdpPaletteCache));
	ZeroMemory(palette_cache, sizeof(rdpPaletteCache));

	if (palette_cache != NULL)
	{
		palette_cache->settings = settings;
		palette_cache->maxEntries = 6;
		palette_cache->entries = (PALETTE_TABLE_ENTRY*) malloc(sizeof(PALETTE_TABLE_ENTRY) * palette_cache->maxEntries);
		ZeroMemory(palette_cache->entries, sizeof(PALETTE_TABLE_ENTRY) * palette_cache->maxEntries);
	}

	return palette_cache;
}

void palette_cache_free(rdpPaletteCache* palette_cache)
{
	if (palette_cache != NULL)
	{
		free(palette_cache->entries);
		free(palette_cache);
	}
}
