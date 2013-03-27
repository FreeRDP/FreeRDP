/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Window Icon Cache
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

#ifndef FREERDP_RAIL_ICON_CACHE_H
#define FREERDP_RAIL_ICON_CACHE_H

#include <freerdp/api.h>
#include <freerdp/rail.h>
#include <freerdp/types.h>
#include <freerdp/update.h>

#include <winpr/stream.h>

typedef struct rdp_icon rdpIcon;
typedef struct rdp_icon_cache rdpIconCache;

#include <freerdp/rail/rail.h>

struct rdp_icon
{
	ICON_INFO* entry;
	BOOL big;
	void* extra;
};

struct _WINDOW_ICON_CACHE
{
	rdpIcon* entries;
};
typedef struct _WINDOW_ICON_CACHE WINDOW_ICON_CACHE;

struct rdp_icon_cache
{
	rdpRail* rail;
	BYTE numCaches;
	UINT16 numCacheEntries;
	WINDOW_ICON_CACHE* caches;
};

ICON_INFO* icon_cache_get(rdpIconCache* cache, BYTE id, UINT16 index, void** extra);
void icon_cache_put(rdpIconCache* cache, BYTE id, UINT16 index, ICON_INFO* entry, void* extra);

rdpIconCache* icon_cache_new(rdpRail* rail);
void icon_cache_free(rdpIconCache* cache);

#endif /* FREERDP_RAIL_ICON_CACHE_H */
