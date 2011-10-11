/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Pointer Cache
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

#ifndef __POINTER_CACHE_H
#define __POINTER_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

typedef struct rdp_pointer rdpPointer;
typedef struct rdp_pointer_cache rdpPointerCache;

#include <freerdp/cache/cache.h>

struct rdp_pointer
{
	uint16 xPos;
	uint16 yPos;
	uint16 width;
	uint16 height;
	uint16 xorBpp;
	uint16 lengthAndMask;
	uint16 lengthXorMask;
	uint8* xorMaskData;
	uint8* andMaskData;
};

typedef void (*cbPointerSize)(rdpUpdate* update, uint32* size);
typedef void (*cbPointerSet)(rdpUpdate* update, rdpPointer* pointer);
typedef void (*cbPointerNew)(rdpUpdate* update, rdpPointer* pointer);
typedef void (*cbPointerFree)(rdpUpdate* update, rdpPointer* pointer);

struct rdp_pointer_cache
{
	cbPointerSize PointerSize;
	cbPointerNew PointerSet;
	cbPointerNew PointerNew;
	cbPointerFree PointerFree;

	uint16 cacheSize;
	rdpUpdate* update;
	rdpSettings* settings;
	rdpPointer** entries;
};

FREERDP_API rdpPointer* pointer_cache_get(rdpPointerCache* pointer_cache, uint16 index);
FREERDP_API void pointer_cache_put(rdpPointerCache* pointer_cache, uint16 index, rdpPointer* pointer);

FREERDP_API void pointer_cache_register_callbacks(rdpUpdate* update);

FREERDP_API rdpPointerCache* pointer_cache_new(rdpSettings* settings);
FREERDP_API void pointer_cache_free(rdpPointerCache* pointer_cache);

#endif /* __POINTER_CACHE_H */
