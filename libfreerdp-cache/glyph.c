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

#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/cache/glyph.h>

void update_process_glyph(rdpUpdate* update, uint8* data, int* index,
		int* x, int* y, uint8 cacheId, boolean delta, boolean vertical)
{
	int offset;
	rdpGlyph* glyph;
	rdpGlyphCache* glyph_cache;

	glyph_cache = update->context->cache->glyph;

	glyph = glyph_cache_get(glyph_cache, cacheId, data[(*index)++]);

	if (glyph != NULL)
	{
		IFCALL(glyph_cache->DrawGlyph, update->context, glyph, *x, *y);
	}

	offset = data[(*index)++];

	if (offset & 0x80)
	{
		offset = data[*index + 1] | (data[*index + 2] << 8);
		*index += 2;
	}

	if (delta)
	{
		if (vertical)
			*y += offset;
		else
			*x += offset;
	}
	else
	{
		*x += glyph->cx;
	}
}

void update_process_glyph_fragments(rdpUpdate* update, uint8* data, uint8 length,
		uint8 cacheId, boolean delta, boolean vertical,
		uint32 bgcolor, uint32 fgcolor, int x, int y,
		int bkX, int bkY, int bkWidth, int bkHeight, int opX, int opY, int opWidth, int opHeight)
{
	int n;
	uint8 id;
	uint8 size;
	int index = 0;
	uint8* fragments;
	rdpGlyphCache* glyph_cache;

	glyph_cache = update->context->cache->glyph;

	if (opWidth > 1)
		IFCALL(glyph_cache->BeginDrawText, update->context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);

	while (index < length)
	{
		switch (data[index])
		{
			case GLYPH_FRAGMENT_USE:

				if (index + 2 > length)
				{
					/* at least two bytes need to follow */
					index = length = 0;
					break;
				}

				id = data[index + 1];
				fragments = (uint8*) glyph_cache_fragment_get(glyph_cache, id, &size);

				n = 0;

				do
				{
					update_process_glyph(update, fragments, &n, &x, &y, cacheId, delta, vertical);
				}
				while (n < size);

				index += (index + 2 < length) ? 3 : 2;
				length -= index;
				data = &(data[index]);
				index = 0;

				break;

			case GLYPH_FRAGMENT_ADD:

				if (index + 3 > length)
				{
					/* at least three bytes need to follow */
					index = length = 0;
					break;
				}

				id = data[index + 1];
				size = data[index + 2];
				glyph_cache_fragment_put(glyph_cache, id, size, data);

				index += 3;
				length -= index;
				data = &(data[index]);
				index = 0;

				break;

			default:
				update_process_glyph(update, data, &index, &x, &y, cacheId, delta, vertical);
				break;
		}
	}

	if (opWidth > 1)
		IFCALL(glyph_cache->EndDrawText, update->context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);
	else
		IFCALL(glyph_cache->EndDrawText, update->context, bkX, bkY, bkWidth, bkHeight, bgcolor, fgcolor);
}

void update_gdi_glyph_index(rdpUpdate* update, GLYPH_INDEX_ORDER* glyph_index)
{

}

void update_gdi_fast_index(rdpUpdate* update, FAST_INDEX_ORDER* fast_index)
{
	boolean delta = False;
	boolean vertical = False;
	rdpGlyphCache* glyph_cache;

	glyph_cache = update->context->cache->glyph;

	if ((fast_index->ulCharInc == 0) && !(fast_index->flAccel & SO_CHAR_INC_EQUAL_BM_BASE))
		delta = True;

	if (fast_index->flAccel & SO_VERTICAL)
		vertical = True;

	update_process_glyph_fragments(update, fast_index->data, fast_index->cbData,
			fast_index->cacheId, delta, vertical,
			fast_index->backColor, fast_index->foreColor, fast_index->x, fast_index->y,
			fast_index->bkLeft, fast_index->bkTop,
			fast_index->bkRight - fast_index->bkLeft, fast_index->bkBottom - fast_index->bkTop,
			fast_index->opLeft, fast_index->opTop,
			fast_index->opRight - fast_index->opLeft, fast_index->opBottom - fast_index->opTop);
}

void update_gdi_cache_glyph(rdpUpdate* update, CACHE_GLYPH_ORDER* cache_glyph)
{
	int i;
	rdpGlyph* glyph;
	GLYPH_DATA* glyph_data;
	rdpCache* cache = update->context->cache;

	for (i = 0; i < cache_glyph->cGlyphs; i++)
	{
		glyph_data = cache_glyph->glyphData[i];

		glyph = (rdpGlyph*) xzalloc(cache->glyph->glyph_size);
		glyph->x = glyph_data->x;
		glyph->y = glyph_data->y;
		glyph->cx = glyph_data->cx;
		glyph->cy = glyph_data->cy;
		glyph->aj = glyph_data->aj;
		glyph->cb = glyph_data->cb;
		IFCALL(cache->glyph->GlyphNew, update->context, glyph);

		glyph_cache_put(cache->glyph, cache_glyph->cacheId, glyph_data->cacheIndex, glyph);
	}
}

void update_gdi_cache_glyph_v2(rdpUpdate* update, CACHE_GLYPH_V2_ORDER* cache_glyph_v2)
{

}

rdpGlyph* glyph_cache_get(rdpGlyphCache* glyph_cache, uint8 id, uint16 index)
{
	rdpGlyph* entry;

	if (id > 9)
	{
		printf("invalid glyph cache id: %d\n", id);
		return NULL;
	}

	if (index > glyph_cache->glyphCache[id].number)
	{
		printf("invalid glyph cache index: %d in cache id: %d\n", index, id);
		return NULL;
	}

	entry = glyph_cache->glyphCache[id].entries[index];

	return entry;
}

void glyph_cache_put(rdpGlyphCache* glyph_cache, uint8 id, uint16 index, rdpGlyph* entry)
{
	if (id > 9)
	{
		printf("invalid glyph cache id: %d\n", id);
		return;
	}

	if (index > glyph_cache->glyphCache[id].number)
	{
		printf("invalid glyph cache index: %d in cache id: %d\n", index, id);
		return;
	}

	glyph_cache->glyphCache[id].entries[index] = entry;
}

void* glyph_cache_fragment_get(rdpGlyphCache* glyph_cache, uint8 index, uint8* size)
{
	void* entry;

	entry = glyph_cache->fragCache.entries[index].entry;
	*size = glyph_cache->fragCache.entries[index].size;

	return entry;
}

void glyph_cache_fragment_put(rdpGlyphCache* glyph_cache, uint8 index, uint8 size, void* entry)
{
	glyph_cache->fragCache.entries[index].entry = entry;
	glyph_cache->fragCache.entries[index].size = size;
}

void glyph_cache_register_callbacks(rdpUpdate* update)
{
	update->GlyphIndex = update_gdi_glyph_index;
	update->FastIndex = update_gdi_fast_index;
	update->CacheGlyph = update_gdi_cache_glyph;
	update->CacheGlyphV2 = update_gdi_cache_glyph_v2;
}

rdpGlyphCache* glyph_cache_new(rdpSettings* settings)
{
	rdpGlyphCache* glyph;

	glyph = (rdpGlyphCache*) xzalloc(sizeof(rdpGlyphCache));

	if (glyph != NULL)
	{
		int i;

		glyph->settings = settings;

		settings->glyphSupportLevel = GLYPH_SUPPORT_FULL;

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
