/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Offscreen Bitmap Cache
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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/cache/offscreen.h>

void* offscreen_cache_get(rdpOffscreenCache* offscreen_cache, uint16 index)
{
	void* bitmap;

	if (index > offscreen_cache->maxEntries)
	{
		printf("invalid offscreen bitmap index: 0x%04X\n", index);
		return NULL;
	}

	bitmap = offscreen_cache->entries[index].bitmap;

	if (bitmap == NULL)
	{
		printf("invalid offscreen bitmap at index: 0x%04X\n", index);
		return NULL;
	}

	return bitmap;
}

void offscreen_cache_put(rdpOffscreenCache* offscreen, uint16 index, void* bitmap)
{
	if (index > offscreen->maxEntries)
	{
		printf("invalid offscreen bitmap index: 0x%04X\n", index);
		return;
	}

	offscreen->entries[index].bitmap = bitmap;
}

rdpOffscreenCache* offscreen_cache_new(rdpSettings* settings)
{
	rdpOffscreenCache* offscreen_cache;

	offscreen_cache = (rdpOffscreenCache*) xzalloc(sizeof(rdpOffscreenCache));

	if (offscreen_cache != NULL)
	{
		offscreen_cache->settings = settings;

		offscreen_cache->maxSize = 7680;
		offscreen_cache->maxEntries = 100;

		settings->offscreen_bitmap_cache = True;
		settings->offscreen_bitmap_cache_size = offscreen_cache->maxSize;
		settings->offscreen_bitmap_cache_entries = offscreen_cache->maxEntries;

		offscreen_cache->entries = (OFFSCREEN_ENTRY*) xzalloc(sizeof(OFFSCREEN_ENTRY) * offscreen_cache->maxEntries);
	}

	return offscreen_cache;
}

void offscreen_cache_free(rdpOffscreenCache* offscreen_cache)
{
	if (offscreen_cache != NULL)
	{
		xfree(offscreen_cache->entries);
		xfree(offscreen_cache);
	}
}
