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

void bitmap_free(rdpBitmap* bitmap)
{
	if (bitmap != NULL)
	{
		if (bitmap->dstData != NULL)
			xfree(bitmap->dstData);

		xfree(bitmap);
	}
}

void update_gdi_memblt(rdpUpdate* update, MEMBLT_ORDER* memblt)
{
	rdpBitmap* bitmap;
	rdpCache* cache = (rdpCache*) update->cache;

	if (memblt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, memblt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, memblt->cacheId, memblt->cacheIndex);

	memblt->bitmap = bitmap;
	IFCALL(cache->bitmap->MemBlt, update, memblt);
}

void update_gdi_mem3blt(rdpUpdate* update, MEM3BLT_ORDER* mem3blt)
{
	rdpBitmap* bitmap;
	rdpCache* cache = (rdpCache*) update->cache;

	if (mem3blt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, mem3blt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, mem3blt->cacheId, mem3blt->cacheIndex);

	mem3blt->bitmap = bitmap;
	IFCALL(cache->bitmap->Mem3Blt, update, mem3blt);
}

void update_gdi_cache_bitmap(rdpUpdate* update, CACHE_BITMAP_V2_ORDER* cache_bitmap)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	uint32 size = sizeof(rdpBitmap);
	rdpCache* cache = (rdpCache*) update->cache;

	bitmap = cache_bitmap->bitmap;
	IFCALL(cache->bitmap->BitmapSize, update, &size);
	bitmap = (rdpBitmap*) xrealloc(bitmap, size);

	IFCALL(cache->bitmap->BitmapNew, update, bitmap);
	cache_bitmap->bitmap = bitmap;

	prevBitmap = bitmap_cache_get(cache->bitmap, cache_bitmap->cacheId, cache_bitmap->cacheIndex);

	if (prevBitmap != NULL)
	{
		IFCALL(cache->bitmap->BitmapFree, update, prevBitmap);
		bitmap_free(prevBitmap);
	}

	bitmap_cache_put(cache->bitmap, cache_bitmap->cacheId, cache_bitmap->cacheIndex, bitmap);
}

rdpBitmap* bitmap_cache_get(rdpBitmapCache* bitmap_cache, uint8 id, uint16 index)
{
	rdpBitmap* bitmap;

	if (id > bitmap_cache->maxCells)
	{
		printf("get invalid bitmap cell id: %d\n", id);
		return NULL;
	}

	if (index == 0x7FFF)
		index = bitmap_cache->cells[id].number - 1;

	if (index > bitmap_cache->cells[id].number)
	{
		printf("get invalid bitmap index %d in cell id: %d\n", index, id);
		return NULL;
	}

	bitmap = bitmap_cache->cells[id].entries[index];

	return bitmap;
}

void bitmap_cache_put(rdpBitmapCache* bitmap_cache, uint8 id, uint16 index, rdpBitmap* bitmap)
{
	if (id > bitmap_cache->maxCells)
	{
		printf("put invalid bitmap cell id: %d\n", id);
		return;
	}

	if (index == 0x7FFF)
		index = bitmap_cache->cells[id].number - 1;

	if (index > bitmap_cache->cells[id].number)
	{
		printf("put invalid bitmap index %d in cell id: %d\n", index, id);
		return;
	}

	bitmap_cache->cells[id].entries[index] = bitmap;
}

void bitmap_cache_register_callbacks(rdpUpdate* update)
{
	rdpCache* cache = (rdpCache*) update->cache;

	cache->bitmap->MemBlt = update->MemBlt;
	cache->bitmap->Mem3Blt = update->Mem3Blt;

	update->MemBlt = update_gdi_memblt;
	update->Mem3Blt = update_gdi_mem3blt;
	update->CacheBitmapV2 = update_gdi_cache_bitmap;
}

rdpBitmapCache* bitmap_cache_new(rdpSettings* settings)
{
	int i;
	rdpBitmapCache* bitmap_cache;

	bitmap_cache = (rdpBitmapCache*) xzalloc(sizeof(rdpBitmapCache));

	if (bitmap_cache != NULL)
	{
		bitmap_cache->settings = settings;
		bitmap_cache->update = ((freerdp*) settings->instance)->update;

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
			bitmap_cache->cells[i].entries = (rdpBitmap**) xzalloc(sizeof(rdpBitmap*) * bitmap_cache->cells[i].number);
		}
	}

	return bitmap_cache;
}

void bitmap_cache_free(rdpBitmapCache* bitmap_cache)
{
	int i, j;
	rdpBitmap* bitmap;

	if (bitmap_cache != NULL)
	{
		for (i = 0; i < bitmap_cache->maxCells; i++)
		{
			for (j = 0; j < (int) bitmap_cache->cells[i].number; j++)
			{
				bitmap = bitmap_cache->cells[i].entries[j];

				if (bitmap != NULL)
				{
					IFCALL(bitmap_cache->BitmapFree, bitmap_cache->update, bitmap);
					bitmap_free(bitmap);
				}
			}

			xfree(bitmap_cache->cells[i].entries);
		}

		xfree(bitmap_cache->cells);
		xfree(bitmap_cache);
	}
}
