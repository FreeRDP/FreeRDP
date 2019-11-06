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

#include "glyph.h"

#define TAG FREERDP_TAG("cache.glyph")

static rdpGlyph* glyph_cache_get(rdpGlyphCache* glyph_cache, UINT32 id, UINT32 index);
static BOOL glyph_cache_put(rdpGlyphCache* glyph_cache, UINT32 id, UINT32 index, rdpGlyph* entry);

static const void* glyph_cache_fragment_get(rdpGlyphCache* glyph, UINT32 index, UINT32* count);
static BOOL glyph_cache_fragment_put(rdpGlyphCache* glyph, UINT32 index, UINT32 count,
                                     const void* entry);

static UINT32 update_glyph_offset(const BYTE* data, UINT32 index, INT32* x, INT32* y,
                                  UINT32 ulCharInc, UINT32 flAccel)
{
	if ((ulCharInc == 0) && (!(flAccel & SO_CHAR_INC_EQUAL_BM_BASE)))
	{
		UINT32 offset = data[index++];

		if (offset & 0x80)
		{
			offset = data[index++];
			offset |= ((UINT32)data[index++]) << 8;
		}

		if (flAccel & SO_VERTICAL)
			*y += offset;

		if (flAccel & SO_HORIZONTAL)
			*x += offset;
	}

	return index;
}

static BOOL update_process_glyph(rdpContext* context, const BYTE* data, UINT32 cacheIndex, INT32* x,
                                 INT32* y, UINT32 cacheId, UINT32 flAccel, BOOL fOpRedundant,
                                 const RDP_RECT* bound)
{
	INT32 sx = 0, sy = 0;
	INT32 dx, dy;
	rdpGlyph* glyph;
	rdpGlyphCache* glyph_cache;

	if (!context || !data || !x || !y || !context->graphics || !context->cache ||
	    !context->cache->glyph)
		return FALSE;

	glyph_cache = context->cache->glyph;
	glyph = glyph_cache_get(glyph_cache, cacheId, cacheIndex);

	if (!glyph)
		return FALSE;

	dx = glyph->x + *x;
	dy = glyph->y + *y;

	if (dx < bound->x)
	{
		sx = bound->x - dx;
		dx = bound->x;
	}

	if (dy < bound->y)
	{
		sy = bound->y - dy;
		dy = bound->y;
	}

	if ((dx <= (bound->x + bound->width)) && (dy <= (bound->y + bound->height)))
	{
		INT32 dw = glyph->cx - sx;
		INT32 dh = glyph->cy - sy;

		if ((dw + dx) > (bound->x + bound->width))
			dw = (bound->x + bound->width) - (dw + dx);

		if ((dh + dy) > (bound->y + bound->height))
			dh = (bound->y + bound->height) - (dh + dy);

		if ((dh > 0) && (dw > 0))
		{
			if (!glyph->Draw(context, glyph, dx, dy, dw, dh, sx, sy, fOpRedundant))
				return FALSE;
		}
	}

	if (flAccel & SO_CHAR_INC_EQUAL_BM_BASE)
		*x += glyph->cx;

	return TRUE;
}

static BOOL update_process_glyph_fragments(rdpContext* context, const BYTE* data, UINT32 length,
                                           UINT32 cacheId, UINT32 ulCharInc, UINT32 flAccel,
                                           UINT32 bgcolor, UINT32 fgcolor, INT32 x, INT32 y,
                                           INT32 bkX, INT32 bkY, INT32 bkWidth, INT32 bkHeight,
                                           INT32 opX, INT32 opY, INT32 opWidth, INT32 opHeight,
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
	RDP_RECT bound;

	if (!context || !data || !context->graphics || !context->cache || !context->cache->glyph)
		return FALSE;

	graphics = context->graphics;
	glyph_cache = context->cache->glyph;
	glyph = graphics->Glyph_Prototype;

	if (!glyph)
		return FALSE;

	/* Limit op rectangle to visible screen. */
	if (opX < 0)
	{
		opWidth += opX;
		opX = 0;
	}

	if (opY < 0)
	{
		opHeight += opY;
		opY = 0;
	}

	if (opWidth < 0)
		opWidth = 0;

	if (opHeight < 0)
		opHeight = 0;

	/* Limit bk rectangle to visible screen. */
	if (bkX < 0)
	{
		bkWidth += bkX;
		bkX = 0;
	}

	if (bkY < 0)
	{
		bkHeight += bkY;
		bkY = 0;
	}

	if (bkWidth < 0)
		bkWidth = 0;

	if (bkHeight < 0)
		bkHeight = 0;

	if (opX + opWidth > (INT64)context->settings->DesktopWidth)
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

	if (bkX + bkWidth > (INT64)context->settings->DesktopWidth)
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
		bkWidth = context->settings->DesktopWidth - bkX;
	}

	bound.x = bkX;
	bound.y = bkY;
	bound.width = bkWidth;
	bound.height = bkHeight;

	if (!glyph->BeginDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor, fOpRedundant))
		return FALSE;

	if (!IFCALLRESULT(TRUE, glyph->SetBounds, context, bkX, bkY, bkWidth, bkHeight))
		return FALSE;

	while (index < length)
	{
		const UINT32 op = data[index++];

		switch (op)
		{
			case GLYPH_FRAGMENT_USE:
				if (index + 1 > length)
					return FALSE;

				id = data[index++];
				fragments = (BYTE*)glyph_cache_fragment_get(glyph_cache, id, &size);

				if (fragments == NULL)
					return FALSE;

				for (n = 0; n < size;)
				{
					const UINT32 fop = fragments[n++];
					n = update_glyph_offset(fragments, n, &x, &y, ulCharInc, flAccel);

					if (!update_process_glyph(context, fragments, fop, &x, &y, cacheId, flAccel,
					                          fOpRedundant, &bound))
						return FALSE;
				}

				break;

			case GLYPH_FRAGMENT_ADD:
				if (index + 2 > length)
					return FALSE;

				id = data[index++];
				size = data[index++];
				glyph_cache_fragment_put(glyph_cache, id, size, data);
				break;

			default:
				index = update_glyph_offset(data, index, &x, &y, ulCharInc, flAccel);

				if (!update_process_glyph(context, data, op, &x, &y, cacheId, flAccel, fOpRedundant,
				                          &bound))
					return FALSE;

				break;
		}
	}

	return glyph->EndDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);
}

static BOOL update_gdi_glyph_index(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	INT32 bkWidth = 0, bkHeight = 0, opWidth = 0, opHeight = 0;

	if (!context || !glyphIndex || !context->cache)
		return FALSE;

	if (glyphIndex->bkRight > glyphIndex->bkLeft)
		bkWidth = glyphIndex->bkRight - glyphIndex->bkLeft + 1;

	if (glyphIndex->opRight > glyphIndex->opLeft)
		opWidth = glyphIndex->opRight - glyphIndex->opLeft + 1;

	if (glyphIndex->bkBottom > glyphIndex->bkTop)
		bkHeight = glyphIndex->bkBottom - glyphIndex->bkTop + 1;

	if (glyphIndex->opBottom > glyphIndex->opTop)
		opHeight = glyphIndex->opBottom - glyphIndex->opTop + 1;

	return update_process_glyph_fragments(
	    context, glyphIndex->data, glyphIndex->cbData, glyphIndex->cacheId, glyphIndex->ulCharInc,
	    glyphIndex->flAccel, glyphIndex->backColor, glyphIndex->foreColor, glyphIndex->x,
	    glyphIndex->y, glyphIndex->bkLeft, glyphIndex->bkTop, bkWidth, bkHeight, glyphIndex->opLeft,
	    glyphIndex->opTop, opWidth, opHeight, glyphIndex->fOpRedundant);
}

static BOOL update_gdi_fast_index(rdpContext* context, const FAST_INDEX_ORDER* fastIndex)
{
	INT32 x, y;
	INT32 opLeft, opTop;
	INT32 opRight, opBottom;
	INT32 opWidth = 0, opHeight = 0;
	INT32 bkWidth = 0, bkHeight = 0;

	if (!context || !fastIndex || !context->cache)
		return FALSE;

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
	if (opRight > (INT64)context->instance->settings->DesktopWidth)
		opRight = (int)context->instance->settings->DesktopWidth;

	if (x == -32768)
		x = fastIndex->bkLeft;

	if (y == -32768)
		y = fastIndex->bkTop;

	if (fastIndex->bkRight > fastIndex->bkLeft)
		bkWidth = fastIndex->bkRight - fastIndex->bkLeft + 1;

	if (fastIndex->bkBottom > fastIndex->bkTop)
		bkHeight = fastIndex->bkBottom - fastIndex->bkTop + 1;

	if (opRight > opLeft)
		opWidth = opRight - opLeft + 1;

	if (opBottom > opTop)
		opHeight = opBottom - opTop + 1;

	return update_process_glyph_fragments(
	    context, fastIndex->data, fastIndex->cbData, fastIndex->cacheId, fastIndex->ulCharInc,
	    fastIndex->flAccel, fastIndex->backColor, fastIndex->foreColor, x, y, fastIndex->bkLeft,
	    fastIndex->bkTop, bkWidth, bkHeight, opLeft, opTop, opWidth, opHeight, FALSE);
}

static BOOL update_gdi_fast_glyph(rdpContext* context, const FAST_GLYPH_ORDER* fastGlyph)
{
	INT32 x, y;
	BYTE text_data[2];
	INT32 opLeft, opTop;
	INT32 opRight, opBottom;
	INT32 opWidth = 0, opHeight = 0;
	INT32 bkWidth = 0, bkHeight = 0;
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
	if (opRight > (INT64)context->instance->settings->DesktopWidth)
		opRight = (int)context->instance->settings->DesktopWidth;

	if (x == -32768)
		x = fastGlyph->bkLeft;

	if (y == -32768)
		y = fastGlyph->bkTop;

	if ((fastGlyph->cbData > 1) && (fastGlyph->glyphData.aj))
	{
		/* got option font that needs to go into cache */
		rdpGlyph* glyph;
		const GLYPH_DATA_V2* glyphData = &fastGlyph->glyphData;

		glyph = Glyph_Alloc(context, glyphData->x, glyphData->y, glyphData->cx, glyphData->cy,
		                    glyphData->cb, glyphData->aj);

		if (!glyph)
			return FALSE;

		if (!glyph_cache_put(cache->glyph, fastGlyph->cacheId, fastGlyph->data[0], glyph))
		{
			glyph->Free(context, glyph);
			return FALSE;
		}
	}

	text_data[0] = fastGlyph->data[0];
	text_data[1] = 0;

	if (fastGlyph->bkRight > fastGlyph->bkLeft)
		bkWidth = fastGlyph->bkRight - fastGlyph->bkLeft + 1;

	if (fastGlyph->bkBottom > fastGlyph->bkTop)
		bkHeight = fastGlyph->bkBottom - fastGlyph->bkTop + 1;

	if (opRight > opLeft)
		opWidth = opRight - opLeft + 1;

	if (opBottom > opTop)
		opHeight = opBottom - opTop + 1;

	return update_process_glyph_fragments(
	    context, text_data, 1, fastGlyph->cacheId, fastGlyph->ulCharInc, fastGlyph->flAccel,
	    fastGlyph->backColor, fastGlyph->foreColor, x, y, fastGlyph->bkLeft, fastGlyph->bkTop,
	    bkWidth, bkHeight, opLeft, opTop, opWidth, opHeight, FALSE);
}

static BOOL update_gdi_cache_glyph(rdpContext* context, const CACHE_GLYPH_ORDER* cacheGlyph)
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

		if (!(glyph = Glyph_Alloc(context, glyph_data->x, glyph_data->y, glyph_data->cx,
		                          glyph_data->cy, glyph_data->cb, glyph_data->aj)))
			return FALSE;

		if (!glyph_cache_put(cache->glyph, cacheGlyph->cacheId, glyph_data->cacheIndex, glyph))
		{
			glyph->Free(context, glyph);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL update_gdi_cache_glyph_v2(rdpContext* context, const CACHE_GLYPH_V2_ORDER* cacheGlyphV2)
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

		glyph = Glyph_Alloc(context, glyphData->x, glyphData->y, glyphData->cx, glyphData->cy,
		                    glyphData->cb, glyphData->aj);

		if (!glyph)
			return FALSE;

		if (!glyph_cache_put(cache->glyph, cacheGlyphV2->cacheId, glyphData->cacheIndex, glyph))
		{
			glyph->Free(context, glyph);
			return FALSE;
		}
	}

	return TRUE;
}

rdpGlyph* glyph_cache_get(rdpGlyphCache* glyphCache, UINT32 id, UINT32 index)
{
	rdpGlyph* glyph;
	WLog_Print(glyphCache->log, WLOG_DEBUG, "GlyphCacheGet: id: %" PRIu32 " index: %" PRIu32 "", id,
	           index);

	if (id > 9)
	{
		WLog_ERR(TAG, "invalid glyph cache id: %" PRIu32 "", id);
		return NULL;
	}

	if (index > glyphCache->glyphCache[id].number)
	{
		WLog_ERR(TAG, "index %" PRIu32 " out of range for cache id: %" PRIu32 "", index, id);
		return NULL;
	}

	glyph = glyphCache->glyphCache[id].entries[index];

	if (!glyph)
		WLog_ERR(TAG, "no glyph found at cache index: %" PRIu32 " in cache id: %" PRIu32 "", index,
		         id);

	return glyph;
}

BOOL glyph_cache_put(rdpGlyphCache* glyphCache, UINT32 id, UINT32 index, rdpGlyph* glyph)
{
	rdpGlyph* prevGlyph;

	if (id > 9)
	{
		WLog_ERR(TAG, "invalid glyph cache id: %" PRIu32 "", id);
		return FALSE;
	}

	if (index > glyphCache->glyphCache[id].number)
	{
		WLog_ERR(TAG, "invalid glyph cache index: %" PRIu32 " in cache id: %" PRIu32 "", index, id);
		return FALSE;
	}

	WLog_Print(glyphCache->log, WLOG_DEBUG, "GlyphCachePut: id: %" PRIu32 " index: %" PRIu32 "", id,
	           index);
	prevGlyph = glyphCache->glyphCache[id].entries[index];

	if (prevGlyph)
		prevGlyph->Free(glyphCache->context, prevGlyph);

	glyphCache->glyphCache[id].entries[index] = glyph;
	return TRUE;
}

const void* glyph_cache_fragment_get(rdpGlyphCache* glyphCache, UINT32 index, UINT32* size)
{
	void* fragment;

	if (index > 255)
	{
		WLog_ERR(TAG, "invalid glyph cache fragment index: %" PRIu32 "", index);
		return NULL;
	}

	fragment = glyphCache->fragCache.entries[index].fragment;
	*size = (BYTE)glyphCache->fragCache.entries[index].size;
	WLog_Print(glyphCache->log, WLOG_DEBUG,
	           "GlyphCacheFragmentGet: index: %" PRIu32 " size: %" PRIu32 "", index, *size);

	if (!fragment)
		WLog_ERR(TAG, "invalid glyph fragment at index:%" PRIu32 "", index);

	return fragment;
}

BOOL glyph_cache_fragment_put(rdpGlyphCache* glyphCache, UINT32 index, UINT32 size,
                              const void* fragment)
{
	void* prevFragment;
	void* copy;

	if (index > 255)
	{
		WLog_ERR(TAG, "invalid glyph cache fragment index: %" PRIu32 "", index);
		return FALSE;
	}

	copy = malloc(size);

	if (!copy)
		return FALSE;

	WLog_Print(glyphCache->log, WLOG_DEBUG,
	           "GlyphCacheFragmentPut: index: %" PRIu32 " size: %" PRIu32 "", index, size);
	CopyMemory(copy, fragment, size);
	prevFragment = glyphCache->fragCache.entries[index].fragment;
	glyphCache->fragCache.entries[index].fragment = copy;
	glyphCache->fragCache.entries[index].size = size;
	free(prevFragment);
	return TRUE;
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
	int i;
	rdpGlyphCache* glyphCache;
	glyphCache = (rdpGlyphCache*)calloc(1, sizeof(rdpGlyphCache));

	if (!glyphCache)
		return NULL;

	glyphCache->log = WLog_Get("com.freerdp.cache.glyph");
	glyphCache->settings = settings;
	glyphCache->context = ((freerdp*)settings->instance)->update->context;

	for (i = 0; i < 10; i++)
	{
		glyphCache->glyphCache[i].number = settings->GlyphCache[i].cacheEntries;
		glyphCache->glyphCache[i].maxCellSize = settings->GlyphCache[i].cacheMaximumCellSize;
		glyphCache->glyphCache[i].entries =
		    (rdpGlyph**)calloc(glyphCache->glyphCache[i].number, sizeof(rdpGlyph*));

		if (!glyphCache->glyphCache[i].entries)
			goto fail;
	}

	glyphCache->fragCache.entries = calloc(256, sizeof(FRAGMENT_CACHE_ENTRY));

	if (!glyphCache->fragCache.entries)
		goto fail;

	return glyphCache;
fail:
	glyph_cache_free(glyphCache);
	return NULL;
}

void glyph_cache_free(rdpGlyphCache* glyphCache)
{
	if (glyphCache)
	{
		int i;
		GLYPH_CACHE* cache = glyphCache->glyphCache;

		for (i = 0; i < 10; i++)
		{
			UINT32 j;
			rdpGlyph** entries = cache[i].entries;

			if (!entries)
				continue;

			for (j = 0; j < cache[i].number; j++)
			{
				rdpGlyph* glyph = entries[j];

				if (glyph)
				{
					glyph->Free(glyphCache->context, glyph);
					entries[j] = NULL;
				}
			}

			free(entries);
			cache[i].entries = NULL;
		}

		if (glyphCache->fragCache.entries)
		{
			for (i = 0; i < 256; i++)
			{
				free(glyphCache->fragCache.entries[i].fragment);
				glyphCache->fragCache.entries[i].fragment = NULL;
			}
		}

		free(glyphCache->fragCache.entries);
		free(glyphCache);
	}
}

CACHE_GLYPH_ORDER* copy_cache_glyph_order(rdpContext* context, const CACHE_GLYPH_ORDER* glyph)
{
	size_t x;
	CACHE_GLYPH_ORDER* dst = calloc(1, sizeof(CACHE_GLYPH_ORDER));

	if (!dst || !glyph)
		goto fail;

	*dst = *glyph;

	for (x = 0; x < glyph->cGlyphs; x++)
	{
		const GLYPH_DATA* src = &glyph->glyphData[x];
		GLYPH_DATA* data = &dst->glyphData[x];

		if (src->aj)
		{
			const size_t size = src->cb;
			data->aj = malloc(size);

			if (!data->aj)
				goto fail;

			memcpy(data->aj, src->aj, size);
		}
	}

	if (glyph->unicodeCharacters)
	{
		if (glyph->cGlyphs == 0)
			goto fail;

		dst->unicodeCharacters = calloc(glyph->cGlyphs, sizeof(WCHAR));

		if (!dst->unicodeCharacters)
			goto fail;

		memcpy(dst->unicodeCharacters, glyph->unicodeCharacters, sizeof(WCHAR) * glyph->cGlyphs);
	}

	return dst;
fail:
	free_cache_glyph_order(context, dst);
	return NULL;
}

void free_cache_glyph_order(rdpContext* context, CACHE_GLYPH_ORDER* glyph)
{
	if (glyph)
	{
		size_t x;

		for (x = 0; x < ARRAYSIZE(glyph->glyphData); x++)
			free(glyph->glyphData[x].aj);

		free(glyph->unicodeCharacters);
	}

	free(glyph);
}

CACHE_GLYPH_V2_ORDER* copy_cache_glyph_v2_order(rdpContext* context,
                                                const CACHE_GLYPH_V2_ORDER* glyph)
{
	size_t x;
	CACHE_GLYPH_V2_ORDER* dst = calloc(1, sizeof(CACHE_GLYPH_V2_ORDER));

	if (!dst || !glyph)
		goto fail;

	*dst = *glyph;

	for (x = 0; x < glyph->cGlyphs; x++)
	{
		const GLYPH_DATA_V2* src = &glyph->glyphData[x];
		GLYPH_DATA_V2* data = &dst->glyphData[x];

		if (src->aj)
		{
			const size_t size = src->cb;
			data->aj = malloc(size);

			if (!data->aj)
				goto fail;

			memcpy(data->aj, src->aj, size);
		}
	}

	if (glyph->unicodeCharacters)
	{
		if (glyph->cGlyphs == 0)
			goto fail;

		dst->unicodeCharacters = calloc(glyph->cGlyphs, sizeof(WCHAR));

		if (!dst->unicodeCharacters)
			goto fail;

		memcpy(dst->unicodeCharacters, glyph->unicodeCharacters, sizeof(WCHAR) * glyph->cGlyphs);
	}

	return dst;
fail:
	free_cache_glyph_v2_order(context, dst);
	return NULL;
}

void free_cache_glyph_v2_order(rdpContext* context, CACHE_GLYPH_V2_ORDER* glyph)
{
	if (glyph)
	{
		size_t x;

		for (x = 0; x < ARRAYSIZE(glyph->glyphData); x++)
			free(glyph->glyphData[x].aj);

		free(glyph->unicodeCharacters);
	}

	free(glyph);
}
