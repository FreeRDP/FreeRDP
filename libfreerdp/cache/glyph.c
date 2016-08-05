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

#include <freerdp/log.h>
#include <freerdp/cache/glyph.h>

#define TAG FREERDP_TAG("cache.glyph")

static rdpGlyph* glyph_cache_get(rdpGlyphCache* glyph_cache, UINT32 id,
                                 UINT32 index);
static BOOL glyph_cache_put(rdpGlyphCache* glyph_cache, UINT32 id, UINT32 index,
                            rdpGlyph* entry);

static void* glyph_cache_fragment_get(rdpGlyphCache* glyph, UINT32 index,
                                      UINT32* count);
static void glyph_cache_fragment_put(rdpGlyphCache* glyph, UINT32 index,
                                     UINT32 count, void* entry);

static BOOL update_process_glyph(rdpContext* context, const BYTE* data,
                                 UINT32* index,	INT32* x, INT32* y,
                                 UINT32 cacheId, UINT32 ulCharInc, UINT32 flAccel)
{
	INT32 offset;
	rdpGlyph* glyph;
	UINT32 cacheIndex;
	rdpGraphics* graphics;
	rdpGlyphCache* glyph_cache;

	if (!context || !data || !index || !x || !y || !context->graphics
	    || !context->cache || !context->cache->glyph)
		return FALSE;

	graphics = context->graphics;
	glyph_cache = context->cache->glyph;
	cacheIndex = data[*index];
	glyph = glyph_cache_get(glyph_cache, cacheId, cacheIndex);

	if (!glyph)
		return FALSE;

	if ((ulCharInc == 0) && (!(flAccel & SO_CHAR_INC_EQUAL_BM_BASE)))
	{
		/* Contrary to fragments, the offset is added before the glyph. */
		(*index)++;
		offset = data[*index];

		if (offset & 0x80)
		{
			offset = data[*index + 1] | ((INT32)data[*index + 2]) << 8;
			(*index)++;
			(*index)++;
		}

		if (flAccel & SO_VERTICAL)
			*y += offset;
		else if (flAccel & SO_HORIZONTAL)
			*x += offset;
	}

	if (glyph != NULL)
	{
		if (!glyph->Draw(context, glyph, glyph->x + *x, glyph->y + *y))
			return FALSE;

		if (flAccel & SO_CHAR_INC_EQUAL_BM_BASE)
			*x += glyph->cx;
	}

	return TRUE;
}

static BOOL update_process_glyph_fragments(rdpContext* context,
        const BYTE* data,
        UINT32 length, UINT32 cacheId,
        UINT32 ulCharInc, UINT32 flAccel,
        UINT32 bgcolor, UINT32 fgcolor,
        INT32 x, INT32 y,
        INT32 bkX, INT32 bkY,
        INT32 bkWidth, INT32 bkHeight,
        INT32 opX, INT32 opY,
        INT32 opWidth, INT32 opHeight,
        BOOL fOpRedundant)
{
	UINT32 n;
	UINT32 id;
	UINT32 size;
	UINT32 index = 0;
	BYTE* fragments;
	rdpGraphics* graphics;
	rdpGlyphCache* glyph_cache;
	rdpGlyph* glyph;

	if (!context || !data || !context->graphics || !context->cache
	    || !context->cache->glyph)
		return FALSE;

	graphics = context->graphics;
	glyph_cache = context->cache->glyph;
	glyph = graphics->Glyph_Prototype;

	if (!glyph)
		return FALSE;

	if (opX + opWidth > context->settings->DesktopWidth)
	{
		/**
		 * Some Microsoft servers send erroneous high values close to the
		 * sint16 maximum in the OpRight field of the GlyphIndex, FastIndex and
		 * FastGlyph drawing orders, probably a result of applications trying to
		 * clear the text line to the very right end.
		 * One example where this can be seen is typing in notepad.exe within
		 * a RDP session to Windows XP Professional SP3.
		 * This workaround prevents resulting problems in the UI callbacks.
		 */
		opWidth = context->settings->DesktopWidth - opX;
	}

	if (opWidth > 0 && opHeight > 0)
	{
		if (!glyph->BeginDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor,
		                      fOpRedundant))
			return FALSE;
	}
	else
	{
		if (fOpRedundant)
		{
			if (!glyph->BeginDraw(context, bkX, bkY, bkWidth, bkHeight, bgcolor, fgcolor,
			                      fOpRedundant))
				return FALSE;
		}
		else
		{
			if (!glyph->BeginDraw(context, 0, 0, 0, 0, bgcolor, fgcolor, fOpRedundant))
				return FALSE;
		}
	}

	while (index < length)
	{
		switch (data[index])
		{
			case GLYPH_FRAGMENT_USE:
				if (index + 2 > length)
				{
					/* at least one byte need to follow */
					index = length = 0;
					break;
				}

				id = data[index + 1];
				fragments = (BYTE*) glyph_cache_fragment_get(glyph_cache, id, &size);

				if (fragments != NULL)
				{
					for (n = 0; n < size; n++)
					{
						update_process_glyph(context, fragments, &n, &x, &y, cacheId, ulCharInc,
						                     flAccel);
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

				index += (index + 2 < length) ? 3 : 2;
				length -= index;
				data = &(data[index]);
				index = 0;
				break;

			case GLYPH_FRAGMENT_ADD:
				if (index + 3 > length)
				{
					/* at least two bytes need to follow */
					index = length = 0;
					break;
				}

				id = data[index + 1];
				size = data[index + 2];
				fragments = (BYTE*) malloc(size);

				if (!fragments)
					return FALSE;

				CopyMemory(fragments, data, size);
				glyph_cache_fragment_put(glyph_cache, id, size, fragments);
				index += 3;
				length -= index;
				data = &(data[index]);
				index = 0;
				break;

			default:
				update_process_glyph(context, data, &index, &x, &y, cacheId, ulCharInc,
				                     flAccel);
				index++;
				break;
		}
	}

	if (opWidth > 0 && opHeight > 0)
		return glyph->EndDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);

	return glyph->EndDraw(context, bkX, bkY, bkWidth, bkHeight, bgcolor, fgcolor);
}

static BOOL update_gdi_glyph_index(rdpContext* context,
                                   GLYPH_INDEX_ORDER* glyphIndex)
{
	rdpGlyphCache* glyph_cache;
	int bkWidth, bkHeight, opWidth, opHeight;

	if (!context || !glyphIndex || !context->cache)
		return FALSE;

	glyph_cache = context->cache->glyph;
	bkWidth = glyphIndex->bkRight - glyphIndex->bkLeft;
	opWidth = glyphIndex->opRight - glyphIndex->opLeft;
	bkHeight = glyphIndex->bkBottom - glyphIndex->bkTop;
	opHeight = glyphIndex->opBottom - glyphIndex->opTop;
	return update_process_glyph_fragments(context, glyphIndex->data,
	                                      glyphIndex->cbData,
	                                      glyphIndex->cacheId, glyphIndex->ulCharInc, glyphIndex->flAccel,
	                                      glyphIndex->backColor, glyphIndex->foreColor, glyphIndex->x, glyphIndex->y,
	                                      glyphIndex->bkLeft, glyphIndex->bkTop, bkWidth, bkHeight,
	                                      glyphIndex->opLeft, glyphIndex->opTop, opWidth, opHeight,
	                                      glyphIndex->fOpRedundant);
}

static BOOL update_gdi_fast_index(rdpContext* context,
                                  const FAST_INDEX_ORDER* fastIndex)
{
	INT32 x, y;
	INT32 opLeft, opTop;
	INT32 opRight, opBottom;
	rdpGlyphCache* glyph_cache;

	if (!context || !fastIndex || !context->cache)
		return FALSE;

	glyph_cache = context->cache->glyph;
	opLeft = fastIndex->opLeft;
	opTop = fastIndex->opTop;
	opRight = fastIndex->opRight;
	opBottom = fastIndex->opBottom;
	x = fastIndex->x;
	y = fastIndex->y;

	if (opBottom == -32768)
	{
		BYTE flags = (BYTE)(opTop & 0x0F);

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

	/* Server can send a massive number (32766) which appears to be
	 * undocumented special behavior for "Erase all the way right".
	 * X11 has nondeterministic results asking for a draw that wide. */
	if (opRight > context->instance->settings->DesktopWidth)
		opRight = context->instance->settings->DesktopWidth;

	if (x == -32768)
		x = fastIndex->bkLeft;

	if (y == -32768)
		y = fastIndex->bkTop;

	return update_process_glyph_fragments(context, fastIndex->data,
	                                      fastIndex->cbData,
	                                      fastIndex->cacheId, fastIndex->ulCharInc, fastIndex->flAccel,
	                                      fastIndex->backColor, fastIndex->foreColor, x, y,
	                                      fastIndex->bkLeft, fastIndex->bkTop,
	                                      fastIndex->bkRight - fastIndex->bkLeft, fastIndex->bkBottom - fastIndex->bkTop,
	                                      opLeft, opTop,
	                                      opRight - opLeft, opBottom - opTop,
	                                      FALSE);
}

static BOOL update_gdi_fast_glyph(rdpContext* context,
                                  const FAST_GLYPH_ORDER* fastGlyph)
{
	INT32 x, y;
	BYTE text_data[2];
	INT32 opLeft, opTop;
	INT32 opRight, opBottom;
	rdpCache* cache;

	if (!context || !fastGlyph || !context->cache)
		return FALSE;

	cache = context->cache;
	opLeft = fastGlyph->opLeft;
	opTop = fastGlyph->opTop;
	opRight = fastGlyph->opRight;
	opBottom = fastGlyph->opBottom;
	x = fastGlyph->x;
	y = fastGlyph->y;

	if (opBottom == -32768)
	{
		BYTE flags = (BYTE)(opTop & 0x0F);

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

	/* See update_gdi_fast_index opRight comment. */
	if (opRight > context->instance->settings->DesktopWidth)
		opRight = context->instance->settings->DesktopWidth;

	if (x == -32768)
		x = fastGlyph->bkLeft;

	if (y == -32768)
		y = fastGlyph->bkTop;

	if ((fastGlyph->cbData > 1) && (fastGlyph->glyphData.aj))
	{
		/* got option font that needs to go into cache */
		rdpGlyph* glyph;
		const GLYPH_DATA_V2* glyphData = &fastGlyph->glyphData;

		if (!glyphData)
			return FALSE;

		glyph = Glyph_Alloc(context, x, y, glyphData->cx, glyphData->cy,
		                    glyphData->cb, glyphData->aj);

		if (!glyph)
			return FALSE;

		glyph_cache_put(cache->glyph, fastGlyph->cacheId, fastGlyph->data[0], glyph);
	}

	text_data[0] = fastGlyph->data[0];
	text_data[1] = 0;
	return update_process_glyph_fragments(context, text_data, 1,
	                                      fastGlyph->cacheId, fastGlyph->ulCharInc, fastGlyph->flAccel,
	                                      fastGlyph->backColor, fastGlyph->foreColor, x, y,
	                                      fastGlyph->bkLeft, fastGlyph->bkTop,
	                                      fastGlyph->bkRight - fastGlyph->bkLeft, fastGlyph->bkBottom - fastGlyph->bkTop,
	                                      opLeft, opTop,
	                                      opRight - opLeft, opBottom - opTop,
	                                      FALSE);
}

static BOOL update_gdi_cache_glyph(rdpContext* context,
                                   const CACHE_GLYPH_ORDER* cacheGlyph)
{
	UINT32 i;
	rdpCache* cache;

	if (!context || !cacheGlyph || !context->cache)
		return FALSE;

	cache = context->cache;

	for (i = 0; i < cacheGlyph->cGlyphs; i++)
	{
		const GLYPH_DATA* glyph_data = &cacheGlyph->glyphData[i];
		rdpGlyph* glyph;

		if (!glyph_data)
			return FALSE;

		if (!(glyph = Glyph_Alloc(context, glyph_data->x,
		                          glyph_data->y,
		                          glyph_data->cx,
		                          glyph_data->cy,
		                          glyph_data->cb,
		                          glyph_data->aj)))
			return FALSE;

		glyph_cache_put(cache->glyph, cacheGlyph->cacheId, glyph_data->cacheIndex,
		                glyph);
	}

	return TRUE;
}

static BOOL update_gdi_cache_glyph_v2(rdpContext* context,
                                      const CACHE_GLYPH_V2_ORDER* cacheGlyphV2)
{
	UINT32 i;
	rdpCache* cache;

	if (!context || !cacheGlyphV2 || !context->cache)
		return FALSE;

	cache = context->cache;

	for (i = 0; i < cacheGlyphV2->cGlyphs; i++)
	{
		const GLYPH_DATA_V2* glyphData = &cacheGlyphV2->glyphData[i];
		rdpGlyph* glyph;

		if (!glyphData)
			return FALSE;

		glyph = Glyph_Alloc(context, glyphData->x,
		                    glyphData->y,
		                    glyphData->cx,
		                    glyphData->cy,
		                    glyphData->cb,
		                    glyphData->aj);

		if (!glyph)
			return FALSE;

		glyph_cache_put(cache->glyph, cacheGlyphV2->cacheId, glyphData->cacheIndex,
		                glyph);
	}

	return TRUE;
}

rdpGlyph* glyph_cache_get(rdpGlyphCache* glyphCache, UINT32 id, UINT32 index)
{
	rdpGlyph* glyph;
	WLog_Print(glyphCache->log, WLOG_DEBUG, "GlyphCacheGet: id: %d index: %d", id,
	           index);

	if (id > 9)
	{
		WLog_ERR(TAG, "invalid glyph cache id: %d", id);
		return NULL;
	}

	if (index > glyphCache->glyphCache[id].number)
	{
		WLog_ERR(TAG, "index %d out of range for cache id: %d", index, id);
		return NULL;
	}

	glyph = glyphCache->glyphCache[id].entries[index];

	if (!glyph)
		WLog_ERR(TAG, "no glyph found at cache index: %d in cache id: %d", index, id);

	return glyph;
}

BOOL glyph_cache_put(rdpGlyphCache* glyphCache, UINT32 id, UINT32 index,
                     rdpGlyph* glyph)
{
	rdpGlyph* prevGlyph;

	if (id > 9)
	{
		WLog_ERR(TAG, "invalid glyph cache id: %d", id);
		return FALSE;
	}

	if (index > glyphCache->glyphCache[id].number)
	{
		WLog_ERR(TAG, "invalid glyph cache index: %d in cache id: %d", index, id);
		return FALSE;
	}

	WLog_Print(glyphCache->log, WLOG_DEBUG, "GlyphCachePut: id: %d index: %d", id,
	           index);
	prevGlyph = glyphCache->glyphCache[id].entries[index];

	if (prevGlyph)
		prevGlyph->Free(glyphCache->context, prevGlyph);

	glyphCache->glyphCache[id].entries[index] = glyph;
	return TRUE;
}

void* glyph_cache_fragment_get(rdpGlyphCache* glyphCache, UINT32 index,
                               UINT32* size)
{
	void* fragment;

	if (index > 255)
	{
		WLog_ERR(TAG, "invalid glyph cache fragment index: %d", index);
		return NULL;
	}

	fragment = glyphCache->fragCache.entries[index].fragment;
	*size = (BYTE) glyphCache->fragCache.entries[index].size;
	WLog_Print(glyphCache->log, WLOG_DEBUG,
	           "GlyphCacheFragmentGet: index: %d size: %d", index, *size);

	if (!fragment)
		WLog_ERR(TAG, "invalid glyph fragment at index:%d", index);

	return fragment;
}

void glyph_cache_fragment_put(rdpGlyphCache* glyphCache, UINT32 index,
                              UINT32 size, void* fragment)
{
	void* prevFragment;

	if (index > 255)
	{
		WLog_ERR(TAG, "invalid glyph cache fragment index: %d", index);
		return;
	}

	WLog_Print(glyphCache->log, WLOG_DEBUG,
	           "GlyphCacheFragmentPut: index: %d size: %d", index, size);
	prevFragment = glyphCache->fragCache.entries[index].fragment;
	glyphCache->fragCache.entries[index].fragment = fragment;
	glyphCache->fragCache.entries[index].size = size;
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
	glyphCache = (rdpGlyphCache*) calloc(1, sizeof(rdpGlyphCache));

	if (glyphCache)
	{
		int i;
		WLog_Init();
		glyphCache->log = WLog_Get("com.freerdp.cache.glyph");
		glyphCache->settings = settings;
		glyphCache->context = ((freerdp*) settings->instance)->update->context;

		for (i = 0; i < 10; i++)
		{
			glyphCache->glyphCache[i].number = settings->GlyphCache[i].cacheEntries;
			glyphCache->glyphCache[i].maxCellSize =
			    settings->GlyphCache[i].cacheMaximumCellSize;
			glyphCache->glyphCache[i].entries = (rdpGlyph**) calloc(
			                                        glyphCache->glyphCache[i].number, sizeof(rdpGlyph*));
		}

		glyphCache->fragCache.entries = calloc(256, sizeof(FRAGMENT_CACHE_ENTRY));
	}

	return glyphCache;
}

void glyph_cache_free(rdpGlyphCache* glyphCache)
{
	if (glyphCache)
	{
		int i;

		for (i = 0; i < 10; i++)
		{
			UINT32 j;

			for (j = 0; j < glyphCache->glyphCache[i].number; j++)
			{
				rdpGlyph* glyph;
				glyph = glyphCache->glyphCache[i].entries[j];

				if (glyph)
				{
					glyph->Free(glyphCache->context, glyph);
					glyphCache->glyphCache[i].entries[j] = NULL;
				}
			}

			free(glyphCache->glyphCache[i].entries);
			glyphCache->glyphCache[i].entries = NULL;
		}

		for (i = 0; i < 256; i++)
		{
			free(glyphCache->fragCache.entries[i].fragment);
			glyphCache->fragCache.entries[i].fragment = NULL;
		}

		free(glyphCache->fragCache.entries);
		free(glyphCache);
	}
}
