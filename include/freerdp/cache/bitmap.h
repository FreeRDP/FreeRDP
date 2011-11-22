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

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

typedef struct _BITMAP_V2_CELL BITMAP_V2_CELL;
typedef struct rdp_bitmap_cache rdpBitmapCache;

#include <freerdp/cache/cache.h>

struct _BITMAP_V2_CELL
{
	uint32 number;
	rdpBitmap** entries;
};

struct rdp_bitmap_cache
{
	pMemBlt MemBlt; /* 0 */
	pMem3Blt Mem3Blt; /* 1 */
	pCacheBitmap CacheBitmap; /* 2 */
	pCacheBitmapV2 CacheBitmapV2; /* 3 */
	pCacheBitmapV3 CacheBitmapV3; /* 4 */
	pBitmapUpdate BitmapUpdate; /* 5 */
	uint32 paddingA[16 - 6]; /* 6 */

	uint32 maxCells; /* 16 */
	BITMAP_V2_CELL* cells; /* 17 */
	uint32 paddingB[32 - 18]; /* 18 */

	/* internal */

	rdpBitmap* bitmap;
	rdpUpdate* update;
	rdpContext* context;
	rdpSettings* settings;
};

FREERDP_API rdpBitmap* bitmap_cache_get(rdpBitmapCache* bitmap_cache, uint32 id, uint32 index);
FREERDP_API void bitmap_cache_put(rdpBitmapCache* bitmap_cache, uint32 id, uint32 index, rdpBitmap* bitmap);

FREERDP_API void bitmap_cache_register_callbacks(rdpUpdate* update);

FREERDP_API rdpBitmapCache* bitmap_cache_new(rdpSettings* settings);
FREERDP_API void bitmap_cache_free(rdpBitmapCache* bitmap_cache);

#endif /* __BITMAP_V2_CACHE_H */
