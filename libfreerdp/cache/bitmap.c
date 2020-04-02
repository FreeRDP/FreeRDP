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
#include <freerdp/gdi/bitmap.h>

#include "../gdi/gdi.h"
#include "../core/graphics.h"

#include "bitmap.h"

#define TAG FREERDP_TAG("cache.bitmap")

static rdpBitmap* bitmap_cache_get(rdpBitmapCache* bitmapCache, UINT32 id, UINT32 index);
static BOOL bitmap_cache_put(rdpBitmapCache* bitmap_cache, UINT32 id, UINT32 index,
                             rdpBitmap* bitmap);

static BOOL update_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;

	if (memblt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, memblt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, (BYTE)memblt->cacheId, memblt->cacheIndex);

	/* XP-SP2 servers sometimes ask for cached bitmaps they've never defined. */
	if (bitmap == NULL)
		return TRUE;

	memblt->bitmap = bitmap;
	return IFCALLRESULT(TRUE, cache->bitmap->MemBlt, context, memblt);
}

static BOOL update_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	BYTE style;
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;
	rdpBrush* brush = &mem3blt->brush;
	BOOL ret = TRUE;

	if (mem3blt->cacheId == 0xFF)
		bitmap = offscreen_cache_get(cache->offscreen, mem3blt->cacheIndex);
	else
		bitmap = bitmap_cache_get(cache->bitmap, (BYTE)mem3blt->cacheId, mem3blt->cacheIndex);

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

static BOOL update_gdi_cache_bitmap(rdpContext* context, const CACHE_BITMAP_ORDER* cacheBitmap)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;
	bitmap = Bitmap_Alloc(context);

	if (!bitmap)
		return FALSE;

	Bitmap_SetDimensions(bitmap, cacheBitmap->bitmapWidth, cacheBitmap->bitmapHeight);

	if (!bitmap->Decompress(context, bitmap, cacheBitmap->bitmapDataStream,
	                        cacheBitmap->bitmapWidth, cacheBitmap->bitmapHeight,
	                        cacheBitmap->bitmapBpp, cacheBitmap->bitmapLength,
	                        cacheBitmap->compressed, RDP_CODEC_ID_NONE))
	{
		Bitmap_Free(context, bitmap);
		return FALSE;
	}

	if (!bitmap->New(context, bitmap))
	{
		Bitmap_Free(context, bitmap);
		return FALSE;
	}

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmap->cacheId, cacheBitmap->cacheIndex);
	Bitmap_Free(context, prevBitmap);
	return bitmap_cache_put(cache->bitmap, cacheBitmap->cacheId, cacheBitmap->cacheIndex, bitmap);
}

static BOOL update_gdi_cache_bitmap_v2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cacheBitmapV2)

{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = context->cache;
	rdpSettings* settings = context->settings;
	bitmap = Bitmap_Alloc(context);

	if (!bitmap)
		return FALSE;

	if (!cacheBitmapV2->bitmapBpp)
		cacheBitmapV2->bitmapBpp = settings->ColorDepth;

	if ((settings->ColorDepth == 15) && (cacheBitmapV2->bitmapBpp == 16))
		cacheBitmapV2->bitmapBpp = settings->ColorDepth;

	Bitmap_SetDimensions(bitmap, cacheBitmapV2->bitmapWidth, cacheBitmapV2->bitmapHeight);

	if (!bitmap->Decompress(context, bitmap, cacheBitmapV2->bitmapDataStream,
	                        cacheBitmapV2->bitmapWidth, cacheBitmapV2->bitmapHeight,
	                        cacheBitmapV2->bitmapBpp, cacheBitmapV2->bitmapLength,
	                        cacheBitmapV2->compressed, RDP_CODEC_ID_NONE))
	{
		Bitmap_Free(context, bitmap);
		return FALSE;
	}

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmapV2->cacheId, cacheBitmapV2->cacheIndex);

	if (!bitmap->New(context, bitmap))
	{
		Bitmap_Free(context, bitmap);
		return FALSE;
	}

	Bitmap_Free(context, prevBitmap);
	return bitmap_cache_put(cache->bitmap, cacheBitmapV2->cacheId, cacheBitmapV2->cacheIndex,
	                        bitmap);
}

static BOOL update_gdi_cache_bitmap_v3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cacheBitmapV3)
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

	if (!cacheBitmapV3->bpp)
		cacheBitmapV3->bpp = settings->ColorDepth;

	compressed = (bitmapData->codecID != RDP_CODEC_ID_NONE);
	Bitmap_SetDimensions(bitmap, bitmapData->width, bitmapData->height);

	if (!bitmap->Decompress(context, bitmap, bitmapData->data, bitmapData->width,
	                        bitmapData->height, bitmapData->bpp, bitmapData->length, compressed,
	                        bitmapData->codecID))
	{
		Bitmap_Free(context, bitmap);
		return FALSE;
	}

	if (!bitmap->New(context, bitmap))
	{
		Bitmap_Free(context, bitmap);
		return FALSE;
	}

	prevBitmap = bitmap_cache_get(cache->bitmap, cacheBitmapV3->cacheId, cacheBitmapV3->cacheIndex);
	Bitmap_Free(context, prevBitmap);
	return bitmap_cache_put(cache->bitmap, cacheBitmapV3->cacheId, cacheBitmapV3->cacheIndex,
	                        bitmap);
}

rdpBitmap* bitmap_cache_get(rdpBitmapCache* bitmapCache, UINT32 id, UINT32 index)
{
	rdpBitmap* bitmap;

	if (id > bitmapCache->maxCells)
	{
		WLog_ERR(TAG, "get invalid bitmap cell id: %" PRIu32 "", id);
		return NULL;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
	{
		index = bitmapCache->cells[id].number;
	}
	else if (index > bitmapCache->cells[id].number)
	{
		WLog_ERR(TAG, "get invalid bitmap index %" PRIu32 " in cell id: %" PRIu32 "", index, id);
		return NULL;
	}

	bitmap = bitmapCache->cells[id].entries[index];
	return bitmap;
}

BOOL bitmap_cache_put(rdpBitmapCache* bitmapCache, UINT32 id, UINT32 index, rdpBitmap* bitmap)
{
	if (id > bitmapCache->maxCells)
	{
		WLog_ERR(TAG, "put invalid bitmap cell id: %" PRIu32 "", id);
		return FALSE;
	}

	if (index == BITMAP_CACHE_WAITING_LIST_INDEX)
	{
		index = bitmapCache->cells[id].number;
	}
	else if (index > bitmapCache->cells[id].number)
	{
		WLog_ERR(TAG, "put invalid bitmap index %" PRIu32 " in cell id: %" PRIu32 "", index, id);
		return FALSE;
	}

	bitmapCache->cells[id].entries[index] = bitmap;
	return TRUE;
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
	update->BitmapUpdate = gdi_bitmap_update;
}

rdpBitmapCache* bitmap_cache_new(rdpSettings* settings)
{
	int i;
	rdpBitmapCache* bitmapCache;
	bitmapCache = (rdpBitmapCache*)calloc(1, sizeof(rdpBitmapCache));

	if (!bitmapCache)
		return NULL;

	bitmapCache->settings = settings;
	bitmapCache->update = ((freerdp*)settings->instance)->update;
	bitmapCache->context = bitmapCache->update->context;
	bitmapCache->cells =
	    (BITMAP_V2_CELL*)calloc(settings->BitmapCacheV2NumCells, sizeof(BITMAP_V2_CELL));

	if (!bitmapCache->cells)
		goto fail;
	bitmapCache->maxCells = settings->BitmapCacheV2NumCells;

	for (i = 0; i < (int)bitmapCache->maxCells; i++)
	{
		bitmapCache->cells[i].number = settings->BitmapCacheV2CellInfo[i].numEntries;
		/* allocate an extra entry for BITMAP_CACHE_WAITING_LIST_INDEX */
		bitmapCache->cells[i].entries =
		    (rdpBitmap**)calloc((bitmapCache->cells[i].number + 1), sizeof(rdpBitmap*));

		if (!bitmapCache->cells[i].entries)
			goto fail;
	}

	return bitmapCache;
fail:

	if (bitmapCache->cells)
	{
		for (i = 0; i < (int)bitmapCache->maxCells; i++)
			free(bitmapCache->cells[i].entries);
	}

	free(bitmapCache);
	return NULL;
}

void bitmap_cache_free(rdpBitmapCache* bitmapCache)
{
	int i, j;
	rdpBitmap* bitmap;

	if (bitmapCache)
	{
		for (i = 0; i < (int)bitmapCache->maxCells; i++)
		{
			for (j = 0; j < (int)bitmapCache->cells[i].number + 1; j++)
			{
				bitmap = bitmapCache->cells[i].entries[j];
				Bitmap_Free(bitmapCache->context, bitmap);
			}

			free(bitmapCache->cells[i].entries);
		}

		free(bitmapCache->cells);
		free(bitmapCache);
	}
}

static void free_bitmap_data(BITMAP_DATA* data, size_t count)
{
	size_t x;

	if (!data)
		return;

	for (x = 0; x < count; x++)
		free(data[x].bitmapDataStream);

	free(data);
}

static BITMAP_DATA* copy_bitmap_data(const BITMAP_DATA* data, size_t count)
{
	size_t x;
	BITMAP_DATA* dst = (BITMAP_DATA*)calloc(count, sizeof(BITMAP_DATA));

	if (!dst)
		goto fail;

	for (x = 0; x < count; x++)
	{
		dst[x] = data[x];

		if (data[x].bitmapLength > 0)
		{
			dst[x].bitmapDataStream = malloc(data[x].bitmapLength);

			if (!dst[x].bitmapDataStream)
				goto fail;

			memcpy(dst[x].bitmapDataStream, data[x].bitmapDataStream, data[x].bitmapLength);
		}
	}

	return dst;
fail:
	free_bitmap_data(dst, count);
	return NULL;
}

void free_bitmap_update(rdpContext* context, BITMAP_UPDATE* pointer)
{
	if (!pointer)
		return;

	free_bitmap_data(pointer->rectangles, pointer->number);
	free(pointer);
}

BITMAP_UPDATE* copy_bitmap_update(rdpContext* context, const BITMAP_UPDATE* pointer)
{
	BITMAP_UPDATE* dst = calloc(1, sizeof(BITMAP_UPDATE));

	if (!dst || !pointer)
		goto fail;

	*dst = *pointer;
	dst->rectangles = copy_bitmap_data(pointer->rectangles, pointer->number);

	if (!dst->rectangles)
		goto fail;

	return dst;
fail:
	free_bitmap_update(context, dst);
	return NULL;
}

CACHE_BITMAP_ORDER* copy_cache_bitmap_order(rdpContext* context, const CACHE_BITMAP_ORDER* order)
{
	CACHE_BITMAP_ORDER* dst = calloc(1, sizeof(CACHE_BITMAP_ORDER));

	if (!dst || !order)
		goto fail;

	*dst = *order;

	if (order->bitmapLength > 0)
	{
		dst->bitmapDataStream = malloc(order->bitmapLength);

		if (!dst->bitmapDataStream)
			goto fail;

		memcpy(dst->bitmapDataStream, order->bitmapDataStream, order->bitmapLength);
	}

	return dst;
fail:
	free_cache_bitmap_order(context, dst);
	return NULL;
}

void free_cache_bitmap_order(rdpContext* context, CACHE_BITMAP_ORDER* order)
{
	if (order)
		free(order->bitmapDataStream);

	free(order);
}

CACHE_BITMAP_V2_ORDER* copy_cache_bitmap_v2_order(rdpContext* context,
                                                  const CACHE_BITMAP_V2_ORDER* order)
{
	CACHE_BITMAP_V2_ORDER* dst = calloc(1, sizeof(CACHE_BITMAP_V2_ORDER));

	if (!dst || !order)
		goto fail;

	*dst = *order;

	if (order->bitmapLength > 0)
	{
		dst->bitmapDataStream = malloc(order->bitmapLength);

		if (!dst->bitmapDataStream)
			goto fail;

		memcpy(dst->bitmapDataStream, order->bitmapDataStream, order->bitmapLength);
	}

	return dst;
fail:
	free_cache_bitmap_v2_order(context, dst);
	return NULL;
}

void free_cache_bitmap_v2_order(rdpContext* context, CACHE_BITMAP_V2_ORDER* order)
{
	if (order)
		free(order->bitmapDataStream);

	free(order);
}

CACHE_BITMAP_V3_ORDER* copy_cache_bitmap_v3_order(rdpContext* context,
                                                  const CACHE_BITMAP_V3_ORDER* order)
{
	CACHE_BITMAP_V3_ORDER* dst = calloc(1, sizeof(CACHE_BITMAP_V3_ORDER));

	if (!dst || !order)
		goto fail;

	*dst = *order;

	if (order->bitmapData.length > 0)
	{
		dst->bitmapData.data = malloc(order->bitmapData.length);

		if (!dst->bitmapData.data)
			goto fail;

		memcpy(dst->bitmapData.data, order->bitmapData.data, order->bitmapData.length);
	}

	return dst;
fail:
	free_cache_bitmap_v3_order(context, dst);
	return NULL;
}

void free_cache_bitmap_v3_order(rdpContext* context, CACHE_BITMAP_V3_ORDER* order)
{
	if (order)
		free(order->bitmapData.data);

	free(order);
}
