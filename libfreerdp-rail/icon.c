/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Window Icon Cache
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
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/unicode.h>

#include <freerdp/rail/icon.h>

ICON_INFO* icon_cache_get(rdpIconCache* cache, uint8 id, uint16 index, void** extra)
{
	ICON_INFO* entry;

	if (id >= cache->numCaches)
	{
		printf("invalid window icon cache id:%d\n", id);
		return (ICON_INFO*) NULL;
	}

	if (index >= cache->numCacheEntries)
	{
		printf("invalid window icon cache index:%d in cache id:%d\n", index, id);
		return (ICON_INFO*) NULL;
	}

	entry = cache->caches[id].entries[index].entry;

	if (extra != NULL)
		*extra = cache->caches[id].entries[index].extra;

	return entry;
}

void icon_cache_put(rdpIconCache* cache, uint8 id, uint16 index, ICON_INFO* entry, void* extra)
{
	if (id >= cache->numCaches)
	{
		printf("invalid window icon cache id:%d\n", id);
		return;
	}

	if (index >= cache->numCacheEntries)
	{
		printf("invalid window icon cache index:%d in cache id:%d\n", index, id);
		return;
	}

	cache->caches[id].entries[index].entry = entry;

	if (extra != NULL)
		cache->caches[id].entries[index].extra = extra;
}

rdpIconCache* icon_cache_new(rdpRail* rail)
{
	rdpIconCache* cache;

	cache = (rdpIconCache*) xzalloc(sizeof(rdpIconCache));

	if (cache != NULL)
	{
		int i;

		cache->rail = rail;
		cache->numCaches = (uint8) rail->settings->num_icon_cache_entries;
		cache->numCacheEntries = rail->settings->num_icon_cache_entries;

		cache->caches = xzalloc(cache->numCaches * sizeof(WINDOW_ICON_CACHE));

		for (i = 0; i < cache->numCaches; i++)
		{
			cache->caches[i].entries = xzalloc(cache->numCacheEntries * sizeof(rdpIconCache));
		}
	}

	return cache;
}

void icon_cache_free(rdpIconCache* cache)
{
	if (cache != NULL)
	{
		int i;

		for (i = 0; i < cache->numCaches; i++)
		{
			xfree(cache->caches[i].entries);
		}

		xfree(cache->caches);

		xfree(cache);
	}
}

