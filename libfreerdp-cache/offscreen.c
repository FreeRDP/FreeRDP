/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/cache/offscreen.h>

void update_gdi_create_offscreen_bitmap(rdpUpdate* update, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	rdpBitmap* bitmap;
	rdpBitmap* prevBitmap;
	rdpCache* cache = update->context->cache;

	bitmap = Bitmap_Alloc(update->context);

	bitmap->width = create_offscreen_bitmap->cx;
	bitmap->height = create_offscreen_bitmap->cy;

	bitmap->New(update->context, bitmap);

	prevBitmap = offscreen_cache_get(cache->offscreen, create_offscreen_bitmap->id);

	if (prevBitmap != NULL)
		Bitmap_Free(update->context, prevBitmap);

	offscreen_cache_put(cache->offscreen, create_offscreen_bitmap->id, bitmap);

	if(cache->offscreen->currentSurface == create_offscreen_bitmap->id)
		Bitmap_SetSurface(update->context, bitmap, False);
}

void update_gdi_switch_surface(rdpUpdate* update, SWITCH_SURFACE_ORDER* switch_surface)
{
	rdpCache* cache = update->context->cache;

	if (switch_surface->bitmapId == SCREEN_BITMAP_SURFACE)
	{
		Bitmap_SetSurface(update->context, NULL, True);
	}
	else
	{
		rdpBitmap* bitmap;
		bitmap = offscreen_cache_get(cache->offscreen, switch_surface->bitmapId);
		Bitmap_SetSurface(update->context, bitmap, False);
	}

	cache->offscreen->currentSurface = switch_surface->bitmapId;
}

rdpBitmap* offscreen_cache_get(rdpOffscreenCache* offscreen_cache, uint16 index)
{
	rdpBitmap* bitmap;

	if (index > offscreen_cache->maxEntries)
	{
		printf("invalid offscreen bitmap index: 0x%04X\n", index);
		return NULL;
	}

	bitmap = offscreen_cache->entries[index];

	if (bitmap == NULL)
	{
		printf("invalid offscreen bitmap at index: 0x%04X\n", index);
		return NULL;
	}

	return bitmap;
}

void offscreen_cache_put(rdpOffscreenCache* offscreen, uint16 index, rdpBitmap* bitmap)
{
	if (index > offscreen->maxEntries)
	{
		printf("invalid offscreen bitmap index: 0x%04X\n", index);
		return;
	}

	offscreen->entries[index] = bitmap;
}

void offscreen_cache_register_callbacks(rdpUpdate* update)
{
	update->CreateOffscreenBitmap = update_gdi_create_offscreen_bitmap;
	update->SwitchSurface = update_gdi_switch_surface;
}

rdpOffscreenCache* offscreen_cache_new(rdpSettings* settings)
{
	rdpOffscreenCache* offscreen_cache;

	offscreen_cache = (rdpOffscreenCache*) xzalloc(sizeof(rdpOffscreenCache));

	if (offscreen_cache != NULL)
	{
		offscreen_cache->settings = settings;
		offscreen_cache->update = ((freerdp*) settings->instance)->update;

		offscreen_cache->currentSurface = SCREEN_BITMAP_SURFACE;
		offscreen_cache->maxSize = 7680;
		offscreen_cache->maxEntries = 100;

		settings->offscreen_bitmap_cache_size = offscreen_cache->maxSize;
		settings->offscreen_bitmap_cache_entries = offscreen_cache->maxEntries;

		offscreen_cache->entries = (rdpBitmap**) xzalloc(sizeof(rdpBitmap*) * offscreen_cache->maxEntries);
	}

	return offscreen_cache;
}

void offscreen_cache_free(rdpOffscreenCache* offscreen_cache)
{
	int i;
	rdpBitmap* bitmap;

	if (offscreen_cache != NULL)
	{
		for (i = 0; i < offscreen_cache->maxEntries; i++)
		{
			bitmap = offscreen_cache->entries[i];

			if (bitmap != NULL)
				Bitmap_Free(offscreen_cache->update->context, bitmap);
		}

		xfree(offscreen_cache->entries);
		xfree(offscreen_cache);
	}
}
