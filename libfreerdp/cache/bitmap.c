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

#include <freerdp/log.h>
#include <freerdp/cache/bitmap.h>

#define TAG FREERDP_TAG("cache.bitmap")

BOOL update_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;

	if (memblt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, memblt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, (BYTE) memblt->cacheId, memblt->cacheIndex);
	/* XP-SP2 servers sometimes ask for cached bitmaps they've never defined. */
	if (bitmap == NULL)
		return TRUE;

	memblt->bitmap = bitmap;
	return IFCALLRESULT(TRUE, cache->bitmap->MemBlt, context, memblt);
}

BOOL update_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	BYTE style;
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;
	rdpBrush* brush = &mem3blt->brush;
	BOOL ret = TRUE;

	if (mem3blt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, mem3blt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, (BYTE) mem3blt->cacheId, mem3blt->cacheIndex);

	/* XP-SP2 servers sometimes ask for cached bitmaps they've never defined. */
	if (!bitmap)
		return TRUE;

	style = brush->style;

	if (brush->style & CACHED_BRUSH)
	{
		brush->data = brush_cache_get(cache->brush, brush->index, &brush->bpp);
		if (!brush->data)
			return FALSE;
		brush->style = 0x03;
	}

	mem3blt->bitmap = bitmap;
	IFCALLRET(cache->bitmap->Mem3Blt, ret, context, mem3blt);
	brush->style = style;
	return ret;
}

BOOL update_gdi_cache_bitmap(rdpContext* context, CACHE_BITMAP_ORDER* cacheBitmap)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;

	bitmap = Bitmap_Alloc(context);
	if (!bitmap)
		return FALSE;


	Bitmap_SetDimensions(context, bitmap, cacheBitmap->bitmapWidth, cacheBitmap->bitmapHeight);

	if (!bitmap->Decompress(context, bitmap,
			cacheBitmap->bitmapDataStream, cacheBitmap->bitmapWidth, cacheBitmap->bitmapHeight,
			cacheBitmap->bitmapBpp, cacheBitmap->bitmapLength,
			cacheBitmap->compressed, RDP_CODEC_ID_NONE))
	{
		Bitmap_Free(context, bitmap);
		return FALSE;
	}

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmap->cacheId, cacheBitmap->cacheIndex);

	if (prevBitmap != NULL)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cacheBitmap->cacheId, cacheBitmap->cacheIndex, bitmap);
	return TRUE;
}

BOOL update_gdi_cache_bitmap_v2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2)

{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;
	rdpSettings* settings = context->settings;

	bitmap = Bitmap_Alloc(context);
	if (!bitmap)
		return FALSE;

	Bitmap_SetDimensions(context, bitmap, cacheBitmapV2->bitmapWidth, cacheBitmapV2->bitmapHeight);

	if (!cacheBitmapV2->bitmapBpp)
		cacheBitmapV2->bitmapBpp = settings->ColorDepth;

	if ((settings->ColorDepth == 15) && (cacheBitmapV2->bitmapBpp == 16))
		cacheBitmapV2->bitmapBpp = settings->ColorDepth;

	if (!bitmap->Decompress(context, bitmap,
			cacheBitmapV2->bitmapDataStream, cacheBitmapV2->bitmapWidth, cacheBitmapV2->bitmapHeight,
			cacheBitmapV2->bitmapBpp, cacheBitmapV2->bitmapLength,
			cacheBitmapV2->compressed, RDP_CODEC_ID_NONE))
	{
		Bitmap_Free(context, bitmap);
		return FALSE;
	}

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmapV2->cacheId, cacheBitmapV2->cacheIndex);

	if (prevBitmap)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cacheBitmapV2->cacheId, cacheBitmapV2->cacheIndex, bitmap);
	return TRUE;
}

BOOL update_gdi_cache_bitmap_v3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	BOOL compressed = TRUE;
	rdpCache* cache = context->cache;
	rdpSettings* settings = context->settings;
	BITMAP_DATA_EX* bitmapData = &cacheBitmapV3->bitmapData;

	bitmap = Bitmap_Alloc(context);
	if (!bitmap)
		return FALSE;

	Bitmap_SetDimensions(context, bitmap, bitmapData->width, bitmapData->height);

	if (!cacheBitmapV3->bpp)
		cacheBitmapV3->bpp = settings->ColorDepth;

	compressed = (bitmapData->codecID != RDP_CODEC_ID_NONE);

	bitmap->Decompress(context, bitmap,
			bitmapData->data, bitmap->width, bitmap->height,
			bitmapData->bpp, bitmapData->length, compressed,
			bitmapData->codecID);

	bitmap->New(context, bitmap);

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmapV3->cacheId, cacheBitmapV3->cacheIndex);

	if (prevBitmap)
		Bitmap_Free(context, prevBitmap);

	bitmap_cache_put(cache->bitmap, cacheBitmapV3->cacheId, cacheBitmapV3->cacheIndex, bitmap);
	return TRUE;
}

BOOL update_gdi_bitmap_update(rdpContext* context, BITMAP_UPDATE* bitmapUpdate)
{
	int i;
	BOOL reused = TRUE;
	rdpBitmap* bitmap;
	BITMAP_DATA* bitmapData;
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
		bitmapData = &bitmapUpdate->rectangles[i];

		bitmap->bpp = bitmapData->bitsPerPixel;
		bitmap->length = bitmapData->bitmapLength;
		bitmap->compressed = bitmapData->compressed;

		Bitmap_SetRectangle(context, bitmap,
				bitmapData->destLeft, bitmapData->destTop,
				bitmapData->destRight, bitmapData->destBottom);

		Bitmap_SetDimensions(context, bitmap, bitmapData->width, bitmapData->height);

		bitmap->Decompress(context, bitmap,
				bitmapData->bitmapDataStream, bitmapData->width, bitmapData->height,
				bitmapData->bitsPerPixel, bitmapData->bitmapLength,
				bitmapData->compressed, RDP_CODEC_ID_NONE);

		if (reused)
			bitmap->Free(context, bitmap);
		else
			reused = TRUE;

		bitmap->New(context, bitmap);

		bitmap->Paint(context, bitmap);
	}
	return TRUE;
}

rdpBitmap* bitmap_cache_get(rdpBitmapCache* bitmapCache, UINT32 id, UINT32 index)
{
	rdpBitmap* bitmap;

	if (id > bitmapCache->maxCells)
	{
		WLog_ERR(TAG,  "get invalid bitmap cell id: %d", id);
		return NULL;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
	{
		index = bitmapCache->cells[id].number;
	}
	else if (index > bitmapCache->cells[id].number)
	{
		WLog_ERR(TAG,  "get invalid bitmap index %d in cell id: %d", index, id);
		return NULL;
	}

	bitmap = bitmapCache->cells[id].entries[index];

	return bitmap;
}

void bitmap_cache_put(rdpBitmapCache* bitmapCache, UINT32 id, UINT32 index, rdpBitmap* bitmap)
{
	if (id > bitmapCache->maxCells)
	{
		WLog_ERR(TAG,  "put invalid bitmap cell id: %d", id);
		return;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
	{
		index = bitmapCache->cells[id].number;
	}
	else if (index > bitmapCache->cells[id].number)
	{
		WLog_ERR(TAG,  "put invalid bitmap index %d in cell id: %d", index, id);
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

	bitmapCache = (rdpBitmapCache*) calloc(1, sizeof(rdpBitmapCache));

	if (bitmapCache)
	{
		bitmapCache->settings = settings;
		bitmapCache->update = ((freerdp*) settings->instance)->update;
		bitmapCache->context = bitmapCache->update->context;

		bitmapCache->maxCells = settings->BitmapCacheV2NumCells;

		bitmapCache->cells = (BITMAP_V2_CELL*) calloc(bitmapCache->maxCells, sizeof(BITMAP_V2_CELL));

		if (!bitmapCache->cells)
		{
			free(bitmapCache);
			return NULL;
		}

		for (i = 0; i < (int) bitmapCache->maxCells; i++)
		{
			bitmapCache->cells[i].number = settings->BitmapCacheV2CellInfo[i].numEntries;
			/* allocate an extra entry for BITMAP_CACHE_WAITING_LIST_INDEX */
			bitmapCache->cells[i].entries = (rdpBitmap**) calloc((bitmapCache->cells[i].number + 1), sizeof(rdpBitmap*));
		}
	}

	return bitmapCache;
}

void bitmap_cache_free(rdpBitmapCache* bitmapCache)
{
	int i, j;
	rdpBitmap* bitmap;

	if (bitmapCache)
	{
		for (i = 0; i < (int) bitmapCache->maxCells; i++)
		{
			for (j = 0; j < (int) bitmapCache->cells[i].number + 1; j++)
			{
				bitmap = bitmapCache->cells[i].entries[j];

				if (bitmap)
					Bitmap_Free(bitmapCache->context, bitmap);
			}

			free(bitmapCache->cells[i].entries);
		}

		if (bitmapCache->bitmap)
			Bitmap_Free(bitmapCache->context, bitmapCache->bitmap);

		free(bitmapCache->cells);
		free(bitmapCache);
	}
}
