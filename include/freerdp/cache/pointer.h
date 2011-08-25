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
#include <freerdp/utils/stream.h>

struct _POINTER_CACHE_ENTRY
{
	void* entry;
	void* extra;
};
typedef struct _POINTER_CACHE_ENTRY POINTER_CACHE_ENTRY;

struct rdp_pointer
{
	uint16 cacheSize;
	rdpSettings* settings;
	POINTER_CACHE_ENTRY* entries;
};
typedef struct rdp_pointer rdpPointer;

FREERDP_API void* pointer_get(rdpPointer* pointer, uint16 index, void** extra);
FREERDP_API void pointer_put(rdpPointer* pointer, uint16 index, void* entry, void* extra);

FREERDP_API rdpPointer* pointer_new(rdpSettings* settings);
FREERDP_API void pointer_free(rdpPointer* pointer);

#endif /* __POINTER_CACHE_H */
