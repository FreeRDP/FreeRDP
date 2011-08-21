/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Glyph Cache
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

#ifndef __GLYPH_CACHE_H
#define __GLYPH_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

struct _GLYPH_CACHE_ENTRY
{
	void* entry;
};
typedef struct _GLYPH_CACHE_ENTRY GLYPH_CACHE_ENTRY;

struct _GLYPH_CACHE
{
	uint16 number;
	uint16 maxCellSize;
	GLYPH_CACHE_ENTRY* entries;
};
typedef struct _GLYPH_CACHE GLYPH_CACHE;

struct rdp_glyph
{
	rdpSettings* settings;
	GLYPH_CACHE glyphCache[10];
	GLYPH_CACHE fragCache;
};
typedef struct rdp_glyph rdpGlyph;

FREERDP_API void* glyph_get(rdpGlyph* glyph, uint8 id, uint16 index);
FREERDP_API void glyph_put(rdpGlyph* glyph, uint8 id, uint16 index, void* entry);

FREERDP_API rdpGlyph* glyph_new(rdpSettings* settings);
FREERDP_API void glyph_free(rdpGlyph* glyph);

#endif /* __GLYPH_CACHE_H */
