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

void update_process_glyph(rdpContext* context, uint8* data, int* index,
		int* x, int* y, uint8 cacheId, uint8 ulCharInc, uint8 flAccel)
{
	int offset;
	rdpGlyph* glyph;
	uint8 cacheIndex;
	rdpGraphics* graphics;
	rdpGlyphCache* glyph_cache;

	graphics = context->graphics;
	glyph_cache = context->cache->glyph;

	cacheIndex = data[(*index)++];

	glyph = glyph_cache_get(glyph_cache, cacheId, cacheIndex);

	if ((ulCharInc == 0) && (!(flAccel & SO_CHAR_INC_EQUAL_BM_BASE)))
	{
		offset = data[*index];

		if (offset & 0x80)
		{
			offset = data[*index + 1] | (data[*index + 2] << 8);
			(*index)++;
			(*index)++;
		}

		if (flAccel & SO_VERTICAL)
			*y += offset;
		else
			*x += offset;
	}

	if (glyph != NULL)
	{
		Glyph_Draw(context, glyph, glyph->x + *x, glyph->y + *y);

		if (flAccel & SO_CHAR_INC_EQUAL_BM_BASE)
			*x += glyph->cx;
	}
}

void update_process_glyph_fragments(rdpContext* context, uint8* data, uint8 length,
		uint8 cacheId, uint8 ulCharInc, uint8 flAccel, uint32 bgcolor, uint32 fgcolor, int x, int y,
		int bkX, int bkY, int bkWidth, int bkHeight, int opX, int opY, int opWidth, int opHeight)
{
	int n;
	uint8 id;
	uint8 size;
	int index = 0;
	uint8* fragments;
	rdpGraphics* graphics;
	rdpGlyphCache* glyph_cache;

	graphics = context->graphics;
	glyph_cache = context->cache->glyph;

	if (opWidth > 1)
		Glyph_BeginDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);
	else
		Glyph_BeginDraw(context, 0, 0, 0, 0, bgcolor, fgcolor);

	while (index < length)
	{
		switch (data[index])
		{
			case GLYPH_FRAGMENT_USE:

				printf("GLYPH_FRAGMENT_USE\n");

				if (index + 2 > length)
				{
					/* at least one byte need to follow */
					index = length = 0;
					break;
				}

				id = data[index + 1];
				fragments = (uint8*) glyph_cache_fragment_get(glyph_cache, id, &size);

				if (fragments != NULL)
				{
					if ((ulCharInc == 0) && (!(flAccel & SO_CHAR_INC_EQUAL_BM_BASE)))
					{
						if (flAccel & SO_VERTICAL)
							y += data[index + 2];
						else
							x += data[index + 2];
					}

					for (n = 0; n < size; n++)
					{
						update_process_glyph(context, fragments, &n, &x, &y, cacheId, ulCharInc, flAccel);
					}
				}

				index += (index + 2 < length) ? 3 : 2;
				length -= index;
				data = &(data[index]);
				index = 0;

				break;

			case GLYPH_FRAGMENT_ADD:

				printf("GLYPH_FRAGMENT_ADD\n");

				if (index + 3 > length)
				{
					/* at least two bytes need to follow */
					index = length = 0;
					break;
				}

				id = data[index + 1];
				size = data[index + 2];

				fragments = (uint8*) xmalloc(size);
				memcpy(fragments, data, size);
				glyph_cache_fragment_put(glyph_cache, id, size, fragments);

				index += 3;
				length -= index;
				data = &(data[index]);
				index = 0;

				break;

			default:
				printf("GLYPH_FRAGMENT_NOP\n");

				update_process_glyph(context, data, &index, &x, &y, cacheId, ulCharInc, flAccel);
				index++;
				break;
		}
	}

	if (opWidth > 1)
		Glyph_EndDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);
	else
		Glyph_EndDraw(context, bkX, bkY, bkWidth, bkHeight, bgcolor, fgcolor);
}

void update_gdi_glyph_index(rdpContext* context, GLYPH_INDEX_ORDER* glyph_index)
{

}

void update_gdi_fast_index(rdpContext* context, FAST_INDEX_ORDER* fast_index)
{
	rdpGlyphCache* glyph_cache;

	glyph_cache = context->cache->glyph;

	fast_index->x = fast_index->bkLeft;

	update_process_glyph_fragments(context, fast_index->data, fast_index->cbData,
			fast_index->cacheId, fast_index->ulCharInc, fast_index->flAccel,
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

		glyph = Glyph_Alloc(update->context);

		glyph->x = glyph_data->x;
		glyph->y = glyph_data->y;
		glyph->cx = glyph_data->cx;
		glyph->cy = glyph_data->cy;
		glyph->aj = glyph_data->aj;
		glyph->cb = glyph_data->cb;
		Glyph_New(update->context, glyph);

		glyph_cache_put(cache->glyph, cache_glyph->cacheId, glyph_data->cacheIndex, glyph);
	}
}

void update_gdi_cache_glyph_v2(rdpUpdate* update, CACHE_GLYPH_V2_ORDER* cache_glyph_v2)
{

}

rdpGlyph* glyph_cache_get(rdpGlyphCache* glyph_cache, uint8 id, uint16 index)
{
	rdpGlyph* glyph;

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

	glyph = glyph_cache->glyphCache[id].entries[index];

	if (glyph == NULL)
	{
		printf("invalid glyph at cache index: %d in cache id: %d\n", index, id);
	}

	return glyph;
}

void glyph_cache_put(rdpGlyphCache* glyph_cache, uint8 id, uint16 index, rdpGlyph* glyph)
{
	rdpGlyph* prevGlyph;

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

	prevGlyph = glyph_cache->glyphCache[id].entries[index];

	if (prevGlyph != NULL)
	{
		xfree(prevGlyph->aj);
		Glyph_Free(glyph_cache->context, prevGlyph);
		xfree(prevGlyph);
	}

	glyph_cache->glyphCache[id].entries[index] = glyph;
}

void* glyph_cache_fragment_get(rdpGlyphCache* glyph_cache, uint8 index, uint8* size)
{
	void* fragment;

	fragment = glyph_cache->fragCache.entries[index].fragment;
	*size = (uint8) glyph_cache->fragCache.entries[index].size;

	if (fragment == NULL)
	{
		printf("invalid glyph fragment at index:%d\n", index);
	}

	return fragment;
}

void glyph_cache_fragment_put(rdpGlyphCache* glyph_cache, uint8 index, uint8 size, void* fragment)
{
	void* prevFragment;

	prevFragment = glyph_cache->fragCache.entries[index].fragment;

	glyph_cache->fragCache.entries[index].fragment = fragment;
	glyph_cache->fragCache.entries[index].size = size;

	if (prevFragment != NULL)
	{
		xfree(prevFragment);
	}
}

void glyph_cache_register_callbacks(rdpUpdate* update)
{
	update->primary->GlyphIndex = update_gdi_glyph_index;
	update->primary->FastIndex = update_gdi_fast_index;
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
		glyph->context = ((freerdp*) settings->instance)->update->context;

		if (settings->glyph_cache)
			settings->glyphSupportLevel = GLYPH_SUPPORT_FULL;

		for (i = 0; i < 10; i++)
		{
			glyph->glyphCache[i].number = settings->glyphCache[i].cacheEntries;
			glyph->glyphCache[i].maxCellSize = settings->glyphCache[i].cacheMaximumCellSize;
			glyph->glyphCache[i].entries = (rdpGlyph**) xzalloc(sizeof(rdpGlyph*) * glyph->glyphCache[i].number);
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
