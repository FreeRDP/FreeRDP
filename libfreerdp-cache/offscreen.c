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

#include "offscreen.h"

void* offscreen_get(rdpOffscreen* offscreen, uint16 index)
{
	void* bitmap;

	if (index > offscreen->maxEntries)
	{
		printf("invalid offscreen bitmap index: 0x%04X\n", index);
		return NULL;
	}

	bitmap = offscreen->entries[index].bitmap;

	if (bitmap == NULL)
	{
		printf("invalid offscreen bitmap at index: 0x%04X\n", index);
		return NULL;
	}

	return bitmap;
}

void offscreen_put(rdpOffscreen* offscreen, uint16 index, void* bitmap)
{
	if (index > offscreen->maxEntries)
	{
		printf("invalid offscreen bitmap index: 0x%04X\n", index);
		return;
	}

	offscreen->entries[index].bitmap = bitmap;
}

rdpOffscreen* offscreen_new(rdpSettings* settings)
{
	rdpOffscreen* offscreen;

	offscreen = (rdpOffscreen*) xzalloc(sizeof(rdpOffscreen));

	if (offscreen != NULL)
	{
		offscreen->settings = settings;

		offscreen->maxSize = 7680;
		offscreen->maxEntries = 100;

		settings->offscreen_bitmap_cache = True;
		settings->offscreen_bitmap_cache_size = offscreen->maxSize;
		settings->offscreen_bitmap_cache_entries = offscreen->maxEntries;

		offscreen->entries = (OFFSCREEN_ENTRY*) xzalloc(sizeof(OFFSCREEN_ENTRY) * offscreen->maxEntries);
	}

	return offscreen;
}

void offscreen_free(rdpOffscreen* offscreen)
{
	if (offscreen != NULL)
	{
		xfree(offscreen->entries);
		xfree(offscreen);
	}
}
