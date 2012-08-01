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

void update_gdi_create_offscreen_bitmap(rdpContext* context, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	int i;
	uint16 index;
	rdpBitmap* bitmap;
	rdpCache* cache = context->cache;

	bitmap = Bitmap_Alloc(context);

	bitmap->width = create_offscreen_bitmap->cx;
	bitmap->height = create_offscreen_bitmap->cy;

	bitmap->New(context, bitmap);

	offscreen_cache_delete(cache->offscreen, create_offscreen_bitmap->id);
	offscreen_cache_put(cache->offscreen, create_offscreen_bitmap->id, bitmap);

	if(cache->offscreen->currentSurface == create_offscreen_bitmap->id)
		Bitmap_SetSurface(context, bitmap, false);

	for (i = 0; i < (int) create_offscreen_bitmap->deleteList.cIndices; i++)
	{
		index = create_offscreen_bitmap->deleteList.indices[i];
		offscreen_cache_delete(cache->offscreen, index);
	}
}

void update_gdi_switch_surface(rdpContext* context, SWITCH_SURFACE_ORDER* switch_surface)
{
	rdpCache* cache = context->cache;

	if (switch_surface->bitmapId == SCREEN_BITMAP_SURFACE)
	{
		Bitmap_SetSurface(context, NULL, true);
	}
	else
	{
		rdpBitmap* bitmap;
		bitmap = offscreen_cache_get(cache->offscreen, switch_surface->bitmapId);
		Bitmap_SetSurface(context, bitmap, false);
	}

	cache->offscreen->currentSurface = switch_surface->bitmapId;
}

rdpBitmap* offscreen_cache_get(rdpOffscreenCache* offscreen_cache, uint32 index)
{
	rdpBitmap* bitmap;

	if (index >= offscreen_cache->maxEntries)
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

void offscreen_cache_put(rdpOffscreenCache* offscreen, uint32 index, rdpBitmap* bitmap)
{
	if (index >= offscreen->maxEntries)
	{
		printf("invalid offscreen bitmap index: 0x%04X\n", index);
		return;
	}

	offscreen_cache_delete(offscreen, index);
	offscreen->entries[index] = bitmap;
}

void offscreen_cache_delete(rdpOffscreenCache* offscreen, uint32 index)
{
	rdpBitmap* prevBitmap;

	if (index >= offscreen->maxEntries)
	{
		printf("invalid offscreen bitmap index (delete): 0x%04X\n", index);
		return;
	}

	prevBitmap = offscreen->entries[index];

	if (prevBitmap != NULL)
		Bitmap_Free(offscreen->update->context, prevBitmap);

	offscreen->entries[index] = NULL;
}

void offscreen_cache_register_callbacks(rdpUpdate* update)
{
	update->altsec->CreateOffscreenBitmap = update_gdi_create_offscreen_bitmap;
	update->altsec->SwitchSurface = update_gdi_switch_surface;
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
		offscreen_cache->maxEntries = 2000;

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
		for (i = 0; i < (int) offscreen_cache->maxEntries; i++)
		{
			bitmap = offscreen_cache->entries[i];

			if (bitmap != NULL)
				Bitmap_Free(offscreen_cache->update->context, bitmap);
		}

		xfree(offscreen_cache->entries);
		xfree(offscreen_cache);
	}
}
