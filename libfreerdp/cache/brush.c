/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Brush Cache
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
#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <winpr/stream.h>

#include <freerdp/cache/brush.h>

#include "brush.h"

#define TAG FREERDP_TAG("cache.brush")

struct _BRUSH_ENTRY
{
	UINT32 bpp;
	void* entry;
};
typedef struct _BRUSH_ENTRY BRUSH_ENTRY;

struct rdp_brush_cache
{
	pPatBlt PatBlt;          /* 0 */
	pCacheBrush CacheBrush;  /* 1 */
	pPolygonSC PolygonSC;    /* 2 */
	pPolygonCB PolygonCB;    /* 3 */
	UINT32 paddingA[16 - 4]; /* 4 */

	UINT32 maxEntries;        /* 16 */
	UINT32 maxMonoEntries;    /* 17 */
	BRUSH_ENTRY* entries;     /* 18 */
	BRUSH_ENTRY* monoEntries; /* 19 */
	UINT32 paddingB[32 - 20]; /* 20 */

	/* internal */

	rdpSettings* settings;
};

static BOOL update_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	BYTE style;
	BOOL ret = TRUE;
	rdpBrush* brush = &patblt->brush;
	const rdpCache* cache = context->cache;
	style = brush->style;

	if (brush->style & CACHED_BRUSH)
	{
		brush->data = brush_cache_get(cache->brush, brush->index, &brush->bpp);
		brush->style = 0x03;
	}

	IFCALLRET(cache->brush->PatBlt, ret, context, patblt);
	brush->style = style;
	return ret;
}

static BOOL update_gdi_polygon_sc(rdpContext* context, const POLYGON_SC_ORDER* polygon_sc)
{
	rdpCache* cache = context->cache;
	return IFCALLRESULT(TRUE, cache->brush->PolygonSC, context, polygon_sc);
}

static BOOL update_gdi_polygon_cb(rdpContext* context, POLYGON_CB_ORDER* polygon_cb)
{
	BYTE style;
	rdpBrush* brush = &polygon_cb->brush;
	rdpCache* cache = context->cache;
	BOOL ret = TRUE;
	style = brush->style;

	if (brush->style & CACHED_BRUSH)
	{
		brush->data = brush_cache_get(cache->brush, brush->index, &brush->bpp);
		brush->style = 0x03;
	}

	IFCALLRET(cache->brush->PolygonCB, ret, context, polygon_cb);
	brush->style = style;
	return ret;
}

static BOOL update_gdi_cache_brush(rdpContext* context, const CACHE_BRUSH_ORDER* cacheBrush)
{
	UINT32 length;
	void* data = NULL;
	rdpCache* cache = context->cache;
	length = cacheBrush->bpp * 64 / 8;
	data = malloc(length);

	if (!data)
		return FALSE;

	CopyMemory(data, cacheBrush->data, length);
	brush_cache_put(cache->brush, cacheBrush->index, data, cacheBrush->bpp);
	return TRUE;
}

void* brush_cache_get(rdpBrushCache* brushCache, UINT32 index, UINT32* bpp)
{
	void* entry;

	if (!brushCache)
		return NULL;

	if (!bpp)
		return NULL;

	if (*bpp == 1)
	{
		if (index >= brushCache->maxMonoEntries)
		{
			WLog_ERR(TAG, "invalid brush (%" PRIu32 " bpp) index: 0x%08" PRIX32 "", *bpp, index);
			return NULL;
		}

		*bpp = brushCache->monoEntries[index].bpp;
		entry = brushCache->monoEntries[index].entry;
	}
	else
	{
		if (index >= brushCache->maxEntries)
		{
			WLog_ERR(TAG, "invalid brush (%" PRIu32 " bpp) index: 0x%08" PRIX32 "", *bpp, index);
			return NULL;
		}

		*bpp = brushCache->entries[index].bpp;
		entry = brushCache->entries[index].entry;
	}

	if (entry == NULL)
	{
		WLog_ERR(TAG, "invalid brush (%" PRIu32 " bpp) at index: 0x%08" PRIX32 "", *bpp, index);
		return NULL;
	}

	return entry;
}

void brush_cache_put(rdpBrushCache* brushCache, UINT32 index, void* entry, UINT32 bpp)
{
	if (bpp == 1)
	{
		if (index >= brushCache->maxMonoEntries)
		{
			WLog_ERR(TAG, "invalid brush (%" PRIu32 " bpp) index: 0x%08" PRIX32 "", bpp, index);
			free(entry);
			return;
		}

		free(brushCache->monoEntries[index].entry);
		brushCache->monoEntries[index].bpp = bpp;
		brushCache->monoEntries[index].entry = entry;
	}
	else
	{
		if (index >= brushCache->maxEntries)
		{
			WLog_ERR(TAG, "invalid brush (%" PRIu32 " bpp) index: 0x%08" PRIX32 "", bpp, index);
			free(entry);
			return;
		}

		free(brushCache->entries[index].entry);
		brushCache->entries[index].bpp = bpp;
		brushCache->entries[index].entry = entry;
	}
}

void brush_cache_register_callbacks(rdpUpdate* update)
{
	rdpCache* cache = update->context->cache;
	cache->brush->PatBlt = update->primary->PatBlt;
	cache->brush->PolygonSC = update->primary->PolygonSC;
	cache->brush->PolygonCB = update->primary->PolygonCB;
	update->primary->PatBlt = update_gdi_patblt;
	update->primary->PolygonSC = update_gdi_polygon_sc;
	update->primary->PolygonCB = update_gdi_polygon_cb;
	update->secondary->CacheBrush = update_gdi_cache_brush;
}

rdpBrushCache* brush_cache_new(rdpSettings* settings)
{
	rdpBrushCache* brushCache;
	brushCache = (rdpBrushCache*)calloc(1, sizeof(rdpBrushCache));

	if (!brushCache)
		return NULL;

	brushCache->settings = settings;
	brushCache->maxEntries = 64;
	brushCache->maxMonoEntries = 64;
	brushCache->entries = (BRUSH_ENTRY*)calloc(brushCache->maxEntries, sizeof(BRUSH_ENTRY));

	if (!brushCache->entries)
		goto error_entries;

	brushCache->monoEntries = (BRUSH_ENTRY*)calloc(brushCache->maxMonoEntries, sizeof(BRUSH_ENTRY));

	if (!brushCache->monoEntries)
		goto error_mono;

	return brushCache;
error_mono:
	free(brushCache->entries);
error_entries:
	free(brushCache);
	return NULL;
}

void brush_cache_free(rdpBrushCache* brushCache)
{
	int i;

	if (brushCache)
	{
		if (brushCache->entries)
		{
			for (i = 0; i < (int)brushCache->maxEntries; i++)
				free(brushCache->entries[i].entry);

			free(brushCache->entries);
		}

		if (brushCache->monoEntries)
		{
			for (i = 0; i < (int)brushCache->maxMonoEntries; i++)
				free(brushCache->monoEntries[i].entry);

			free(brushCache->monoEntries);
		}

		free(brushCache);
	}
}

void free_cache_brush_order(rdpContext* context, CACHE_BRUSH_ORDER* order)
{
	free(order);
}

CACHE_BRUSH_ORDER* copy_cache_brush_order(rdpContext* context, const CACHE_BRUSH_ORDER* order)
{
	CACHE_BRUSH_ORDER* dst = calloc(1, sizeof(CACHE_BRUSH_ORDER));

	if (!dst || !order)
		goto fail;

	*dst = *order;
	return dst;
fail:
	free_cache_brush_order(context, dst);
	return NULL;
}
