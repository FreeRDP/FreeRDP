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

#ifndef __COLOR_TABLE_CACHE_H
#define __COLOR_TABLE_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

struct _COLOR_TABLE_ENTRY
{
	void* entry;
};
typedef struct _COLOR_TABLE_ENTRY COLOR_TABLE_ENTRY;

struct rdp_color_table
{
	uint8 maxEntries;
	rdpSettings* settings;
	COLOR_TABLE_ENTRY* entries;
};
typedef struct rdp_color_table rdpColorTable;

FREERDP_API void* color_table_get(rdpColorTable* color_table, uint8 index);
FREERDP_API void color_table_put(rdpColorTable* color_table, uint8 index, void* entry);

FREERDP_API rdpColorTable* color_table_new(rdpSettings* settings);
FREERDP_API void color_table_free(rdpColorTable* color_table);

#endif /* __COLOR_TABLE_CACHE_H */
