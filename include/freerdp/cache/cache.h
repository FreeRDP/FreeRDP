/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Caches
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

#ifndef __CACHE_H
#define __CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/utils/stream.h>

#include <freerdp/cache/glyph.h>
#include <freerdp/cache/brush.h>
#include <freerdp/cache/pointer.h>
#include <freerdp/cache/bitmap.h>
#include <freerdp/cache/offscreen.h>
#include <freerdp/cache/palette.h>

struct rdp_cache
{
	rdpGlyphCache* glyph; /* 0 */
	rdpBrushCache* brush; /* 1 */
	rdpPointerCache* pointer; /* 2 */
	rdpBitmapCache* bitmap; /* 3 */
	rdpOffscreenCache* offscreen; /* 4 */
	rdpPaletteCache* palette; /* 5 */

	/* internal */

	rdpSettings* settings;
};

FREERDP_API rdpCache* cache_new(rdpSettings* settings);
FREERDP_API void cache_free(rdpCache* cache);

#endif /* __CACHE_H */
