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

#ifndef __BRUSH_CACHE_H
#define __BRUSH_CACHE_H

#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

struct _BRUSH_ENTRY
{
	uint8 bpp;
	void* entry;
};
typedef struct _BRUSH_ENTRY BRUSH_ENTRY;

struct rdp_brush
{
	rdpSettings* settings;
	uint8 maxEntries;
	uint8 maxMonoEntries;
	BRUSH_ENTRY* entries;
	BRUSH_ENTRY* monoEntries;
};
typedef struct rdp_brush rdpBrush;

void* brush_get(rdpBrush* brush, uint8 index, uint8* bpp);
void brush_put(rdpBrush* brush, uint8 index, void* entry, uint8 bpp);

rdpBrush* brush_new(rdpSettings* settings);
void brush_free(rdpBrush* brush);

#endif /* __BRUSH_CACHE_H */
