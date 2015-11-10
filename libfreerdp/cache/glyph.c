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
			offset = data[*index + 1] | ((int)((char)data[*index + 2]) << 8);
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

BOOL update_process_glyph_fragments(rdpContext* context, BYTE* data, UINT32 length,
		UINT32 cacheId, UINT32 ulCharInc, UINT32 flAccel, UINT32 bgcolor, UINT32 fgcolor, int x, int y,
		int bkX, int bkY, int bkWidth, int bkHeight, int opX, int opY, int opWidth, int opHeight, BOOL fOpRedundant)
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
		if (!Glyph_BeginDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor, fOpRedundant))
			return FALSE;
	}
	else
	{
		if (fOpRedundant)
		{
			if (!Glyph_BeginDraw(context, bkX, bkY, bkWidth, bkHeight, bgcolor, fgcolor, fOpRedundant))
				return FALSE;
		}
		else
		{
			if (!Glyph_BeginDraw(context, 0, 0, 0, 0, bgcolor, fgcolor, fOpRedundant))
				return FALSE;
		}
	}

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
				update_process_glyph(context, data, &index, &x, &y, cacheId, ulCharInc, flAccel);
				index++;
				break;
		}
	}

	if (opWidth > 0 && opHeight > 0)
		return Glyph_EndDraw(context, opX, opY, opWidth, opHeight, bgcolor, fgcolor);

	return Glyph_EndDraw(context, bkX, bkY, bkWidth, bkHeight, bgcolor, fgcolor);
}

BOOL update_gdi_glyph_index(rdpContext* context, GLYPH_INDEX_ORDER* glyphIndex)
{
	rdpGlyphCache* glyph_cache;
	int bkWidth, bkHeight, opWidth, opHeight;

	glyph_cache = context->cache->glyph;

	bkWidth = glyphIndex->bkRight - glyphIndex->bkLeft;
	opWidth = glyphIndex->opRight - glyphIndex->opLeft;
	bkHeight = glyphIndex->bkBottom - glyphIndex->bkTop;
	opHeight = glyphIndex->opBottom - glyphIndex->opTop;

	return update_process_glyph_fragments(context, glyphIndex->data, glyphIndex->cbData,
			glyphIndex->cacheId, glyphIndex->ulCharInc, glyphIndex->flAccel,
			glyphIndex->backColor, glyphIndex->foreColor, glyphIndex->x, glyphIndex->y,
			glyphIndex->bkLeft, glyphIndex->bkTop, bkWidth, bkHeight,
			glyphIndex->opLeft, glyphIndex->opTop, opWidth, opHeight,
			glyphIndex->fOpRedundant);
}

BOOL update_gdi_fast_index(rdpContext* context, FAST_INDEX_ORDER* fastIndex)
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

	/* Server can send a massive number (32766) which appears to be
	 * undocumented special behavior for "Erase all the way right".
	 * X11 has nondeterministic results asking for a draw that wide. */
	if (opRight > context->instance->settings->DesktopWidth)
		opRight = context->instance->settings->DesktopWidth;

	if (x == -32768)
		x = fastIndex->bkLeft;

	if (y == -32768)
		y = fastIndex->bkTop;

	return update_process_glyph_fragments(context, fastIndex->data, fastIndex->cbData,
			fastIndex->cacheId, fastIndex->ulCharInc, fastIndex->flAccel,
			fastIndex->backColor, fastIndex->foreColor, x, y,
			fastIndex->bkLeft, fastIndex->bkTop,
			fastIndex->bkRight - fastIndex->bkLeft, fastIndex->bkBottom - fastIndex->bkTop,
			opLeft, opTop,
			opRight - opLeft, opBottom - opTop,
			FALSE);
}

BOOL update_gdi_fast_glyph(rdpContext* context, FAST_GLYPH_ORDER* fastGlyph)
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
		glyphData = &fastGlyph->glyphData;

		glyph = Glyph_Alloc(context);
		if (!glyph)
			return FALSE;
		glyph->x = glyphData->x;
		glyph->y = glyphData->y;
		glyph->cx = glyphData->cx;
		glyph->cy = glyphData->cy;
		glyph->cb = glyphData->cb;
		glyph->aj = malloc(glyphData->cb);
		if (!glyph->aj)
			goto error_aj;
		CopyMemory(glyph->aj, glyphData->aj, glyph->cb);

		if (!Glyph_New(context, glyph))
			goto error_glyph_new;

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

error_glyph_new:
	free(glyph->aj);
	glyph->aj = NULL;
error_aj:
	Glyph_Free(context, glyph);
	return FALSE;
}

BOOL update_gdi_cache_glyph(rdpContext* context, CACHE_GLYPH_ORDER* cacheGlyph)
{
	int i;
	rdpGlyph* glyph;
	GLYPH_DATA* glyph_data;
	rdpCache* cache = context->cache;

	for (i = 0; i < (int) cacheGlyph->cGlyphs; i++)
	{
		glyph_data = &cacheGlyph->glyphData[i];

		glyph = Glyph_Alloc(context);
		if (!glyph)
			return FALSE;

		glyph->x = glyph_data->x;
		glyph->y = glyph_data->y;
		glyph->cx = glyph_data->cx;
		glyph->cy = glyph_data->cy;
		glyph->cb = glyph_data->cb;
		glyph->aj = glyph_data->aj;
		if (!Glyph_New(context, glyph))
		{
			Glyph_Free(context, glyph);
			return FALSE;
		}

		glyph_cache_put(cache->glyph, cacheGlyph->cacheId, glyph_data->cacheIndex, glyph);
	}
	return TRUE;
}

BOOL update_gdi_cache_glyph_v2(rdpContext* context, CACHE_GLYPH_V2_ORDER* cacheGlyphV2)
{
	int i;
	rdpGlyph* glyph;
	GLYPH_DATA_V2* glyphData;
	rdpCache* cache = context->cache;

	for (i = 0; i < (int) cacheGlyphV2->cGlyphs; i++)
	{
		glyphData = &cacheGlyphV2->glyphData[i];

		glyph = Glyph_Alloc(context);
		if (!glyph)
		{
			/* TODO: cleanup perviosly allocated glyph memory in error case */
			return FALSE;
		}

		glyph->x = glyphData->x;
		glyph->y = glyphData->y;
		glyph->cx = glyphData->cx;
		glyph->cy = glyphData->cy;
		glyph->cb = glyphData->cb;
		glyph->aj = glyphData->aj;
		Glyph_New(context, glyph);

		glyph_cache_put(cache->glyph, cacheGlyphV2->cacheId, glyphData->cacheIndex, glyph);
	}
	return TRUE;
}

rdpGlyph* glyph_cache_get(rdpGlyphCache* glyphCache, UINT32 id, UINT32 index)
{
	rdpGlyph* glyph;

	WLog_DBG(TAG, "GlyphCacheGet: id: %d index: %d", id, index);

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

void glyph_cache_put(rdpGlyphCache* glyphCache, UINT32 id, UINT32 index, rdpGlyph* glyph)
{
	rdpGlyph* prevGlyph;

	if (id > 9)
	{
		WLog_ERR(TAG, "invalid glyph cache id: %d", id);
		return;
	}

	if (index > glyphCache->glyphCache[id].number)
	{
		WLog_ERR(TAG, "invalid glyph cache index: %d in cache id: %d", index, id);
		return;
	}

	WLog_DBG(TAG, "GlyphCachePut: id: %d index: %d", id, index);

	prevGlyph = glyphCache->glyphCache[id].entries[index];

	if (prevGlyph)
	{
		Glyph_Free(glyphCache->context, prevGlyph);
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
		WLog_ERR(TAG, "invalid glyph cache fragment index: %d", index);
		return NULL;
	}

	fragment = glyphCache->fragCache.entries[index].fragment;
	*size = (BYTE) glyphCache->fragCache.entries[index].size;

	WLog_DBG(TAG, "GlyphCacheFragmentGet: index: %d size: %d", index, *size);

	if (!fragment)
		WLog_ERR(TAG, "invalid glyph fragment at index:%d", index);

	return fragment;
}

void glyph_cache_fragment_put(rdpGlyphCache* glyphCache, UINT32 index, UINT32 size, void* fragment)
{
	void* prevFragment;

	if (index > 255)
	{
		WLog_ERR(TAG, "invalid glyph cache fragment index: %d", index);
		return;
	}

	WLog_DBG(TAG, "GlyphCacheFragmentPut: index: %d size: %d", index, size);

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
			glyphCache->glyphCache[i].maxCellSize = settings->GlyphCache[i].cacheMaximumCellSize;
			glyphCache->glyphCache[i].entries = (rdpGlyph**) calloc(glyphCache->glyphCache[i].number, sizeof(rdpGlyph*));
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
			int j;

			for (j = 0; j < (int) glyphCache->glyphCache[i].number; j++)
			{
				rdpGlyph* glyph;

				glyph = glyphCache->glyphCache[i].entries[j];

				if (glyph)
				{
					Glyph_Free(glyphCache->context, glyph);
					free(glyph->aj);
					free(glyph);
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
