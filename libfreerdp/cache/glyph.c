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

#include <freerdp/utils/debug.h>
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

void update_gdi_glyph_index(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	rdpGlyphCache* glyph_cache;

	glyph_cache = context->cache->glyph;

	update_process_glyph_fragments(context, glyphIndex->data, glyphIndex->cbData,
			glyphIndex->cacheId, glyphIndex->ulCharInc, glyphIndex->flAccel,
			glyphIndex->backColor, glyphIndex->foreColor, glyphIndex->x, glyphIndex->y,
			glyphIndex->bkLeft, glyphIndex->bkTop,
			glyphIndex->bkRight - glyphIndex->bkLeft, glyphIndex->bkBottom - glyphIndex->bkTop,
			glyphIndex->opLeft, glyphIndex->opTop,
			glyphIndex->opRight - glyphIndex->opLeft, glyphIndex->opBottom - glyphIndex->opTop);
}

void update_gdi_fast_index(rdpContext* context, FAST_INDEX_ORDER* fastIndex)
{
	INT32 x, y;
	INT32 opLeft, opTop;
	INT32 opRight, opBottom;
	rdpGlyphCache* glyph_cache;

	glyph_cache = context->cache->glyph;

	opLeft = fastIndex->opLeft;
	opTop = fastIndex->opTop;
	opRight = fastIndex->opRight;
	opBottom = fastIndex->opBottom;
	x = fastIndex->x;
	y = fastIndex->y;

	if (opBottom == -32768)
	{
		BYTE flags = (BYTE) (opTop & 0x0F);

		if (flags & 0x01)
			opBottom = fastIndex->bkBottom;
		if (flags & 0x02)
			opRight = fastIndex->bkRight;
		if (flags & 0x04)
			opTop = fastIndex->bkTop;
		if (flags & 0x08)
			opLeft = fastIndex->bkLeft;
	}

	if (opLeft == 0)
		opLeft = fastIndex->bkLeft;

	if (opRight == 0)
		opRight = fastIndex->bkRight;

	if (x == -32768)
		x = fastIndex->bkLeft;

	if (y == -32768)
		y = fastIndex->bkTop;

	update_process_glyph_fragments(context, fastIndex->data, fastIndex->cbData,
			fastIndex->cacheId, fastIndex->ulCharInc, fastIndex->flAccel,
			fastIndex->backColor, fastIndex->foreColor, x, y,
			fastIndex->bkLeft, fastIndex->bkTop,
			fastIndex->bkRight - fastIndex->bkLeft, fastIndex->bkBottom - fastIndex->bkTop,
			opLeft, opTop,
			opRight - opLeft, opBottom - opTop);
}

void update_gdi_fast_glyph(rdpContext* context, FAST_GLYPH_ORDER* fastGlyph)
{
	INT32 x, y;
	rdpGlyph* glyph;
	BYTE text_data[2];
	INT32 opLeft, opTop;
	INT32 opRight, opBottom;
	GLYPH_DATA_V2* glyphData;
	rdpCache* cache = context->cache;

	opLeft = fastGlyph->opLeft;
	opTop = fastGlyph->opTop;
	opRight = fastGlyph->opRight;
	opBottom = fastGlyph->opBottom;
	x = fastGlyph->x;
	y = fastGlyph->y;

	if (opBottom == -32768)
	{
		BYTE flags = (BYTE) (opTop & 0x0F);

		if (flags & 0x01)
			opBottom = fastGlyph->bkBottom;
		if (flags & 0x02)
			opRight = fastGlyph->bkRight;
		if (flags & 0x04)
			opTop = fastGlyph->bkTop;
		if (flags & 0x08)
			opLeft = fastGlyph->bkLeft;
	}

	if (opLeft == 0)
		opLeft = fastGlyph->bkLeft;

	if (opRight == 0)
		opRight = fastGlyph->bkRight;

	if (x == -32768)
		x = fastGlyph->bkLeft;

	if (y == -32768)
		y = fastGlyph->bkTop;

	if ((fastGlyph->cbData > 1) && (fastGlyph->glyphData.aj))
	{
		/* got option font that needs to go into cache */
		glyphData = &fastGlyph->glyphData;

		glyph = Glyph_Alloc(context);
		glyph->x = glyphData->x;
		glyph->y = glyphData->y;
		glyph->cx = glyphData->cx;
		glyph->cy = glyphData->cy;
		glyph->cb = glyphData->cb;
		glyph->aj = malloc(glyphData->cb);
		CopyMemory(glyph->aj, glyphData->aj, glyph->cb);
		Glyph_New(context, glyph);

		glyph_cache_put(cache->glyph, fastGlyph->cacheId, fastGlyph->data[0], glyph);
	}

	text_data[0] = fastGlyph->data[0];
	text_data[1] = 0;

	update_process_glyph_fragments(context, text_data, 1,
			fastGlyph->cacheId, fastGlyph->ulCharInc, fastGlyph->flAccel,
			fastGlyph->backColor, fastGlyph->foreColor, x, y,
			fastGlyph->bkLeft, fastGlyph->bkTop,
			fastGlyph->bkRight - fastGlyph->bkLeft, fastGlyph->bkBottom - fastGlyph->bkTop,
			opLeft, opTop,
			opRight - opLeft, opBottom - opTop);
}

void update_gdi_cache_glyph(rdpContext* context, CACHE_GLYPH_ORDER* cacheGlyph)
{
	int i;
	rdpGlyph* glyph;
	GLYPH_DATA* glyph_data;
	rdpCache* cache = context->cache;

	for (i = 0; i < (int) cacheGlyph->cGlyphs; i++)
	{
		glyph_data = &cacheGlyph->glyphData[i];

		glyph = Glyph_Alloc(context);

		glyph->x = glyph_data->x;
		glyph->y = glyph_data->y;
		glyph->cx = glyph_data->cx;
		glyph->cy = glyph_data->cy;
		glyph->cb = glyph_data->cb;
		glyph->aj = glyph_data->aj;
		Glyph_New(context, glyph);

		glyph_cache_put(cache->glyph, cacheGlyph->cacheId, glyph_data->cacheIndex, glyph);
	}
}

void update_gdi_cache_glyph_v2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cacheGlyphV2)
{
	int i;
	rdpGlyph* glyph;
	GLYPH_DATA_V2* glyphData;
	rdpCache* cache = context->cache;

	for (i = 0; i < (int) cacheGlyphV2->cGlyphs; i++)
	{
		glyphData = &cacheGlyphV2->glyphData[i];

		glyph = Glyph_Alloc(context);

		glyph->x = glyphData->x;
		glyph->y = glyphData->y;
		glyph->cx = glyphData->cx;
		glyph->cy = glyphData->cy;
		glyph->cb = glyphData->cb;
		glyph->aj = glyphData->aj;
		Glyph_New(context, glyph);

		glyph_cache_put(cache->glyph, cacheGlyphV2->cacheId, glyphData->cacheIndex, glyph);
	}
}

rdpGlyph* glyph_cache_get(rdpGlyphCache* glyphCache, UINT32 id, UINT32 index)
{
	rdpGlyph* glyph;

	WLog_Print(glyphCache->log, WLOG_DEBUG, "GlyphCacheGet: id: %d index: %d", id, index);

	if (id > 9)
	{
		DEBUG_WARN( "invalid glyph cache id: %d\n", id);
		return NULL;
	}

	if (index > glyphCache->glyphCache[id].number)
	{
		DEBUG_WARN( "index %d out of range for cache id: %d\n", index, id);
		return NULL;
	}

	glyph = glyphCache->glyphCache[id].entries[index];

	if (!glyph)
		DEBUG_WARN( "no glyph found at cache index: %d in cache id: %d\n", index, id);

	return glyph;
}

void glyph_cache_put(rdpGlyphCache* glyphCache, UINT32 id, UINT32 index, rdpGlyph* glyph)
{
	rdpGlyph* prevGlyph;

	if (id > 9)
	{
		DEBUG_WARN( "invalid glyph cache id: %d\n", id);
		return;
	}

	if (index > glyphCache->glyphCache[id].number)
	{
		DEBUG_WARN( "invalid glyph cache index: %d in cache id: %d\n", index, id);
		return;
	}

	WLog_Print(glyphCache->log, WLOG_DEBUG, "GlyphCachePut: id: %d index: %d", id, index);

	prevGlyph = glyphCache->glyphCache[id].entries[index];

	if (prevGlyph)
	{
		Glyph_Free(glyphCache->context, prevGlyph);

		if (prevGlyph->aj)
			free(prevGlyph->aj);
		free(prevGlyph);
	}

	glyphCache->glyphCache[id].entries[index] = glyph;
}

void* glyph_cache_fragment_get(rdpGlyphCache* glyphCache, UINT32 index, UINT32* size)
{
	void* fragment;

	if (index > 255)
	{
		DEBUG_WARN( "invalid glyph cache fragment index: %d\n", index);
		return NULL;
	}

	fragment = glyphCache->fragCache.entries[index].fragment;
	*size = (BYTE) glyphCache->fragCache.entries[index].size;

	WLog_Print(glyphCache->log, WLOG_DEBUG, "GlyphCacheFragmentGet: index: %d size: %d", index, *size);

	if (!fragment)
		DEBUG_WARN( "invalid glyph fragment at index:%d\n", index);

	return fragment;
}

void glyph_cache_fragment_put(rdpGlyphCache* glyphCache, UINT32 index, UINT32 size, void* fragment)
{
	void* prevFragment;

	if (index > 255)
	{
		DEBUG_WARN( "invalid glyph cache fragment index: %d\n", index);
		return;
	}

	WLog_Print(glyphCache->log, WLOG_DEBUG, "GlyphCacheFragmentPut: index: %d size: %d", index, size);

	prevFragment = glyphCache->fragCache.entries[index].fragment;

	glyphCache->fragCache.entries[index].fragment = fragment;
	glyphCache->fragCache.entries[index].size = size;

	if (!prevFragment)
		free(prevFragment);
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
	rdpGlyphCache* glyphCache;

	glyphCache = (rdpGlyphCache*) malloc(sizeof(rdpGlyphCache));

	if (glyphCache)
	{
		int i;

		ZeroMemory(glyphCache, sizeof(rdpGlyphCache));

		WLog_Init();
		glyphCache->log = WLog_Get("com.freerdp.cache.glyph");

		glyphCache->settings = settings;
		glyphCache->context = ((freerdp*) settings->instance)->update->context;

		for (i = 0; i < 10; i++)
		{
			glyphCache->glyphCache[i].number = settings->GlyphCache[i].cacheEntries;
			glyphCache->glyphCache[i].maxCellSize = settings->GlyphCache[i].cacheMaximumCellSize;
			glyphCache->glyphCache[i].entries = (rdpGlyph**) malloc(sizeof(rdpGlyph*) * glyphCache->glyphCache[i].number);
			ZeroMemory(glyphCache->glyphCache[i].entries, sizeof(rdpGlyph*) * glyphCache->glyphCache[i].number);
		}

		glyphCache->fragCache.entries = malloc(sizeof(FRAGMENT_CACHE_ENTRY) * 256);
		ZeroMemory(glyphCache->fragCache.entries, sizeof(FRAGMENT_CACHE_ENTRY) * 256);
	}

	return glyphCache;
}

void glyph_cache_free(rdpGlyphCache* glyphCache)
{
	if (glyphCache)
	{
		int i;
		void* fragment;

		for (i = 0; i < 10; i++)
		{
			int j;

			for (j = 0; j < (int) glyphCache->glyphCache[i].number; j++)
			{
				rdpGlyph* glyph;

				glyph = glyphCache->glyphCache[i].entries[j];

				if (glyph)
				{
					Glyph_Free(glyphCache->context, glyph);

					if (glyph->aj)
						free(glyph->aj);
					free(glyph);

					glyphCache->glyphCache[i].entries[j] = NULL;
				}
			}
			free(glyphCache->glyphCache[i].entries);
			glyphCache->glyphCache[i].entries = NULL;
		}

		for (i = 0; i < 255; i++)
		{
			fragment = glyphCache->fragCache.entries[i].fragment;
			free(fragment);
			glyphCache->fragCache.entries[i].fragment = NULL;
		}

		free(glyphCache->fragCache.entries);
		free(glyphCache);
	}
}
