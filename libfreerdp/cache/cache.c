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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/stream.h>

#include <freerdp/cache/cache.h>

rdpCache* cache_new(rdpSettings* settings)
{
	rdpCache* cache;
	cache = (rdpCache*) calloc(1, sizeof(rdpCache));

	if (!cache)
		return NULL;

	cache->glyph = glyph_cache_new(settings);

	if (!cache->glyph)
		goto error;

	cache->brush = brush_cache_new(settings);

	if (!cache->brush)
		goto error;

	cache->pointer = pointer_cache_new(settings);

	if (!cache->pointer)
		goto error;

	cache->bitmap = bitmap_cache_new(settings);

	if (!cache->bitmap)
		goto error;

	cache->offscreen = offscreen_cache_new(settings);

	if (!cache->offscreen)
		goto error;

	cache->palette = palette_cache_new(settings);

	if (!cache->palette)
		goto error;

	cache->nine_grid = nine_grid_cache_new(settings);

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
