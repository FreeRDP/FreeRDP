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

#ifndef __BITMAP_V2_CACHE_H
#define __BITMAP_V2_CACHE_H

#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

struct _BITMAP_V2_ENTRY
{
	void* entry;
};
typedef struct _BITMAP_V2_ENTRY BITMAP_V2_ENTRY;

struct _BITMAP_V2_CELL
{
	uint32 number;
	BITMAP_V2_ENTRY* entries;
};
typedef struct _BITMAP_V2_CELL BITMAP_V2_CELL;

struct rdp_bitmap_v2
{
	uint8 maxCells;
	rdpSettings* settings;
	BITMAP_V2_CELL* cells;
};
typedef struct rdp_bitmap_v2 rdpBitmapV2;

void* bitmap_v2_get(rdpBitmapV2* bitmap_v2, uint8 id, uint16 index);
void bitmap_v2_put(rdpBitmapV2* bitmap_v2, uint8 id, uint16 index, void* entry);

rdpBitmapV2* bitmap_v2_new(rdpSettings* settings);
void bitmap_v2_free(rdpBitmapV2* bitmap_v2);

#endif /* __BITMAP_V2_CACHE_H */
