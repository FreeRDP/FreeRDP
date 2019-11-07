/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_GLYPH_CACHE_H
#define FREERDP_GLYPH_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>

#include <winpr/wlog.h>
#include <winpr/stream.h>

typedef struct _GLYPH_CACHE GLYPH_CACHE;
typedef struct _FRAGMENT_CACHE_ENTRY FRAGMENT_CACHE_ENTRY;
typedef struct _FRAGMENT_CACHE FRAGMENT_CACHE;
typedef struct rdp_glyph_cache rdpGlyphCache;

#include <freerdp/cache/cache.h>

struct _GLYPH_CACHE
{
	UINT32 number;
	UINT32 maxCellSize;
	rdpGlyph** entries;
};

struct _FRAGMENT_CACHE_ENTRY
{
	void* fragment;
	UINT32 size;
};

struct _FRAGMENT_CACHE
{
	FRAGMENT_CACHE_ENTRY* entries;
};

struct rdp_glyph_cache
{
	FRAGMENT_CACHE fragCache;
	GLYPH_CACHE glyphCache[10];

	wLog* log;
	rdpContext* context;
	rdpSettings* settings;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API void glyph_cache_register_callbacks(rdpUpdate* update);

	FREERDP_API rdpGlyphCache* glyph_cache_new(rdpSettings* settings);
	FREERDP_API void glyph_cache_free(rdpGlyphCache* glyph);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GLYPH_CACHE_H */
