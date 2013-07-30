/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <winpr/crt.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <winpr/stream.h>

#include <freerdp/cache/bitmap.h>

void update_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;

	if (memblt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, memblt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, (BYTE) memblt->cacheId, memblt->cacheIndex);
	/* XP-SP2 servers sometimes ask for cached bitmaps they've never defined. */
	if (bitmap == NULL) return;

	memblt->bitmap = bitmap;
	IFCALL(cache->bitmap->MemBlt, context, memblt);
}

void update_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	BYTE style;
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;
	rdpBrush* brush = &mem3blt->brush;

	if (mem3blt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, mem3blt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, (BYTE) mem3blt->cacheId, mem3blt->cacheIndex);
	/* XP-SP2 servers sometimes ask for cached bitmaps they've never defined. */
	if (bitmap == NULL) return;

	style = brush->style;

	if (brush->style & CACHED_BRUSH)
	{
		brush->data = brush_cache_get(cache->brush, brush->index, &brush->bpp);
		brush->style = 0x03;
	}

	mem3blt->bitmap = bitmap;
	IFCALL(cache->bitmap->Mem3Blt, context, mem3blt);
	brush->style = style;
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
			cache_bitmap->bitmapBpp, cache_bitmap->bitmapLength,
			cache_bitmap->compressed, RDP_CODEC_ID_NONE);

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
		cache_bitmap_v2->bitmapBpp = context->instance->settings->ColorDepth;
	}

	bitmap->Decompress(context, bitmap,
			cache_bitmap_v2->bitmapDataStream, cache_bitmap_v2->bitmapWidth, cache_bitmap_v2->bitmapHeight,
			cache_bitmap_v2->bitmapBpp, cache_bitmap_v2->bitmapLength,
			cache_bitmap_v2->compressed, RDP_CODEC_ID_NONE);

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cache_bitmap_v2->cacheId, cache_bitmap_v2->cacheIndex);

	if (prevBitmap != NULL)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cache_bitmap_v2->cacheId, cache_bitmap_v2->cacheIndex, bitmap);
}

void update_gdi_cache_bitmap_v3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;
	BITMAP_DATA_EX* bitmapData = &cache_bitmap_v3->bitmapData;

	bitmap = Bitmap_Alloc(context);

	Bitmap_SetDimensions(context, bitmap, bitmapData->width, bitmapData->height);

	if (cache_bitmap_v3->bitmapData.bpp == 0)
	{
		/* Workaround for Windows 8 bug where bitmapBpp is not set */
		cache_bitmap_v3->bitmapData.bpp = context->instance->settings->ColorDepth;
	}

	bitmap->Decompress(context, bitmap,
			bitmapData->data, bitmap->width, bitmap->height,
			bitmapData->bpp, bitmapData->length, TRUE,
			bitmapData->codecID);

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cache_bitmap_v3->cacheId, cache_bitmap_v3->cacheIndex);

	if (prevBitmap != NULL)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cache_bitmap_v3->cacheId, cache_bitmap_v3->cacheIndex, bitmap);
}

void update_gdi_bitmap_update(rdpContext* context, BITMAP_UPDATE* bitmap_update)
{
	int i;
	rdpBitmap* bitmap;
	BITMAP_DATA* bitmap_data;
	BOOL reused = TRUE;
	rdpCache* cache = context->cache;

	if (cache->bitmap->bitmap == NULL)
	{
		cache->bitmap->bitmap = Bitmap_Alloc(context);
		cache->bitmap->bitmap->ephemeral = TRUE;
		reused = FALSE;
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
				bitmap_data->bitsPerPixel, bitmap_data->bitmapLength,
				bitmap_data->compressed, RDP_CODEC_ID_NONE);

		if (reused)
			bitmap->Free(context, bitmap);
		else
			reused = TRUE;

		bitmap->New(context, bitmap);

		bitmap->Paint(context, bitmap);
	}
}

rdpBitmap* bitmap_cache_get(rdpBitmapCache* bitmap_cache, UINT32 id, UINT32 index)
{
	rdpBitmap* bitmap;

	if (id > bitmap_cache->maxCells)
	{
		fprintf(stderr, "get invalid bitmap cell id: %d\n", id);
		return NULL;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
	{
		index = bitmap_cache->cells[id].number;
	}
	else if (index > bitmap_cache->cells[id].number)
	{
		fprintf(stderr, "get invalid bitmap index %d in cell id: %d\n", index, id);
		return NULL;
	}

	bitmap = bitmap_cache->cells[id].entries[index];

	return bitmap;
}

void bitmap_cache_put(rdpBitmapCache* bitmap_cache, UINT32 id, UINT32 index, rdpBitmap* bitmap)
{
	if (id > bitmap_cache->maxCells)
	{
		fprintf(stderr, "put invalid bitmap cell id: %d\n", id);
		return;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
	{
		index = bitmap_cache->cells[id].number;
	}
	else if (index > bitmap_cache->cells[id].number)
	{
		fprintf(stderr, "put invalid bitmap index %d in cell id: %d\n", index, id);
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
	update->secondary->CacheBitmapV3 = update_gdi_cache_bitmap_v3;

	update->BitmapUpdate = update_gdi_bitmap_update;
}

rdpBitmapCache* bitmap_cache_new(rdpSettings* settings)
{
	int i;
	rdpBitmapCache* bitmap_cache;

	bitmap_cache = (rdpBitmapCache*) malloc(sizeof(rdpBitmapCache));
	ZeroMemory(bitmap_cache, sizeof(rdpBitmapCache));

	if (bitmap_cache != NULL)
	{
		bitmap_cache->settings = settings;
		bitmap_cache->update = ((freerdp*) settings->instance)->update;
		bitmap_cache->context = bitmap_cache->update->context;

		bitmap_cache->maxCells = settings->BitmapCacheV2NumCells;

		bitmap_cache->cells = (BITMAP_V2_CELL*) malloc(sizeof(BITMAP_V2_CELL) * bitmap_cache->maxCells);
		ZeroMemory(bitmap_cache->cells, sizeof(BITMAP_V2_CELL) * bitmap_cache->maxCells);

		for (i = 0; i < (int) bitmap_cache->maxCells; i++)
		{
			bitmap_cache->cells[i].number = settings->BitmapCacheV2CellInfo[i].numEntries;
			/* allocate an extra entry for BITMAP_CACHE_WAITING_LIST_INDEX */
			bitmap_cache->cells[i].entries = (rdpBitmap**) malloc(sizeof(rdpBitmap*) * (bitmap_cache->cells[i].number + 1));
			ZeroMemory(bitmap_cache->cells[i].entries, sizeof(rdpBitmap*) * (bitmap_cache->cells[i].number + 1));
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
			for (j = 0; j < (int) bitmap_cache->cells[i].number + 1; j++)
			{
				bitmap = bitmap_cache->cells[i].entries[j];

				if (bitmap != NULL)
				{
					Bitmap_Free(bitmap_cache->context, bitmap);
				}
			}

			free(bitmap_cache->cells[i].entries);
		}

		if (bitmap_cache->bitmap != NULL)
			Bitmap_Free(bitmap_cache->context, bitmap_cache->bitmap);

		free(bitmap_cache->cells);
		free(bitmap_cache);
	}
}
