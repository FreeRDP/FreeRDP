/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Bitmap Cache V2
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

#include "bitmap_v2.h"

void* bitmap_v2_get(rdpBitmapV2* bitmap_v2, uint8 id, uint16 index)
{
	void* entry;

	if (index > bitmap_v2->maxCells)
	{
		printf("invalid bitmap_v2 cell id: %d\n", id);
		return NULL;
	}

	if (index > bitmap_v2->cells[id].number)
	{
		printf("invalid bitmap_v2 index %d in cell id: %d\n", index, id);
		return NULL;
	}

	entry = bitmap_v2->cells[id].entries[index].entry;

	if (entry == NULL)
	{
		printf("invalid bitmap_v2 at index %d in cell id: %d\n", index, id);
		return NULL;
	}

	return entry;
}

void bitmap_v2_put(rdpBitmapV2* bitmap_v2, uint8 id, uint16 index, void* entry)
{
	if (id > bitmap_v2->maxCells)
	{
		printf("invalid bitmap_v2 cell id: %d\n", id);
		return;
	}

	if (index > bitmap_v2->cells[id].number)
	{
		printf("invalid bitmap_v2 index %d in cell id: %d\n", index, id);
		return;
	}

	bitmap_v2->cells[id].entries[index].entry = entry;
}

rdpBitmapV2* bitmap_v2_new(rdpSettings* settings)
{
	int i;
	rdpBitmapV2* bitmap_v2;

	bitmap_v2 = (rdpBitmapV2*) xzalloc(sizeof(rdpBitmapV2));

	if (bitmap_v2 != NULL)
	{
		bitmap_v2->settings = settings;

		bitmap_v2->maxCells = 5;

		settings->bitmap_cache = True;
		settings->bitmapCacheV2NumCells = 5;
		settings->bitmapCacheV2CellInfo[0].numEntries = 600;
		settings->bitmapCacheV2CellInfo[0].persistent = False;
		settings->bitmapCacheV2CellInfo[1].numEntries = 600;
		settings->bitmapCacheV2CellInfo[1].persistent = False;
		settings->bitmapCacheV2CellInfo[2].numEntries = 2048;
		settings->bitmapCacheV2CellInfo[2].persistent = False;
		settings->bitmapCacheV2CellInfo[3].numEntries = 4096;
		settings->bitmapCacheV2CellInfo[3].persistent = False;
		settings->bitmapCacheV2CellInfo[4].numEntries = 2048;
		settings->bitmapCacheV2CellInfo[4].persistent = False;

		bitmap_v2->cells = (BITMAP_V2_CELL*) xzalloc(sizeof(BITMAP_V2_CELL) * bitmap_v2->maxCells);

		for (i = 0; i < bitmap_v2->maxCells; i++)
		{
			bitmap_v2->cells[i].number = settings->bitmapCacheV2CellInfo[i].numEntries;
			bitmap_v2->cells[i].entries = (BITMAP_V2_ENTRY*) xzalloc(sizeof(BITMAP_V2_ENTRY) * bitmap_v2->cells[i].number);
		}
	}

	return bitmap_v2;
}

void bitmap_v2_free(rdpBitmapV2* bitmap_v2)
{
	int i, j;

	if (bitmap_v2 != NULL)
	{
		for (i = 0; i < bitmap_v2->maxCells; i++)
		{
			for (j = 0; j < bitmap_v2->cells[i].number; j++)
			{
				if (bitmap_v2->cells[i].entries[j].entry != NULL)
					xfree(bitmap_v2->cells[i].entries[j].entry);
			}

			xfree(bitmap_v2->cells[i].entries);
		}

		xfree(bitmap_v2->cells);
		xfree(bitmap_v2);
	}
}
