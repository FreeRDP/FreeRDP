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

#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/cache/bitmap.h>

void update_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;

	if (memblt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, memblt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, (uint8) memblt->cacheId, memblt->cacheIndex);

	memblt->bitmap = bitmap;
	IFCALL(cache->bitmap->MemBlt, context, memblt);
}

void update_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;

	if (mem3blt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, mem3blt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, (uint8) mem3blt->cacheId, mem3blt->cacheIndex);

	mem3blt->bitmap = bitmap;
	IFCALL(cache->bitmap->Mem3Blt, context, mem3blt);
}

void update_gdi_cache_bitmap(rdpContext* context, CACHE_BITMAP_ORDER* cache_bitmap)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;

	bitmap = Bitmap_Alloc(context);

	Bitmap_SetDimensions(context, bitmap, cache_bitmap->bitmapWidth, cache_bitmap->bitmapHeight);

	bitmap->Decompress(context, bitmap,
			cache_bitmap->bitmapDataStream, cache_bitmap->bitmapWidth, cache_bitmap->bitmapHeight,
			cache_bitmap->bitmapBpp, cache_bitmap->bitmapLength, cache_bitmap->compressed);

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cache_bitmap->cacheId, cache_bitmap->cacheIndex);

	if (prevBitmap != NULL)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cache_bitmap->cacheId, cache_bitmap->cacheIndex, bitmap);
}

void update_gdi_cache_bitmap_v2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;

	bitmap = Bitmap_Alloc(context);

	Bitmap_SetDimensions(context, bitmap, cache_bitmap_v2->bitmapWidth, cache_bitmap_v2->bitmapHeight);

	if (cache_bitmap_v2->bitmapBpp == 0)
	{
		/* Workaround for Windows 8 bug where bitmapBpp is not set */
		cache_bitmap_v2->bitmapBpp = context->instance->settings->color_depth;
	}

	bitmap->Decompress(context, bitmap,
			cache_bitmap_v2->bitmapDataStream, cache_bitmap_v2->bitmapWidth, cache_bitmap_v2->bitmapHeight,
			cache_bitmap_v2->bitmapBpp, cache_bitmap_v2->bitmapLength, cache_bitmap_v2->compressed);

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cache_bitmap_v2->cacheId, cache_bitmap_v2->cacheIndex);

	if (prevBitmap != NULL)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cache_bitmap_v2->cacheId, cache_bitmap_v2->cacheIndex, bitmap);
}

void update_gdi_bitmap_update(rdpContext* context, BITMAP_UPDATE* bitmap_update)
{
	int i;
	rdpBitmap* bitmap;
	BITMAP_DATA* bitmap_data;
	boolean reused = true;
	rdpCache* cache = context->cache;

	if (cache->bitmap->bitmap == NULL)
	{
		cache->bitmap->bitmap = Bitmap_Alloc(context);
		cache->bitmap->bitmap->ephemeral = true;
		reused = false;
	}

	bitmap = cache->bitmap->bitmap;

	for (i = 0; i < (int) bitmap_update->number; i++)
	{
		bitmap_data = &bitmap_update->rectangles[i];

		bitmap->bpp = bitmap_data->bitsPerPixel;
		bitmap->length = bitmap_data->bitmapLength;
		bitmap->compressed = bitmap_data->compressed;

		Bitmap_SetRectangle(context, bitmap,
				bitmap_data->destLeft, bitmap_data->destTop,
				bitmap_data->destRight, bitmap_data->destBottom);

		Bitmap_SetDimensions(context, bitmap, bitmap_data->width, bitmap_data->height);

		bitmap->Decompress(context, bitmap,
				bitmap_data->bitmapDataStream, bitmap_data->width, bitmap_data->height,
				bitmap_data->bitsPerPixel, bitmap_data->bitmapLength, bitmap_data->compressed);

		if (reused)
			bitmap->Free(context, bitmap);
		else
			reused = true;

		bitmap->New(context, bitmap);

		bitmap->Paint(context, bitmap);
	}
}

rdpBitmap* bitmap_cache_get(rdpBitmapCache* bitmap_cache, uint32 id, uint32 index)
{
	rdpBitmap* bitmap;

	if (id > bitmap_cache->maxCells)
	{
		printf("get invalid bitmap cell id: %d\n", id);
		return NULL;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
		index = bitmap_cache->cells[id].number - 1;

	if (index > bitmap_cache->cells[id].number)
	{
		printf("get invalid bitmap index %d in cell id: %d\n", index, id);
		return NULL;
	}

	bitmap = bitmap_cache->cells[id].entries[index];

	return bitmap;
}

void bitmap_cache_put(rdpBitmapCache* bitmap_cache, uint32 id, uint32 index, rdpBitmap* bitmap)
{
	if (id > bitmap_cache->maxCells)
	{
		printf("put invalid bitmap cell id: %d\n", id);
		return;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
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
	rdpCache* cache = update->context->cache;

	cache->bitmap->MemBlt = update->primary->MemBlt;
	cache->bitmap->Mem3Blt = update->primary->Mem3Blt;

	update->primary->MemBlt = update_gdi_memblt;
	update->primary->Mem3Blt = update_gdi_mem3blt;

	update->secondary->CacheBitmap = update_gdi_cache_bitmap;
	update->secondary->CacheBitmapV2 = update_gdi_cache_bitmap_v2;

	update->BitmapUpdate = update_gdi_bitmap_update;
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
		bitmap_cache->context = bitmap_cache->update->context;

		bitmap_cache->maxCells = 5;

		settings->bitmap_cache = false;
		settings->bitmapCacheV2NumCells = 5;
		settings->bitmapCacheV2CellInfo[0].numEntries = 600;
		settings->bitmapCacheV2CellInfo[0].persistent = false;
		settings->bitmapCacheV2CellInfo[1].numEntries = 600;
		settings->bitmapCacheV2CellInfo[1].persistent = false;
		settings->bitmapCacheV2CellInfo[2].numEntries = 2048;
		settings->bitmapCacheV2CellInfo[2].persistent = false;
		settings->bitmapCacheV2CellInfo[3].numEntries = 4096;
		settings->bitmapCacheV2CellInfo[3].persistent = false;
		settings->bitmapCacheV2CellInfo[4].numEntries = 2048;
		settings->bitmapCacheV2CellInfo[4].persistent = false;

		bitmap_cache->cells = (BITMAP_V2_CELL*) xzalloc(sizeof(BITMAP_V2_CELL) * bitmap_cache->maxCells);

		for (i = 0; i < (int) bitmap_cache->maxCells; i++)
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
		for (i = 0; i < (int) bitmap_cache->maxCells; i++)
		{
			for (j = 0; j < (int) bitmap_cache->cells[i].number; j++)
			{
				bitmap = bitmap_cache->cells[i].entries[j];

				if (bitmap != NULL)
				{
					Bitmap_Free(bitmap_cache->context, bitmap);
				}
			}

			xfree(bitmap_cache->cells[i].entries);
		}

		if (bitmap_cache->bitmap != NULL)
			Bitmap_Free(bitmap_cache->context, bitmap_cache->bitmap);

		xfree(bitmap_cache->cells);
		xfree(bitmap_cache);
	}
}
