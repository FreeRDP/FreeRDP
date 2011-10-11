/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Glyph Cache
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

#include <freerdp/cache/glyph.h>

void* glyph_get(rdpGlyphCache* glyph, uint8 id, uint16 index, void** extra)
{
	void* entry;

	if (id > 9)
	{
		printf("invalid glyph cache id: %d\n", id);
		return NULL;
	}

	if (index > glyph->glyphCache[id].number)
	{
		printf("invalid glyph cache index: %d in cache id: %d\n", index, id);
		return NULL;
	}

	entry = glyph->glyphCache[id].entries[index].entry;

	if (extra != NULL)
		*extra = glyph->glyphCache[id].entries[index].extra;

	return entry;
}

void glyph_put(rdpGlyphCache* glyph, uint8 id, uint16 index, void* entry, void* extra)
{
	if (id > 9)
	{
		printf("invalid glyph cache id: %d\n", id);
		return;
	}

	if (index > glyph->glyphCache[id].number)
	{
		printf("invalid glyph cache index: %d in cache id: %d\n", index, id);
		return;
	}

	glyph->glyphCache[id].entries[index].entry = entry;

	if (extra != NULL)
		glyph->glyphCache[id].entries[index].extra = extra;
}

void* glyph_fragment_get(rdpGlyphCache* glyph, uint8 index, uint8* count, void** extra)
{
	void* entry;

	entry = glyph->fragCache.entries[index].entry;
	*count = glyph->fragCache.entries[index].count;

	if (extra != NULL)
		*extra = glyph->fragCache.entries[index].extra;

	return entry;
}

void glyph_fragment_put(rdpGlyphCache* glyph, uint8 index, uint8 count, void* entry, void* extra)
{
	glyph->fragCache.entries[index].entry = entry;
	glyph->fragCache.entries[index].count = count;
	glyph->fragCache.entries[index].extra = extra;
}

rdpGlyphCache* glyph_cache_new(rdpSettings* settings)
{
	rdpGlyphCache* glyph;

	glyph = (rdpGlyphCache*) xzalloc(sizeof(rdpGlyphCache));

	if (glyph != NULL)
	{
		int i;

		glyph->settings = settings;

		//settings->glyphSupportLevel = GLYPH_SUPPORT_FULL;

		for (i = 0; i < 10; i++)
		{
			glyph->glyphCache[i].number = settings->glyphCache[i].cacheEntries;
			glyph->glyphCache[i].maxCellSize = settings->glyphCache[i].cacheMaximumCellSize;
			glyph->glyphCache[i].entries = xzalloc(sizeof(GLYPH_CACHE) * glyph->glyphCache[i].number);
		}

		glyph->fragCache.entries = xzalloc(sizeof(FRAGMENT_CACHE_ENTRY) * 256);
	}

	return glyph;
}

void glyph_cache_free(rdpGlyphCache* glyph)
{
	if (glyph != NULL)
	{
		int i;

		for (i = 0; i < 10; i++)
		{
			xfree(glyph->glyphCache[i].entries);
		}

		xfree(glyph->fragCache.entries);

		xfree(glyph);
	}
}
