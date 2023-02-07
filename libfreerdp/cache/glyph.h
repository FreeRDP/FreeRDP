/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_CACHE_GLYPH_H
#define FREERDP_LIB_CACHE_GLYPH_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/pointer.h>

typedef struct
{
	UINT32 number;
	UINT32 maxCellSize;
	rdpGlyph** entries;
} GLYPH_CACHE;

typedef struct
{
	void* fragment;
	UINT32 size;
} FRAGMENT_CACHE_ENTRY;

typedef struct
{
	FRAGMENT_CACHE_ENTRY entries[256];
} FRAGMENT_CACHE;

typedef struct
{
	FRAGMENT_CACHE fragCache;
	GLYPH_CACHE glyphCache[10];

	wLog* log;
	rdpContext* context;
} rdpGlyphCache;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL void glyph_cache_register_callbacks(rdpUpdate* update);

	FREERDP_LOCAL rdpGlyphCache* glyph_cache_new(rdpContext* context);
	FREERDP_LOCAL void glyph_cache_free(rdpGlyphCache* glyph);

	FREERDP_LOCAL CACHE_GLYPH_ORDER* copy_cache_glyph_order(rdpContext* context,
	                                                        const CACHE_GLYPH_ORDER* glyph);
	FREERDP_LOCAL void free_cache_glyph_order(rdpContext* context, CACHE_GLYPH_ORDER* glyph);

	FREERDP_LOCAL CACHE_GLYPH_V2_ORDER*
	copy_cache_glyph_v2_order(rdpContext* context, const CACHE_GLYPH_V2_ORDER* glyph);
	FREERDP_LOCAL void free_cache_glyph_v2_order(rdpContext* context, CACHE_GLYPH_V2_ORDER* glyph);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CACHE_GLYPH_H */
