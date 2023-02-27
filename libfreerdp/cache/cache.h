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

#ifndef FREERDP_LIB_CACHE_CACHE_H
#define FREERDP_LIB_CACHE_CACHE_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/pointer.h>

#include "glyph.h"
#include "brush.h"
#include "pointer.h"
#include "bitmap.h"
#include "nine_grid.h"
#include "offscreen.h"
#include "palette.h"

struct rdp_cache
{
	rdpGlyphCache* glyph;         /* 0 */
	rdpBrushCache* brush;         /* 1 */
	rdpPointerCache* pointer;     /* 2 */
	rdpBitmapCache* bitmap;       /* 3 */
	rdpOffscreenCache* offscreen; /* 4 */
	rdpPaletteCache* palette;     /* 5 */
	rdpNineGridCache* nine_grid;  /* 6 */
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL rdpCache* cache_new(rdpContext* context);
	FREERDP_LOCAL void cache_free(rdpCache* cache);

	FREERDP_LOCAL CACHE_COLOR_TABLE_ORDER*
	copy_cache_color_table_order(rdpContext* context, const CACHE_COLOR_TABLE_ORDER* order);
	FREERDP_LOCAL void free_cache_color_table_order(rdpContext* context,
	                                                CACHE_COLOR_TABLE_ORDER* order);

	FREERDP_LOCAL SURFACE_BITS_COMMAND*
	copy_surface_bits_command(rdpContext* context, const SURFACE_BITS_COMMAND* order);
	FREERDP_LOCAL void free_surface_bits_command(rdpContext* context, SURFACE_BITS_COMMAND* order);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CACHE_CACHE_H */
