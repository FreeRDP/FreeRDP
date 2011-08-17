/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Offscreen Bitmap Cache
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

#ifndef __OFFSCREEN_CACHE_H
#define __OFFSCREEN_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

struct _OFFSCREEN_ENTRY
{
	void* bitmap;
};
typedef struct _OFFSCREEN_ENTRY OFFSCREEN_ENTRY;

struct rdp_offscreen
{
	uint16 maxSize;
	uint16 maxEntries;
	rdpSettings* settings;
	OFFSCREEN_ENTRY* entries;
};
typedef struct rdp_offscreen rdpOffscreen;

FREERDP_API void* offscreen_get(rdpOffscreen* offscreen, uint16 index);
FREERDP_API void offscreen_put(rdpOffscreen* offscreen, uint16 index, void* bitmap);

FREERDP_API rdpOffscreen* offscreen_new(rdpSettings* settings);
FREERDP_API void offscreen_free(rdpOffscreen* offscreen);

#endif /* __OFFSCREEN_CACHE_H */
