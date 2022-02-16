/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Caches
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

#include <freerdp/config.h>

#include <winpr/crt.h>

#include <winpr/stream.h>

#include <freerdp/cache/cache.h>

#include "cache.h"

rdpCache* cache_new(rdpContext* context)
{
	rdpCache* cache;

	WINPR_ASSERT(context);

	cache = (rdpCache*)calloc(1, sizeof(rdpCache));

	if (!cache)
		return NULL;

	cache->glyph = glyph_cache_new(context);

	if (!cache->glyph)
		goto error;

	cache->brush = brush_cache_new(context);

	if (!cache->brush)
		goto error;

	cache->pointer = pointer_cache_new(context);

	if (!cache->pointer)
		goto error;

	cache->bitmap = bitmap_cache_new(context);

	if (!cache->bitmap)
		goto error;

	cache->offscreen = offscreen_cache_new(context);

	if (!cache->offscreen)
		goto error;

	cache->palette = palette_cache_new(context);

	if (!cache->palette)
		goto error;

	cache->nine_grid = nine_grid_cache_new(context);

	if (!cache->nine_grid)
		goto error;

	return cache;
error:
	cache_free(cache);
	return NULL;
}

void cache_free(rdpCache* cache)
{
	if (cache != NULL)
	{
		glyph_cache_free(cache->glyph);
		brush_cache_free(cache->brush);
		pointer_cache_free(cache->pointer);
		bitmap_cache_free(cache->bitmap);
		offscreen_cache_free(cache->offscreen);
		palette_cache_free(cache->palette);
		nine_grid_cache_free(cache->nine_grid);
		free(cache);
	}
}

CACHE_COLOR_TABLE_ORDER* copy_cache_color_table_order(rdpContext* context,
                                                      const CACHE_COLOR_TABLE_ORDER* order)
{
	CACHE_COLOR_TABLE_ORDER* dst = calloc(1, sizeof(CACHE_COLOR_TABLE_ORDER));

	if (!dst || !order)
		goto fail;

	*dst = *order;
	return dst;
fail:
	free_cache_color_table_order(context, dst);
	return NULL;
}

void free_cache_color_table_order(rdpContext* context, CACHE_COLOR_TABLE_ORDER* order)
{
	free(order);
}

SURFACE_BITS_COMMAND* copy_surface_bits_command(rdpContext* context,
                                                const SURFACE_BITS_COMMAND* order)
{
	SURFACE_BITS_COMMAND* dst = calloc(1, sizeof(SURFACE_BITS_COMMAND));
	if (!dst || !order)
		goto fail;

	*dst = *order;

	dst->bmp.bitmapData = (BYTE*)malloc(order->bmp.bitmapDataLength);

	if (!dst->bmp.bitmapData)
		goto fail;

	CopyMemory(dst->bmp.bitmapData, order->bmp.bitmapData, order->bmp.bitmapDataLength);

	return dst;

fail:
	free_surface_bits_command(context, dst);
	return NULL;
}

void free_surface_bits_command(rdpContext* context, SURFACE_BITS_COMMAND* order)
{
	if (order)
		free(order->bmp.bitmapData);
	free(order);
}
