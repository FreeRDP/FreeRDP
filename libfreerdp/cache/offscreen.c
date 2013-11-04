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

#include <freerdp/cache/offscreen.h>

void update_gdi_create_offscreen_bitmap(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* createOffscreenBitmap)
{
	int i;
	UINT16 index;
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;

	bitmap = Bitmap_Alloc(context);

	bitmap->width = createOffscreenBitmap->cx;
	bitmap->height = createOffscreenBitmap->cy;

	bitmap->New(context, bitmap);

	offscreen_cache_delete(cache->offscreen, createOffscreenBitmap->id);
	offscreen_cache_put(cache->offscreen, createOffscreenBitmap->id, bitmap);

	if(cache->offscreen->currentSurface == createOffscreenBitmap->id)
		Bitmap_SetSurface(context, bitmap, FALSE);

	for (i = 0; i < (int) createOffscreenBitmap->deleteList.cIndices; i++)
	{
		index = createOffscreenBitmap->deleteList.indices[i];
		offscreen_cache_delete(cache->offscreen, index);
	}
}

void update_gdi_switch_surface(rdpContext* context, SWITCH_SURFACE_ORDER* switchSurface)
{
	rdpCache* cache = context->cache;

	if (switchSurface->bitmapId == SCREEN_BITMAP_SURFACE)
	{
		Bitmap_SetSurface(context, NULL, TRUE);
	}
	else
	{
		rdpBitmap* bitmap;
		bitmap = offscreen_cache_get(cache->offscreen, switchSurface->bitmapId);
		Bitmap_SetSurface(context, bitmap, FALSE);
	}

	cache->offscreen->currentSurface = switchSurface->bitmapId;
}

rdpBitmap* offscreen_cache_get(rdpOffscreenCache* offscreenCache, UINT32 index)
{
	rdpBitmap* bitmap;

	if (index >= offscreenCache->maxEntries)
	{
		fprintf(stderr, "invalid offscreen bitmap index: 0x%04X\n", index);
		return NULL;
	}

	bitmap = offscreenCache->entries[index];

	if (!bitmap)
	{
		fprintf(stderr, "invalid offscreen bitmap at index: 0x%04X\n", index);
		return NULL;
	}

	return bitmap;
}

void offscreen_cache_put(rdpOffscreenCache* offscreenCache, UINT32 index, rdpBitmap* bitmap)
{
	if (index >= offscreenCache->maxEntries)
	{
		fprintf(stderr, "invalid offscreen bitmap index: 0x%04X\n", index);
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
		fprintf(stderr, "invalid offscreen bitmap index (delete): 0x%04X\n", index);
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

	offscreenCache = (rdpOffscreenCache*) malloc(sizeof(rdpOffscreenCache));

	if (offscreenCache)
	{
		ZeroMemory(offscreenCache, sizeof(rdpOffscreenCache));

		offscreenCache->settings = settings;
		offscreenCache->update = ((freerdp*) settings->instance)->update;

		offscreenCache->currentSurface = SCREEN_BITMAP_SURFACE;
		offscreenCache->maxSize = 7680;
		offscreenCache->maxEntries = 2000;

		settings->OffscreenCacheSize = offscreenCache->maxSize;
		settings->OffscreenCacheEntries = offscreenCache->maxEntries;

		offscreenCache->entries = (rdpBitmap**) malloc(sizeof(rdpBitmap*) * offscreenCache->maxEntries);
		ZeroMemory(offscreenCache->entries, sizeof(rdpBitmap*) * offscreenCache->maxEntries);
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

			if (bitmap)
				Bitmap_Free(offscreenCache->update->context, bitmap);
		}

		free(offscreenCache->entries);
		free(offscreenCache);
	}
}
