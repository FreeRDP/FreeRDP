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

#include <freerdp/log.h>
#include <freerdp/cache/palette.h>

#include "palette.h"

#define TAG FREERDP_TAG("cache.palette")

static void* palette_cache_get(rdpPaletteCache* palette, UINT32 index);

static void palette_cache_put(rdpPaletteCache* palette, UINT32 index, void* entry);

static BOOL update_gdi_cache_color_table(rdpContext* context,
                                         const CACHE_COLOR_TABLE_ORDER* cacheColorTable)
{
	UINT32* colorTable;
	rdpCache* cache = context->cache;
	colorTable = (UINT32*)malloc(sizeof(UINT32) * 256);

	if (!colorTable)
		return FALSE;

	CopyMemory(colorTable, cacheColorTable->colorTable, sizeof(UINT32) * 256);
	palette_cache_put(cache->palette, cacheColorTable->cacheIndex, (void*)colorTable);
	return TRUE;
}

void* palette_cache_get(rdpPaletteCache* paletteCache, UINT32 index)
{
	void* entry;

	if (index >= paletteCache->maxEntries)
	{
		WLog_ERR(TAG, "invalid color table index: 0x%08" PRIX32 "", index);
		return NULL;
	}

	entry = paletteCache->entries[index].entry;

	if (!entry)
	{
		WLog_ERR(TAG, "invalid color table at index: 0x%08" PRIX32 "", index);
		return NULL;
	}

	return entry;
}

void palette_cache_put(rdpPaletteCache* paletteCache, UINT32 index, void* entry)
{
	if (index >= paletteCache->maxEntries)
	{
		WLog_ERR(TAG, "invalid color table index: 0x%08" PRIX32 "", index);
		free(entry);
		return;
	}

	free(paletteCache->entries[index].entry);
	paletteCache->entries[index].entry = entry;
}

void palette_cache_register_callbacks(rdpUpdate* update)
{
	update->secondary->CacheColorTable = update_gdi_cache_color_table;
}

rdpPaletteCache* palette_cache_new(rdpSettings* settings)
{
	rdpPaletteCache* paletteCache;
	paletteCache = (rdpPaletteCache*)calloc(1, sizeof(rdpPaletteCache));

	if (paletteCache)
	{
		paletteCache->settings = settings;
		paletteCache->maxEntries = 6;
		paletteCache->entries =
		    (PALETTE_TABLE_ENTRY*)calloc(paletteCache->maxEntries, sizeof(PALETTE_TABLE_ENTRY));
	}

	return paletteCache;
}

void palette_cache_free(rdpPaletteCache* paletteCache)
{
	if (paletteCache)
	{
		UINT32 i;

		for (i = 0; i < paletteCache->maxEntries; i++)
			free(paletteCache->entries[i].entry);

		free(paletteCache->entries);
		free(paletteCache);
	}
}

void free_palette_update(rdpContext* context, PALETTE_UPDATE* pointer)
{
	free(pointer);
}

PALETTE_UPDATE* copy_palette_update(rdpContext* context, const PALETTE_UPDATE* pointer)
{
	PALETTE_UPDATE* dst = calloc(1, sizeof(PALETTE_UPDATE));

	if (!dst || !pointer)
		goto fail;

	*dst = *pointer;
	return dst;
fail:
	free_palette_update(context, dst);
	return NULL;
}
