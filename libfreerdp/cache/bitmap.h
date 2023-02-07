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

#ifndef FREERDP_LIB_CACHE_BITMAP_H
#define FREERDP_LIB_CACHE_BITMAP_H

#include <freerdp/api.h>
#include <freerdp/update.h>

#include <freerdp/cache/persistent.h>

typedef struct
{
	UINT32 number;
	rdpBitmap** entries;
} BITMAP_V2_CELL;

typedef struct
{
	pMemBlt MemBlt;               /* 0 */
	pMem3Blt Mem3Blt;             /* 1 */
	pCacheBitmap CacheBitmap;     /* 2 */
	pCacheBitmapV2 CacheBitmapV2; /* 3 */
	pCacheBitmapV3 CacheBitmapV3; /* 4 */
	pBitmapUpdate BitmapUpdate;   /* 5 */
	UINT32 paddingA[16 - 6];      /* 6 */

	UINT32 maxCells;          /* 16 */
	BITMAP_V2_CELL* cells;    /* 17 */
	UINT32 paddingB[32 - 18]; /* 18 */

	/* internal */
	rdpContext* context;
	rdpPersistentCache* persistent;
} rdpBitmapCache;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL void bitmap_cache_register_callbacks(rdpUpdate* update);

	FREERDP_LOCAL rdpBitmapCache* bitmap_cache_new(rdpContext* context);
	FREERDP_LOCAL void bitmap_cache_free(rdpBitmapCache* bitmap_cache);

	FREERDP_LOCAL BITMAP_UPDATE* copy_bitmap_update(rdpContext* context,
	                                                const BITMAP_UPDATE* pointer);
	FREERDP_LOCAL void free_bitmap_update(rdpContext* context, BITMAP_UPDATE* pointer);

	FREERDP_LOCAL CACHE_BITMAP_ORDER* copy_cache_bitmap_order(rdpContext* context,
	                                                          const CACHE_BITMAP_ORDER* order);
	FREERDP_LOCAL void free_cache_bitmap_order(rdpContext* context, CACHE_BITMAP_ORDER* order);

	FREERDP_LOCAL CACHE_BITMAP_V2_ORDER*
	copy_cache_bitmap_v2_order(rdpContext* context, const CACHE_BITMAP_V2_ORDER* order);
	FREERDP_LOCAL void free_cache_bitmap_v2_order(rdpContext* context,
	                                              CACHE_BITMAP_V2_ORDER* order);

	FREERDP_LOCAL CACHE_BITMAP_V3_ORDER*
	copy_cache_bitmap_v3_order(rdpContext* context, const CACHE_BITMAP_V3_ORDER* order);
	FREERDP_LOCAL void free_cache_bitmap_v3_order(rdpContext* context,
	                                              CACHE_BITMAP_V3_ORDER* order);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CACHE_BITMAP_H */
