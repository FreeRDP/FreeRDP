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

void update_gdi_cache_brush(rdpContext* context, CACHE_BRUSH_ORDER* cache_brush)
{
	int length;
	void* data = NULL;
	rdpCache* cache = context->cache;

	length = cache_brush->bpp * 64 / 8;

	data = malloc(length);
	CopyMemory(data, cache_brush->data, length);

	brush_cache_put(cache->brush, cache_brush->index, data, cache_brush->bpp);
}

void* brush_cache_get(rdpBrushCache* brush, UINT32 index, UINT32* bpp)
{
	void* entry;

	if (*bpp == 1)
	{
		if (index >= brush->maxMonoEntries)
		{
			fprintf(stderr, "invalid brush (%d bpp) index: 0x%04X\n", *bpp, index);
			return NULL;
		}

		*bpp = brush->monoEntries[index].bpp;
		entry = brush->monoEntries[index].entry;
	}
	else
	{
		if (index >= brush->maxEntries)
		{
			fprintf(stderr, "invalid brush (%d bpp) index: 0x%04X\n", *bpp, index);
			return NULL;
		}

		*bpp = brush->entries[index].bpp;
		entry = brush->entries[index].entry;
	}

	if (entry == NULL)
	{
		fprintf(stderr, "invalid brush (%d bpp) at index: 0x%04X\n", *bpp, index);
		return NULL;
	}

	return entry;
}

void brush_cache_put(rdpBrushCache* brush, UINT32 index, void* entry, UINT32 bpp)
{
	void* prevEntry;

	if (bpp == 1)
	{
		if (index >= brush->maxMonoEntries)
		{
			fprintf(stderr, "invalid brush (%d bpp) index: 0x%04X\n", bpp, index);
			return;
		}

		prevEntry = brush->monoEntries[index].entry;

		if (prevEntry != NULL)
			free(prevEntry);

		brush->monoEntries[index].bpp = bpp;
		brush->monoEntries[index].entry = entry;
	}
	else
	{
		if (index >= brush->maxEntries)
		{
			fprintf(stderr, "invalid brush (%d bpp) index: 0x%04X\n", bpp, index);
			return;
		}

		prevEntry = brush->entries[index].entry;

		if (prevEntry != NULL)
			free(prevEntry);

		brush->entries[index].bpp = bpp;
		brush->entries[index].entry = entry;
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
	rdpBrushCache* brush;

	brush = (rdpBrushCache*) malloc(sizeof(rdpBrushCache));
	ZeroMemory(brush, sizeof(rdpBrushCache));

	if (brush != NULL)
	{
		brush->settings = settings;

		brush->maxEntries = 64;
		brush->maxMonoEntries = 64;

		brush->entries = (BRUSH_ENTRY*) malloc(sizeof(BRUSH_ENTRY) * brush->maxEntries);
		ZeroMemory(brush->entries, sizeof(BRUSH_ENTRY) * brush->maxEntries);

		brush->monoEntries = (BRUSH_ENTRY*) malloc(sizeof(BRUSH_ENTRY) * brush->maxMonoEntries);
		ZeroMemory(brush->monoEntries, sizeof(BRUSH_ENTRY) * brush->maxMonoEntries);
	}

	return brush;
}

void brush_cache_free(rdpBrushCache* brush)
{
	int i;

	if (brush != NULL)
	{
		if (brush->entries != NULL)
		{
			for (i = 0; i < (int) brush->maxEntries; i++)
			{
				if (brush->entries[i].entry != NULL)
					free(brush->entries[i].entry);
			}

			free(brush->entries);
		}

		if (brush->monoEntries != NULL)
		{
			for (i = 0; i < (int) brush->maxMonoEntries; i++)
			{
				if (brush->monoEntries[i].entry != NULL)
					free(brush->monoEntries[i].entry);
			}

			free(brush->monoEntries);
		}

		free(brush);
	}
}
