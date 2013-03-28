/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <winpr/crt.h>

#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <winpr/stream.h>

#include <freerdp/cache/nine_grid.h>

void update_gdi_draw_nine_grid(rdpContext* context, DRAW_NINE_GRID_ORDER* draw_nine_grid)
{
	rdpCache* cache = context->cache;
	IFCALL(cache->nine_grid->DrawNineGrid, context, draw_nine_grid);
}

void update_gdi_multi_draw_nine_grid(rdpContext* context, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid)
{
	rdpCache* cache = context->cache;
	IFCALL(cache->nine_grid->MultiDrawNineGrid, context, multi_draw_nine_grid);
}

void nine_grid_cache_register_callbacks(rdpUpdate* update)
{
	rdpCache* cache = update->context->cache;

	cache->nine_grid->DrawNineGrid = update->primary->DrawNineGrid;
	cache->nine_grid->MultiDrawNineGrid = update->primary->MultiDrawNineGrid;

	update->primary->DrawNineGrid = update_gdi_draw_nine_grid;
	update->primary->MultiDrawNineGrid = update_gdi_multi_draw_nine_grid;
}

void* nine_grid_cache_get(rdpNineGridCache* nine_grid, UINT32 index)
{
	void* entry;

	if (index >= nine_grid->maxEntries)
	{
		fprintf(stderr, "invalid NineGrid index: 0x%04X\n", index);
		return NULL;
	}

	entry = nine_grid->entries[index].entry;

	if (entry == NULL)
	{
		fprintf(stderr, "invalid NineGrid at index: 0x%04X\n", index);
		return NULL;
	}

	return entry;
}

void nine_grid_cache_put(rdpNineGridCache* nine_grid, UINT32 index, void* entry)
{
	void* prevEntry;

	if (index >= nine_grid->maxEntries)
	{
		fprintf(stderr, "invalid NineGrid index: 0x%04X\n", index);
		return;
	}

	prevEntry = nine_grid->entries[index].entry;

	if (prevEntry != NULL)
		free(prevEntry);

	nine_grid->entries[index].entry = entry;
}

rdpNineGridCache* nine_grid_cache_new(rdpSettings* settings)
{
	rdpNineGridCache* nine_grid;

	nine_grid = (rdpNineGridCache*) malloc(sizeof(rdpNineGridCache));
	ZeroMemory(nine_grid, sizeof(rdpNineGridCache));

	if (nine_grid != NULL)
	{
		nine_grid->settings = settings;

		nine_grid->maxSize = 2560;
		nine_grid->maxEntries = 256;

		nine_grid->settings->DrawNineGridCacheSize = nine_grid->maxSize;
		nine_grid->settings->DrawNineGridCacheEntries = nine_grid->maxEntries;

		nine_grid->entries = (NINE_GRID_ENTRY*) malloc(sizeof(NINE_GRID_ENTRY) * nine_grid->maxEntries);
		ZeroMemory(nine_grid->entries, sizeof(NINE_GRID_ENTRY) * nine_grid->maxEntries);
	}

	return nine_grid;
}

void nine_grid_cache_free(rdpNineGridCache* nine_grid)
{
	int i;

	if (nine_grid != NULL)
	{
		if (nine_grid->entries != NULL)
		{
			for (i = 0; i < (int) nine_grid->maxEntries; i++)
			{
				if (nine_grid->entries[i].entry != NULL)
					free(nine_grid->entries[i].entry);
			}

			free(nine_grid->entries);
		}

		free(nine_grid);
	}
}
