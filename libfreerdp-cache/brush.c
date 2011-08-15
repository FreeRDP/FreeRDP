/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Brush Cache
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

#include <freerdp/cache/brush.h>

void* brush_get(rdpBrush* brush, uint8 index, uint8* bpp)
{
	void* entry;

	if (*bpp == 1)
	{
		if (index > brush->maxMonoEntries)
		{
			printf("invalid brush (%d bpp) index: 0x%04X\n", *bpp, index);
			return NULL;
		}

		*bpp = brush->monoEntries[index].bpp;
		entry = brush->monoEntries[index].entry;
	}
	else
	{
		if (index > brush->maxEntries)
		{
			printf("invalid brush (%d bpp) index: 0x%04X\n", *bpp, index);
			return NULL;
		}

		*bpp = brush->entries[index].bpp;
		entry = brush->entries[index].entry;
	}

	if (entry == NULL)
	{
		printf("invalid brush (%d bpp) at index: 0x%04X\n", *bpp, index);
		return NULL;
	}

	return entry;
}

void brush_put(rdpBrush* brush, uint8 index, void* entry, uint8 bpp)
{
	if (bpp == 1)
	{
		if (index > brush->maxMonoEntries)
		{
			printf("invalid brush (%d bpp) index: 0x%04X\n", bpp, index);
			return;
		}

		brush->monoEntries[index].bpp = bpp;
		brush->monoEntries[index].entry = entry;
	}
	else
	{
		if (index > brush->maxEntries)
		{
			printf("invalid brush (%d bpp) index: 0x%04X\n", bpp, index);
			return;
		}

		brush->entries[index].bpp = bpp;
		brush->entries[index].entry = entry;
	}
}

rdpBrush* brush_new(rdpSettings* settings)
{
	rdpBrush* brush;

	brush = (rdpBrush*) xzalloc(sizeof(rdpBrush));

	if (brush != NULL)
	{
		brush->settings = settings;

		brush->maxEntries = 64;
		brush->maxMonoEntries = 64;

		brush->entries = (BRUSH_ENTRY*) xzalloc(sizeof(BRUSH_ENTRY) * brush->maxEntries);
		brush->monoEntries = (BRUSH_ENTRY*) xzalloc(sizeof(BRUSH_ENTRY) * brush->maxMonoEntries);
	}

	return brush;
}

void brush_free(rdpBrush* brush)
{
	if (brush != NULL)
	{
		xfree(brush);
	}
}
