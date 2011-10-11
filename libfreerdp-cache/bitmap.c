/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Bitmap Cache V2
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

#include <freerdp/cache/bitmap.h>

void* bitmap_cache_get(rdpBitmapCache* bitmap_cache, uint8 id, uint16 index, void** extra)
{
	void* entry;

	if (id > bitmap_cache->maxCells)
	{
		printf("get invalid bitmap_v2 cell id: %d\n", id);
		return NULL;
	}

	if (index == 0x7FFF)
		index = bitmap_cache->cells[id].number - 1;

	if (index > bitmap_cache->cells[id].number)
	{
		printf("get invalid bitmap_v2 index %d in cell id: %d\n", index, id);
		return NULL;
	}

	entry = bitmap_cache->cells[id].entries[index].entry;

	if (extra != NULL)
		*extra = bitmap_cache->cells[id].entries[index].extra;

	if (entry == NULL)
	{
		printf("get invalid bitmap_v2 at index %d in cell id: %d\n", index, id);
		return NULL;
	}

	return entry;
}

void bitmap_cache_put(rdpBitmapCache* bitmap_cache, uint8 id, uint16 index, void* entry, void* extra)
{
	if (id > bitmap_cache->maxCells)
	{
		printf("put invalid bitmap_v2 cell id: %d\n", id);
		return;
	}

	if (index == 0x7FFF)
		index = bitmap_cache->cells[id].number - 1;

	if (index > bitmap_cache->cells[id].number)
	{
		printf("put invalid bitmap_v2 index %d in cell id: %d\n", index, id);
		return;
	}

	bitmap_cache->cells[id].entries[index].entry = entry;
	bitmap_cache->cells[id].entries[index].extra = extra;
}

rdpBitmapCache* bitmap_cache_new(rdpSettings* settings)
{
	int i;
	rdpBitmapCache* bitmap_cache;

	bitmap_cache = (rdpBitmapCache*) xzalloc(sizeof(rdpBitmapCache));

	if (bitmap_cache != NULL)
	{
		bitmap_cache->settings = settings;

		bitmap_cache->maxCells = 5;

		settings->bitmap_cache = False;
		settings->bitmapCacheV2NumCells = 5;
		settings->bitmapCacheV2CellInfo[0].numEntries = 600;
		settings->bitmapCacheV2CellInfo[0].persistent = False;
		settings->bitmapCacheV2CellInfo[1].numEntries = 600;
		settings->bitmapCacheV2CellInfo[1].persistent = False;
		settings->bitmapCacheV2CellInfo[2].numEntries = 2048;
		settings->bitmapCacheV2CellInfo[2].persistent = False;
		settings->bitmapCacheV2CellInfo[3].numEntries = 4096;
		settings->bitmapCacheV2CellInfo[3].persistent = False;
		settings->bitmapCacheV2CellInfo[4].numEntries = 2048;
		settings->bitmapCacheV2CellInfo[4].persistent = False;

		bitmap_cache->cells = (BITMAP_V2_CELL*) xzalloc(sizeof(BITMAP_V2_CELL) * bitmap_cache->maxCells);

		for (i = 0; i < bitmap_cache->maxCells; i++)
		{
			bitmap_cache->cells[i].number = settings->bitmapCacheV2CellInfo[i].numEntries;
			bitmap_cache->cells[i].entries = (BITMAP_V2_ENTRY*) xzalloc(sizeof(BITMAP_V2_ENTRY) * bitmap_cache->cells[i].number);
		}
	}

	return bitmap_cache;
}

void bitmap_cache_free(rdpBitmapCache* bitmap_cache)
{
	int i, j;

	if (bitmap_cache != NULL)
	{
		for (i = 0; i < bitmap_cache->maxCells; i++)
		{
			for (j = 0; j < (int) bitmap_cache->cells[i].number; j++)
			{
				if (bitmap_cache->cells[i].entries[j].entry != NULL)
					xfree(bitmap_cache->cells[i].entries[j].entry);
			}

			xfree(bitmap_cache->cells[i].entries);
		}

		xfree(bitmap_cache->cells);
		xfree(bitmap_cache);
	}
}
