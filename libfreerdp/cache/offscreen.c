/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <winpr/crt.h>

#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/cache/offscreen.h>

#include "../core/graphics.h"

#define TAG FREERDP_TAG("cache.offscreen")

static void offscreen_cache_put(rdpOffscreenCache* offscreen_cache,
                                UINT32 index, rdpBitmap* bitmap);
static void offscreen_cache_delete(rdpOffscreenCache* offscreen, UINT32 index);

static BOOL update_gdi_create_offscreen_bitmap(rdpContext* context,
        const CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
{
	UINT32 i;
	UINT16 index;
	rdpBitmap* bitmap;
	rdpCache* cache;

	if (!context || !createOffscreenBitmap || !context->cache)
		return FALSE;

	cache = context->cache;
	bitmap = Bitmap_Alloc(context);

	if (!bitmap)
		return FALSE;

	Bitmap_SetDimensions(bitmap, createOffscreenBitmap->cx,
	                     createOffscreenBitmap->cy);

	if (!bitmap->New(context, bitmap))
	{
		free(bitmap);
		return FALSE;
	}

	offscreen_cache_delete(cache->offscreen, createOffscreenBitmap->id);
	offscreen_cache_put(cache->offscreen, createOffscreenBitmap->id, bitmap);

	if (cache->offscreen->currentSurface == createOffscreenBitmap->id)
		bitmap->SetSurface(context, bitmap, FALSE);

	for (i = 0; i < createOffscreenBitmap->deleteList.cIndices; i++)
	{
		index = createOffscreenBitmap->deleteList.indices[i];
		offscreen_cache_delete(cache->offscreen, index);
	}

	return TRUE;
}

static BOOL update_gdi_switch_surface(rdpContext* context,
                                      const SWITCH_SURFACE_ORDER* switchSurface)
{
	rdpCache* cache;
	rdpBitmap* bitmap;

	if (!context || !context->cache || !switchSurface || !context->graphics)
		return FALSE;

	cache = context->cache;
	bitmap = context->graphics->Bitmap_Prototype;
	if (!bitmap)
		return FALSE;

	if (switchSurface->bitmapId == SCREEN_BITMAP_SURFACE)
	{
		bitmap->SetSurface(context, NULL, TRUE);
	}
	else
	{
		rdpBitmap* bmp;
		bmp = offscreen_cache_get(cache->offscreen, switchSurface->bitmapId);
		bitmap->SetSurface(context, bmp, FALSE);
	}

	cache->offscreen->currentSurface = switchSurface->bitmapId;
	return TRUE;
}

rdpBitmap* offscreen_cache_get(rdpOffscreenCache* offscreenCache, UINT32 index)
{
	rdpBitmap* bitmap;

	if (index >= offscreenCache->maxEntries)
	{
		WLog_ERR(TAG,  "invalid offscreen bitmap index: 0x%08"PRIX32"", index);
		return NULL;
	}

	bitmap = offscreenCache->entries[index];

	if (!bitmap)
	{
		WLog_ERR(TAG,  "invalid offscreen bitmap at index: 0x%08"PRIX32"", index);
		return NULL;
	}

	return bitmap;
}

void offscreen_cache_put(rdpOffscreenCache* offscreenCache, UINT32 index,
                         rdpBitmap* bitmap)
{
	if (index >= offscreenCache->maxEntries)
	{
		WLog_ERR(TAG,  "invalid offscreen bitmap index: 0x%08"PRIX32"", index);
		return;
	}

	offscreen_cache_delete(offscreenCache, index);
	offscreenCache->entries[index] = bitmap;
}

void offscreen_cache_delete(rdpOffscreenCache* offscreenCache, UINT32 index)
{
	rdpBitmap* prevBitmap;

	if (index >= offscreenCache->maxEntries)
	{
		WLog_ERR(TAG,  "invalid offscreen bitmap index (delete): 0x%08"PRIX32"", index);
		return;
	}

	prevBitmap = offscreenCache->entries[index];

	if (prevBitmap != NULL)
		Bitmap_Free(offscreenCache->update->context, prevBitmap);

	offscreenCache->entries[index] = NULL;
}

void offscreen_cache_register_callbacks(rdpUpdate* update)
{
	update->altsec->CreateOffscreenBitmap = update_gdi_create_offscreen_bitmap;
	update->altsec->SwitchSurface = update_gdi_switch_surface;
}

rdpOffscreenCache* offscreen_cache_new(rdpSettings* settings)
{
	rdpOffscreenCache* offscreenCache;
	offscreenCache = (rdpOffscreenCache*) calloc(1, sizeof(rdpOffscreenCache));

	if (!offscreenCache)
		return NULL;

	offscreenCache->settings = settings;
	offscreenCache->update = ((freerdp*) settings->instance)->update;
	offscreenCache->currentSurface = SCREEN_BITMAP_SURFACE;
	offscreenCache->maxSize = 7680;
	offscreenCache->maxEntries = 2000;
	settings->OffscreenCacheSize = offscreenCache->maxSize;
	settings->OffscreenCacheEntries = offscreenCache->maxEntries;
	offscreenCache->entries = (rdpBitmap**) calloc(offscreenCache->maxEntries,
	                          sizeof(rdpBitmap*));

	if (!offscreenCache->entries)
	{
		free(offscreenCache);
		return NULL;
	}

	return offscreenCache;
}

void offscreen_cache_free(rdpOffscreenCache* offscreenCache)
{
	int i;
	rdpBitmap* bitmap;

	if (offscreenCache)
	{
		for (i = 0; i < (int) offscreenCache->maxEntries; i++)
		{
			bitmap = offscreenCache->entries[i];
			Bitmap_Free(offscreenCache->update->context, bitmap);
		}

		free(offscreenCache->entries);
		free(offscreenCache);
	}
}
