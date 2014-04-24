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

#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <winpr/stream.h>

#include <freerdp/cache/brush.h>

void update_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	BYTE style;
	rdpBrush* brush = &patblt->brush;
	rdpCache* cache = context->cache;

	style = brush->style;

	if (brush->style & CACHED_BRUSH)
	{
		brush->data = brush_cache_get(cache->brush, brush->index, &brush->bpp);
		brush->style = 0x03;
	}

	IFCALL(cache->brush->PatBlt, context, patblt);
	brush->style = style;
}

void update_gdi_polygon_sc(rdpContext* context, POLYGON_SC_ORDER* polygon_sc)
{
	rdpCache* cache = context->cache;
	IFCALL(cache->brush->PolygonSC, context, polygon_sc);
}

void update_gdi_polygon_cb(rdpContext* context, POLYGON_CB_ORDER* polygon_cb)
{
	BYTE style;
	rdpBrush* brush = &polygon_cb->brush;
	rdpCache* cache = context->cache;

	style = brush->style;

	if (brush->style & CACHED_BRUSH)
	{
		brush->data = brush_cache_get(cache->brush, brush->index, &brush->bpp);
		brush->style = 0x03;
	}

	IFCALL(cache->brush->PolygonCB, context, polygon_cb);
	brush->style = style;
}

static void update_gdi_cache_brush(rdpContext* context, CACHE_BRUSH_ORDER* cacheBrush)
{
	int length;
	void* data = NULL;
	rdpCache* cache = context->cache;

	length = cacheBrush->bpp * 64 / 8;

	data = malloc(length);
	CopyMemory(data, cacheBrush->data, length);

	brush_cache_put(cache->brush, cacheBrush->index, data, cacheBrush->bpp);
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
			fprintf(stderr, "invalid brush (%d bpp) index: 0x%04X\n", *bpp, index);
			return NULL;
		}

		*bpp = brushCache->monoEntries[index].bpp;
		entry = brushCache->monoEntries[index].entry;
	}
	else
	{
		if (index >= brushCache->maxEntries)
		{
			fprintf(stderr, "invalid brush (%d bpp) index: 0x%04X\n", *bpp, index);
			return NULL;
		}

		*bpp = brushCache->entries[index].bpp;
		entry = brushCache->entries[index].entry;
	}

	if (entry == NULL)
	{
		fprintf(stderr, "invalid brush (%d bpp) at index: 0x%04X\n", *bpp, index);
		return NULL;
	}

	return entry;
}

void brush_cache_put(rdpBrushCache* brushCache, UINT32 index, void* entry, UINT32 bpp)
{
	void* prevEntry;

	if (bpp == 1)
	{
		if (index >= brushCache->maxMonoEntries)
		{
			fprintf(stderr, "invalid brush (%d bpp) index: 0x%04X\n", bpp, index);

			if (entry)
				free(entry);

			return;
		}

		prevEntry = brushCache->monoEntries[index].entry;

		if (prevEntry != NULL)
			free(prevEntry);

		brushCache->monoEntries[index].bpp = bpp;
		brushCache->monoEntries[index].entry = entry;
	}
	else
	{
		if (index >= brushCache->maxEntries)
		{
			fprintf(stderr, "invalid brush (%d bpp) index: 0x%04X\n", bpp, index);

			if (entry)
				free(entry);

			return;
		}

		prevEntry = brushCache->entries[index].entry;

		if (prevEntry != NULL)
			free(prevEntry);

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

	brushCache = (rdpBrushCache*) malloc(sizeof(rdpBrushCache));

	if (brushCache)
	{
		ZeroMemory(brushCache, sizeof(rdpBrushCache));

		brushCache->settings = settings;

		brushCache->maxEntries = 64;
		brushCache->maxMonoEntries = 64;

		brushCache->entries = (BRUSH_ENTRY*) malloc(sizeof(BRUSH_ENTRY) * brushCache->maxEntries);
		ZeroMemory(brushCache->entries, sizeof(BRUSH_ENTRY) * brushCache->maxEntries);

		brushCache->monoEntries = (BRUSH_ENTRY*) malloc(sizeof(BRUSH_ENTRY) * brushCache->maxMonoEntries);
		ZeroMemory(brushCache->monoEntries, sizeof(BRUSH_ENTRY) * brushCache->maxMonoEntries);
	}

	return brushCache;
}

void brush_cache_free(rdpBrushCache* brushCache)
{
	int i;

	if (brushCache)
	{
		if (brushCache->entries)
		{
			for (i = 0; i < (int) brushCache->maxEntries; i++)
			{
				if (brushCache->entries[i].entry != NULL)
					free(brushCache->entries[i].entry);
			}

			free(brushCache->entries);
		}

		if (brushCache->monoEntries)
		{
			for (i = 0; i < (int) brushCache->maxMonoEntries; i++)
			{
				if (brushCache->monoEntries[i].entry != NULL)
					free(brushCache->monoEntries[i].entry);
			}

			free(brushCache->monoEntries);
		}

		free(brushCache);
	}
}
