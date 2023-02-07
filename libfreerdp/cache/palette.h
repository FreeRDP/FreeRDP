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

#ifndef FREERDP_LIB_CACHE_PALETTE_H
#define FREERDP_LIB_CACHE_PALETTE_H

#include <freerdp/api.h>
#include <freerdp/update.h>

typedef struct rdp_palette_cache rdpPaletteCache;

typedef struct
{
	void* entry;
} PALETTE_TABLE_ENTRY;

struct rdp_palette_cache
{
	UINT32 maxEntries;            /* 0 */
	PALETTE_TABLE_ENTRY* entries; /* 1 */

	/* internal */

	rdpContext* context;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL void palette_cache_register_callbacks(rdpUpdate* update);

	FREERDP_LOCAL rdpPaletteCache* palette_cache_new(rdpContext* context);
	FREERDP_LOCAL void palette_cache_free(rdpPaletteCache* palette_cache);

	FREERDP_LOCAL PALETTE_UPDATE* copy_palette_update(rdpContext* context,
	                                                  const PALETTE_UPDATE* pointer);
	FREERDP_LOCAL void free_palette_update(rdpContext* context, PALETTE_UPDATE* pointer);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CACHE_PALETTE_H */
