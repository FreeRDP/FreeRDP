/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/cache/brush.h>

void update_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	uint8 style;
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

void update_gdi_cache_brush(rdpContext* context, CACHE_BRUSH_ORDER* cache_brush)
{
	rdpCache* cache = context->cache;
	brush_cache_put(cache->brush, cache_brush->index, cache_brush->data, cache_brush->bpp);
}

void* brush_cache_get(rdpBrushCache* brush, uint32 index, uint32* bpp)
{
	void* entry;

	if (*bpp == 1)
	{
		if (index > brush->maxMonoEntries)
		{
			printf("invalid brush (%d bpp) index: 0x%04X\n", *bpp, index);
			return NULL;
		}

		*bpp = brush->monoEntries[index].bpp;
		entry = brush->monoEntries[index].entry;
	}
	else
	{
		if (index > brush->maxEntries)
		{
			printf("invalid brush (%d bpp) index: 0x%04X\n", *bpp, index);
			return NULL;
		}

		*bpp = brush->entries[index].bpp;
		entry = brush->entries[index].entry;
	}

	if (entry == NULL)
	{
		printf("invalid brush (%d bpp) at index: 0x%04X\n", *bpp, index);
		return NULL;
	}

	return entry;
}

void brush_cache_put(rdpBrushCache* brush, uint32 index, void* entry, uint32 bpp)
{
	void* prevEntry;

	if (bpp == 1)
	{
		if (index > brush->maxMonoEntries)
		{
			printf("invalid brush (%d bpp) index: 0x%04X\n", bpp, index);
			return;
		}

		prevEntry = brush->monoEntries[index].entry;

		if (prevEntry != NULL)
			xfree(prevEntry);

		brush->monoEntries[index].bpp = bpp;
		brush->monoEntries[index].entry = entry;
	}
	else
	{
		if (index > brush->maxEntries)
		{
			printf("invalid brush (%d bpp) index: 0x%04X\n", bpp, index);
			return;
		}

		prevEntry = brush->entries[index].entry;

		if (prevEntry != NULL)
			xfree(prevEntry);

		brush->entries[index].bpp = bpp;
		brush->entries[index].entry = entry;
	}
}

void brush_cache_register_callbacks(rdpUpdate* update)
{
	rdpCache* cache = update->context->cache;

	cache->brush->PatBlt = update->primary->PatBlt;

	update->primary->PatBlt = update_gdi_patblt;
	update->secondary->CacheBrush = update_gdi_cache_brush;
}

rdpBrushCache* brush_cache_new(rdpSettings* settings)
{
	rdpBrushCache* brush;

	brush = (rdpBrushCache*) xzalloc(sizeof(rdpBrushCache));

	if (brush != NULL)
	{
		brush->settings = settings;

		brush->maxEntries = 64;
		brush->maxMonoEntries = 64;

		brush->entries = (BRUSH_ENTRY*) xzalloc(sizeof(BRUSH_ENTRY) * brush->maxEntries);
		brush->monoEntries = (BRUSH_ENTRY*) xzalloc(sizeof(BRUSH_ENTRY) * brush->maxMonoEntries);
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
					xfree(brush->entries[i].entry);
			}

			xfree(brush->entries);
		}

		if (brush->monoEntries != NULL)
		{
			for (i = 0; i < (int) brush->maxMonoEntries; i++)
			{
				if (brush->monoEntries[i].entry != NULL)
					xfree(brush->monoEntries[i].entry);
			}

			xfree(brush->monoEntries);
		}

		xfree(brush);
	}
}
