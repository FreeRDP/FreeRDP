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
	if (!bitmap)
		return;

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

void update_gdi_cache_bitmap(rdpContext* context, CACHE_BITMAP_ORDER* cacheBitmap)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;

	bitmap = Bitmap_Alloc(context);

	Bitmap_SetDimensions(context, bitmap, cacheBitmap->bitmapWidth, cacheBitmap->bitmapHeight);

	bitmap->Decompress(context, bitmap,
			cacheBitmap->bitmapDataStream, cacheBitmap->bitmapWidth, cacheBitmap->bitmapHeight,
			cacheBitmap->bitmapBpp, cacheBitmap->bitmapLength,
			cacheBitmap->compressed, RDP_CODEC_ID_NONE);

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmap->cacheId, cacheBitmap->cacheIndex);

	if (prevBitmap != NULL)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cacheBitmap->cacheId, cacheBitmap->cacheIndex, bitmap);
}

void update_gdi_cache_bitmap_v2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;

	bitmap = Bitmap_Alloc(context);

	Bitmap_SetDimensions(context, bitmap, cacheBitmapV2->bitmapWidth, cacheBitmapV2->bitmapHeight);

	if (cacheBitmapV2->bitmapBpp == 0)
	{
		/* Workaround for Windows 8 bug where bitmapBpp is not set */
		cacheBitmapV2->bitmapBpp = context->instance->settings->ColorDepth;
	}

	bitmap->Decompress(context, bitmap,
			cacheBitmapV2->bitmapDataStream, cacheBitmapV2->bitmapWidth, cacheBitmapV2->bitmapHeight,
			cacheBitmapV2->bitmapBpp, cacheBitmapV2->bitmapLength,
			cacheBitmapV2->compressed, RDP_CODEC_ID_NONE);

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmapV2->cacheId, cacheBitmapV2->cacheIndex);

	if (prevBitmap != NULL)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cacheBitmapV2->cacheId, cacheBitmapV2->cacheIndex, bitmap);
}

void update_gdi_cache_bitmap_v3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;
	BITMAP_DATA_EX* bitmapData = &cacheBitmapV3->bitmapData;

	bitmap = Bitmap_Alloc(context);

	Bitmap_SetDimensions(context, bitmap, bitmapData->width, bitmapData->height);

	if (cacheBitmapV3->bitmapData.bpp == 0)
	{
		/* Workaround for Windows 8 bug where bitmapBpp is not set */
		cacheBitmapV3->bitmapData.bpp = context->instance->settings->ColorDepth;
	}

	bitmap->Decompress(context, bitmap,
			bitmapData->data, bitmap->width, bitmap->height,
			bitmapData->bpp, bitmapData->length, TRUE,
			bitmapData->codecID);

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmapV3->cacheId, cacheBitmapV3->cacheIndex);

	if (prevBitmap != NULL)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cacheBitmapV3->cacheId, cacheBitmapV3->cacheIndex, bitmap);
}

void update_gdi_bitmap_update(rdpContext* context, BITMAP_UPDATE* bitmapUpdate)
{
	int i;
	rdpBitmap* bitmap;
	BITMAP_DATA* bitmap_data;
	BOOL reused = TRUE;
	rdpCache* cache = context->cache;

	if (!cache->bitmap->bitmap)
	{
		cache->bitmap->bitmap = Bitmap_Alloc(context);
		cache->bitmap->bitmap->ephemeral = TRUE;
		reused = FALSE;
	}

	bitmap = cache->bitmap->bitmap;

	for (i = 0; i < (int) bitmapUpdate->number; i++)
	{
		bitmap_data = &bitmapUpdate->rectangles[i];

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

rdpBitmap* bitmap_cache_get(rdpBitmapCache* bitmapCache, UINT32 id, UINT32 index)
{
	rdpBitmap* bitmap;

	if (id > bitmapCache->maxCells)
	{
		fprintf(stderr, "get invalid bitmap cell id: %d\n", id);
		return NULL;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
	{
		index = bitmapCache->cells[id].number;
	}
	else if (index > bitmapCache->cells[id].number)
	{
		fprintf(stderr, "get invalid bitmap index %d in cell id: %d\n", index, id);
		return NULL;
	}

	bitmap = bitmapCache->cells[id].entries[index];

	return bitmap;
}

void bitmap_cache_put(rdpBitmapCache* bitmapCache, UINT32 id, UINT32 index, rdpBitmap* bitmap)
{
	if (id > bitmapCache->maxCells)
	{
		fprintf(stderr, "put invalid bitmap cell id: %d\n", id);
		return;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
	{
		index = bitmapCache->cells[id].number;
	}
	else if (index > bitmapCache->cells[id].number)
	{
		fprintf(stderr, "put invalid bitmap index %d in cell id: %d\n", index, id);
		return;
	}

	bitmapCache->cells[id].entries[index] = bitmap;
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
	rdpBitmapCache* bitmapCache;

	bitmapCache = (rdpBitmapCache*) malloc(sizeof(rdpBitmapCache));

	if (bitmapCache)
	{
		ZeroMemory(bitmapCache, sizeof(rdpBitmapCache));

		bitmapCache->settings = settings;
		bitmapCache->update = ((freerdp*) settings->instance)->update;
		bitmapCache->context = bitmapCache->update->context;

		bitmapCache->maxCells = settings->BitmapCacheV2NumCells;

		bitmapCache->cells = (BITMAP_V2_CELL*) malloc(sizeof(BITMAP_V2_CELL) * bitmapCache->maxCells);
		ZeroMemory(bitmapCache->cells, sizeof(BITMAP_V2_CELL) * bitmapCache->maxCells);

		for (i = 0; i < (int) bitmapCache->maxCells; i++)
		{
			bitmapCache->cells[i].number = settings->BitmapCacheV2CellInfo[i].numEntries;
			/* allocate an extra entry for BITMAP_CACHE_WAITING_LIST_INDEX */
			bitmapCache->cells[i].entries = (rdpBitmap**) malloc(sizeof(rdpBitmap*) * (bitmapCache->cells[i].number + 1));
			ZeroMemory(bitmapCache->cells[i].entries, sizeof(rdpBitmap*) * (bitmapCache->cells[i].number + 1));
		}
	}

	return bitmapCache;
}

void bitmap_cache_free(rdpBitmapCache* bitmapCache)
{
	int i, j;
	rdpBitmap* bitmap;

	if (bitmapCache != NULL)
	{
		for (i = 0; i < (int) bitmapCache->maxCells; i++)
		{
			for (j = 0; j < (int) bitmapCache->cells[i].number + 1; j++)
			{
				bitmap = bitmapCache->cells[i].entries[j];

				if (bitmap != NULL)
				{
					Bitmap_Free(bitmapCache->context, bitmap);
				}
			}

			free(bitmapCache->cells[i].entries);
		}

		if (bitmapCache->bitmap)
			Bitmap_Free(bitmapCache->context, bitmapCache->bitmap);

		free(bitmapCache->cells);
		free(bitmapCache);
	}
}
