/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Color Table Cache
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

#include "color_table.h"

void* color_table_get(rdpColorTable* color_table, uint8 index)
{
	void* entry;

	if (index > color_table->maxEntries)
	{
		printf("invalid color table index: 0x%04X\n", index);
		return NULL;
	}

	entry = color_table->entries[index].entry;

	if (entry == NULL)
	{
		printf("invalid color table at index: 0x%04X\n", index);
		return NULL;
	}

	return entry;
}

void color_table_put(rdpColorTable* color_table, uint8 index, void* entry)
{
	if (index > color_table->maxEntries)
	{
		printf("invalid color table index: 0x%04X\n", index);
		return;
	}

	color_table->entries[index].entry = entry;
}

rdpColorTable* color_table_new(rdpSettings* settings)
{
	rdpColorTable* color_table;

	color_table = (rdpColorTable*) xzalloc(sizeof(rdpColorTable));

	if (color_table != NULL)
	{
		color_table->settings = settings;

		color_table->maxEntries = 6;

		color_table->entries = (COLOR_TABLE_ENTRY*) xzalloc(sizeof(COLOR_TABLE_ENTRY) * color_table->maxEntries);
	}

	return color_table;
}

void color_table_free(rdpColorTable* color_table)
{
	if (color_table != NULL)
	{
		xfree(color_table);
	}
}

