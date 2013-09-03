/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <winpr/crt.h>

#include <freerdp/freerdp.h>
#include <winpr/stream.h>

#include <freerdp/cache/glyph.h>

void update_process_glyph(rdpContext* context, BYTE* data, int* index,
		int* x, int* y, UINT32 cacheId, UINT32 ulCharInc, UINT32 flAccel)
{
	int offset;
	rdpGlyph* glyph;
	UINT32 cacheIndex;
	rdpGraphics* graphics;
	rdpGlyphCache* glyph_cache;

	graphics = context->graphics;
	glyph_cache = context->cache->glyph;

	cacheIndex = data[*index];

	glyph = glyph_cache_get(glyph_cache, cacheId, cacheIndex);

	if ((ulCharInc == 0) && (!(flAccel & SO_CHAR_INC_EQUAL_BM_BASE)))
	{
		/* Contrary to fragments, the offset is added before the glyph. */
		(*index)++;
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

void update_process_glyph_fragments(rdpContext* context, BYTE* data, UINT32 length,
		UINT32 cacheId, UINT32 ulCharInc, UINT32 flAccel, UINT32 bgcolor, UINT32 fgcolor, int x, int y,
		int bkX, int bkY, int bkWidth, int bkHeight, int opX, int opY, int opWidth, int opHeight)
{
	int n;
	UINT32 id;
	UINT32 size;
	int index = 0;
	BYTE* fragments;
	rdpGraphics* graphics;
	rdpGlyphCache* glyph_cache;

	graphics = context->graphics;
	glyph_cache = context->cache->glyph;

	if (opWidth > 0 && opHeight > 0)
		Glyph_BeginDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);
	else
		Glyph_BeginDraw(context, 0, 0, 0, 0, bgcolor, fgcolor);

	while (index < (int) length)
	{
		switch (data[index])
		{
			case GLYPH_FRAGMENT_USE:

				if (index + 2 > (int) length)
				{
					/* at least one byte need to follow */
					index = length = 0;
					break;
				}

				id = data[index + 1];
				fragments = (BYTE*) glyph_cache_fragment_get(glyph_cache, id, &size);

				if (fragments != NULL)
				{
					for (n = 0; n < (int) size; n++)
					{
						update_process_glyph(context, fragments, &n, &x, &y, cacheId, ulCharInc, flAccel);
					}

					/* Contrary to glyphs, the offset is added after the fragment. */
					if ((ulCharInc == 0) && (!(flAccel & SO_CHAR_INC_EQUAL_BM_BASE)))
					{
						if (flAccel & SO_VERTICAL)
							y += data[index + 2];
						else
							x += data[index + 2];
					}
				}

				index += (index + 2 < (int) length) ? 3 : 2;
				length -= index;
				data = &(data[index]);
				index = 0;

				break;

			case GLYPH_FRAGMENT_ADD:

				if (index + 3 > (int) length)
				{
					/* at least two bytes need to follow */
					index = length = 0;
					break;
				}

				id = data[index + 1];
				size = data[index + 2];

				fragments = (BYTE*) malloc(size);
				memcpy(fragments, data, size);
				glyph_cache_fragment_put(glyph_cache, id, size, fragments);

				index += 3;
				length -= index;
				data = &(data[index]);
				index = 0;

				break;

			default:
				update_process_glyph(context, data, &index, &x, &y, cacheId, ulCharInc, flAccel);
				index++;
				break;
		}
	}

	if (opWidth > 0 && opHeight > 0)
		Glyph_EndDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);
	else
		Glyph_EndDraw(context, bkX, bkY, bkWidth, bkHeight, bgcolor, fgcolor);
}

void update_gdi_glyph_index(rdpContext* context, GLYPH_INDEX_ORDER* glyph_index)
{
	rdpGlyphCache* glyph_cache;

	glyph_cache = context->cache->glyph;

	update_process_glyph_fragments(context, glyph_index->data, glyph_index->cbData,
			glyph_index->cacheId, glyph_index->ulCharInc, glyph_index->flAccel,
			glyph_index->backColor, glyph_index->foreColor, glyph_index->x, glyph_index->y,
			glyph_index->bkLeft, glyph_index->bkTop,
			glyph_index->bkRight - glyph_index->bkLeft, glyph_index->bkBottom - glyph_index->bkTop,
			glyph_index->opLeft, glyph_index->opTop,
			glyph_index->opRight - glyph_index->opLeft, glyph_index->opBottom - glyph_index->opTop);
}

void update_gdi_fast_index(rdpContext* context, FAST_INDEX_ORDER* fast_index)
{
	INT32 x, y;
	INT32 opLeft, opTop;
	INT32 opRight, opBottom;
	rdpGlyphCache* glyph_cache;

	glyph_cache = context->cache->glyph;

	opLeft = fast_index->opLeft;
	opTop = fast_index->opTop;
	opRight = fast_index->opRight;
	opBottom = fast_index->opBottom;
	x = fast_index->x;
	y = fast_index->y;

	if (opBottom == -32768)
	{
		BYTE flags = (BYTE) (opTop & 0x0F);

		if (flags & 0x01)
			opBottom = fast_index->bkBottom;
		if (flags & 0x02)
			opRight = fast_index->bkRight;
		if (flags & 0x04)
			opTop = fast_index->bkTop;
		if (flags & 0x08)
			opLeft = fast_index->bkLeft;
	}

	if (opLeft == 0)
		opLeft = fast_index->bkLeft;

	if (opRight == 0)
		opRight = fast_index->bkRight;

	if (x == -32768)
		x = fast_index->bkLeft;

	if (y == -32768)
		y = fast_index->bkTop;

	update_process_glyph_fragments(context, fast_index->data, fast_index->cbData,
			fast_index->cacheId, fast_index->ulCharInc, fast_index->flAccel,
			fast_index->backColor, fast_index->foreColor, x, y,
			fast_index->bkLeft, fast_index->bkTop,
			fast_index->bkRight - fast_index->bkLeft, fast_index->bkBottom - fast_index->bkTop,
			opLeft, opTop,
			opRight - opLeft, opBottom - opTop);
}

void update_gdi_fast_glyph(rdpContext* context, FAST_GLYPH_ORDER* fast_glyph)
{
	INT32 x, y;
	rdpGlyph* glyph;
	BYTE text_data[2];
	INT32 opLeft, opTop;
	INT32 opRight, opBottom;
	GLYPH_DATA_V2* glyph_data;
	rdpCache* cache = context->cache;

	opLeft = fast_glyph->opLeft;
	opTop = fast_glyph->opTop;
	opRight = fast_glyph->opRight;
	opBottom = fast_glyph->opBottom;
	x = fast_glyph->x;
	y = fast_glyph->y;

	if (opBottom == -32768)
	{
		BYTE flags = (BYTE) (opTop & 0x0F);

		if (flags & 0x01)
			opBottom = fast_glyph->bkBottom;
		if (flags & 0x02)
			opRight = fast_glyph->bkRight;
		if (flags & 0x04)
			opTop = fast_glyph->bkTop;
		if (flags & 0x08)
			opLeft = fast_glyph->bkLeft;
	}

	if (opLeft == 0)
		opLeft = fast_glyph->bkLeft;

	if (opRight == 0)
		opRight = fast_glyph->bkRight;

	if (x == -32768)
		x = fast_glyph->bkLeft;

	if (y == -32768)
		y = fast_glyph->bkTop;

	if (fast_glyph->cbData > 1)
	{
		/* got option font that needs to go into cache */
		glyph_data = &fast_glyph->glyphData;

		glyph = Glyph_Alloc(context);
		glyph->x = glyph_data->x;
		glyph->y = glyph_data->y;
		glyph->cx = glyph_data->cx;
		glyph->cy = glyph_data->cy;
		glyph->cb = glyph_data->cb;
		glyph->aj = glyph_data->aj;
		Glyph_New(context, glyph);

		glyph_cache_put(cache->glyph, fast_glyph->cacheId, fast_glyph->data[0], glyph);
	}

	text_data[0] = fast_glyph->data[0];
	text_data[1] = 0;

	update_process_glyph_fragments(context, text_data, 1,
			fast_glyph->cacheId, fast_glyph->ulCharInc, fast_glyph->flAccel,
			fast_glyph->backColor, fast_glyph->foreColor, x, y,
			fast_glyph->bkLeft, fast_glyph->bkTop,
			fast_glyph->bkRight - fast_glyph->bkLeft, fast_glyph->bkBottom - fast_glyph->bkTop,
			opLeft, opTop,
			opRight - opLeft, opBottom - opTop);
}

void update_gdi_cache_glyph(rdpContext* context, CACHE_GLYPH_ORDER* cache_glyph)
{
	int i;
	rdpGlyph* glyph;
	GLYPH_DATA* glyph_data;
	rdpCache* cache = context->cache;

	for (i = 0; i < (int) cache_glyph->cGlyphs; i++)
	{
		glyph_data = &cache_glyph->glyphData[i];

		glyph = Glyph_Alloc(context);

		glyph->x = glyph_data->x;
		glyph->y = glyph_data->y;
		glyph->cx = glyph_data->cx;
		glyph->cy = glyph_data->cy;
		glyph->cb = glyph_data->cb;
		glyph->aj = glyph_data->aj;
		Glyph_New(context, glyph);

		glyph_cache_put(cache->glyph, cache_glyph->cacheId, glyph_data->cacheIndex, glyph);
	}
}

void update_gdi_cache_glyph_v2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cache_glyph_v2)
{
	int i;
	rdpGlyph* glyph;
	GLYPH_DATA_V2* glyph_data;
	rdpCache* cache = context->cache;

	for (i = 0; i < (int) cache_glyph_v2->cGlyphs; i++)
	{
		glyph_data = &cache_glyph_v2->glyphData[i];

		glyph = Glyph_Alloc(context);

		glyph->x = glyph_data->x;
		glyph->y = glyph_data->y;
		glyph->cx = glyph_data->cx;
		glyph->cy = glyph_data->cy;
		glyph->cb = glyph_data->cb;
		glyph->aj = glyph_data->aj;
		Glyph_New(context, glyph);

		glyph_cache_put(cache->glyph, cache_glyph_v2->cacheId, glyph_data->cacheIndex, glyph);
	}
}

rdpGlyph* glyph_cache_get(rdpGlyphCache* glyph_cache, UINT32 id, UINT32 index)
{
	rdpGlyph* glyph;

	if (id > 9)
	{
		fprintf(stderr, "invalid glyph cache id: %d\n", id);
		return NULL;
	}

	if (index > glyph_cache->glyphCache[id].number)
	{
		fprintf(stderr, "invalid glyph cache index: %d in cache id: %d\n", index, id);
		return NULL;
	}

	glyph = glyph_cache->glyphCache[id].entries[index];

	if (glyph == NULL)
	{
		fprintf(stderr, "invalid glyph at cache index: %d in cache id: %d\n", index, id);
	}

	return glyph;
}

void glyph_cache_put(rdpGlyphCache* glyph_cache, UINT32 id, UINT32 index, rdpGlyph* glyph)
{
	rdpGlyph* prevGlyph;

	if (id > 9)
	{
		fprintf(stderr, "invalid glyph cache id: %d\n", id);
		return;
	}

	if (index > glyph_cache->glyphCache[id].number)
	{
		fprintf(stderr, "invalid glyph cache index: %d in cache id: %d\n", index, id);
		return;
	}

	prevGlyph = glyph_cache->glyphCache[id].entries[index];

	if (prevGlyph != NULL)
	{
		Glyph_Free(glyph_cache->context, prevGlyph);
		free(prevGlyph->aj);
		free(prevGlyph);
	}

	glyph_cache->glyphCache[id].entries[index] = glyph;
}

void* glyph_cache_fragment_get(rdpGlyphCache* glyph_cache, UINT32 index, UINT32* size)
{
	void* fragment;

	fragment = glyph_cache->fragCache.entries[index].fragment;
	*size = (BYTE) glyph_cache->fragCache.entries[index].size;

	if (fragment == NULL)
	{
		fprintf(stderr, "invalid glyph fragment at index:%d\n", index);
	}

	return fragment;
}

void glyph_cache_fragment_put(rdpGlyphCache* glyph_cache, UINT32 index, UINT32 size, void* fragment)
{
	void* prevFragment;

	prevFragment = glyph_cache->fragCache.entries[index].fragment;

	glyph_cache->fragCache.entries[index].fragment = fragment;
	glyph_cache->fragCache.entries[index].size = size;

	if (prevFragment != NULL)
	{
		free(prevFragment);
	}
}

void glyph_cache_register_callbacks(rdpUpdate* update)
{
	update->primary->GlyphIndex = update_gdi_glyph_index;
	update->primary->FastIndex = update_gdi_fast_index;
	update->primary->FastGlyph = update_gdi_fast_glyph;
	update->secondary->CacheGlyph = update_gdi_cache_glyph;
	update->secondary->CacheGlyphV2 = update_gdi_cache_glyph_v2;
}

rdpGlyphCache* glyph_cache_new(rdpSettings* settings)
{
	rdpGlyphCache* glyph;

	glyph = (rdpGlyphCache*) malloc(sizeof(rdpGlyphCache));
	ZeroMemory(glyph, sizeof(rdpGlyphCache));

	if (glyph != NULL)
	{
		int i;

		glyph->settings = settings;
		glyph->context = ((freerdp*) settings->instance)->update->context;

		for (i = 0; i < 10; i++)
		{
			glyph->glyphCache[i].number = settings->GlyphCache[i].cacheEntries;
			glyph->glyphCache[i].maxCellSize = settings->GlyphCache[i].cacheMaximumCellSize;
			glyph->glyphCache[i].entries = (rdpGlyph**) malloc(sizeof(rdpGlyph*) * glyph->glyphCache[i].number);
			ZeroMemory(glyph->glyphCache[i].entries, sizeof(rdpGlyph*) * glyph->glyphCache[i].number);
		}

		glyph->fragCache.entries = malloc(sizeof(FRAGMENT_CACHE_ENTRY) * 256);
		ZeroMemory(glyph->fragCache.entries, sizeof(FRAGMENT_CACHE_ENTRY) * 256);
	}

	return glyph;
}

void glyph_cache_free(rdpGlyphCache* glyph_cache)
{
	if (glyph_cache != NULL)
	{
		int i;
		void* fragment;

		for (i = 0; i < 10; i++)
		{
			int j;

			for (j = 0; j < (int) glyph_cache->glyphCache[i].number; j++)
			{
				rdpGlyph* glyph;

				glyph = glyph_cache->glyphCache[i].entries[j];

				if (glyph != NULL)
				{
					Glyph_Free(glyph_cache->context, glyph);
					free(glyph->aj);
					free(glyph);
					glyph_cache->glyphCache[i].entries[j] = NULL;
				}
			}
			free(glyph_cache->glyphCache[i].entries);
		}

		for (i = 0; i < 255; i++)
		{
			fragment = glyph_cache->fragCache.entries[i].fragment;
			free(fragment);
			glyph_cache->fragCache.entries[i].fragment = NULL;
		}

		free(glyph_cache->fragCache.entries);
		free(glyph_cache);
	}
}
