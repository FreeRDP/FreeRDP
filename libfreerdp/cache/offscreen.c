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

#include <freerdp/config.h>

#include <stdio.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/cache/offscreen.h>
#include <freerdp/cache/cache.h>

#include "../core/graphics.h"

#define TAG FREERDP_TAG("cache.offscreen")

struct rdp_offscreen_cache
{
	UINT32 maxSize;        /* 0 */
	UINT32 maxEntries;     /* 1 */
	rdpBitmap** entries;   /* 2 */
	UINT32 currentSurface; /* 3 */

	rdpContext* context;
};

static void offscreen_cache_put(rdpOffscreenCache* offscreen_cache, UINT32 index,
                                rdpBitmap* bitmap);
static void offscreen_cache_delete(rdpOffscreenCache* offscreen, UINT32 index);

static BOOL
update_gdi_create_offscreen_bitmap(rdpContext* context,
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

	Bitmap_SetDimensions(bitmap, createOffscreenBitmap->cx, createOffscreenBitmap->cy);

	if (!bitmap->New(context, bitmap))
	{
		Bitmap_Free(context, bitmap);
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
		if (bmp == NULL)
			return FALSE;

		bitmap->SetSurface(context, bmp, FALSE);
	}

	cache->offscreen->currentSurface = switchSurface->bitmapId;
	return TRUE;
}

rdpBitmap* offscreen_cache_get(rdpOffscreenCache* offscreenCache, UINT32 index)
{
	rdpBitmap* bitmap;

	WINPR_ASSERT(offscreenCache);

	if (index >= offscreenCache->maxEntries)
	{
		WLog_ERR(TAG, "invalid offscreen bitmap index: 0x%08" PRIX32 "", index);
		return NULL;
	}

	bitmap = offscreenCache->entries[index];

	if (!bitmap)
	{
		WLog_ERR(TAG, "invalid offscreen bitmap at index: 0x%08" PRIX32 "", index);
		return NULL;
	}

	return bitmap;
}

void offscreen_cache_put(rdpOffscreenCache* offscreenCache, UINT32 index, rdpBitmap* bitmap)
{
	WINPR_ASSERT(offscreenCache);

	if (index >= offscreenCache->maxEntries)
	{
		WLog_ERR(TAG, "invalid offscreen bitmap index: 0x%08" PRIX32 "", index);
		return;
	}

	offscreen_cache_delete(offscreenCache, index);
	offscreenCache->entries[index] = bitmap;
}

void offscreen_cache_delete(rdpOffscreenCache* offscreenCache, UINT32 index)
{
	rdpBitmap* prevBitmap;

	WINPR_ASSERT(offscreenCache);

	if (index >= offscreenCache->maxEntries)
	{
		WLog_ERR(TAG, "invalid offscreen bitmap index (delete): 0x%08" PRIX32 "", index);
		return;
	}

	prevBitmap = offscreenCache->entries[index];

	if (prevBitmap != NULL)
		Bitmap_Free(offscreenCache->context, prevBitmap);

	offscreenCache->entries[index] = NULL;
}

void offscreen_cache_register_callbacks(rdpUpdate* update)
{
	WINPR_ASSERT(update);
	WINPR_ASSERT(update->altsec);

	update->altsec->CreateOffscreenBitmap = update_gdi_create_offscreen_bitmap;
	update->altsec->SwitchSurface = update_gdi_switch_surface;
}

rdpOffscreenCache* offscreen_cache_new(rdpContext* context)
{
	rdpOffscreenCache* offscreenCache;
	rdpSettings* settings;

	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	offscreenCache = (rdpOffscreenCache*)calloc(1, sizeof(rdpOffscreenCache));

	if (!offscreenCache)
		return NULL;

	offscreenCache->context = context;
	offscreenCache->currentSurface = SCREEN_BITMAP_SURFACE;
	offscreenCache->maxSize = 7680;
	offscreenCache->maxEntries = 2000;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OffscreenCacheSize, offscreenCache->maxSize))
		goto fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OffscreenCacheEntries,
	                                 offscreenCache->maxEntries))
		goto fail;
	offscreenCache->entries = (rdpBitmap**)calloc(offscreenCache->maxEntries, sizeof(rdpBitmap*));

	if (!offscreenCache->entries)
		goto fail;

	return offscreenCache;
fail:
	offscreen_cache_free(offscreenCache);
	return NULL;
}

void offscreen_cache_free(rdpOffscreenCache* offscreenCache)
{
	if (offscreenCache)
	{
		size_t i;
		if (offscreenCache->entries)
		{
			for (i = 0; i < offscreenCache->maxEntries; i++)
			{
				rdpBitmap* bitmap = offscreenCache->entries[i];
				Bitmap_Free(offscreenCache->context, bitmap);
			}
		}

		free(offscreenCache->entries);
		free(offscreenCache);
	}
}
