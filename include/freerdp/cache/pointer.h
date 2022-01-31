/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_POINTER_CACHE_H
#define FREERDP_POINTER_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <freerdp/graphics.h>

#include <winpr/stream.h>

typedef struct rdp_pointer_cache rdpPointerCache;

#include <freerdp/cache/cache.h>

struct rdp_pointer_cache
{
	UINT32 cacheSize;     /* 0 */
	rdpPointer** entries; /* 1 */

	/* internal */
	rdpContext* context;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API void pointer_cache_register_callbacks(rdpUpdate* update);

	FREERDP_API rdpPointerCache* pointer_cache_new(rdpContext* context);
	FREERDP_API void pointer_cache_free(rdpPointerCache* pointer_cache);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_POINTER_CACHE_H */
