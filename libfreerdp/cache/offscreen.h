/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_LIB_OFFSCREEN_CACHE_H
#define FREERDP_LIB_OFFSCREEN_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>

#include <winpr/stream.h>

typedef struct rdp_offscreen_cache rdpOffscreenCache;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL rdpBitmap* offscreen_cache_get(rdpOffscreenCache* offscreen_cache, UINT32 index);

	FREERDP_LOCAL void offscreen_cache_register_callbacks(rdpUpdate* update);

	FREERDP_LOCAL void offscreen_cache_free(rdpOffscreenCache* offscreen);

	WINPR_ATTR_MALLOC(offscreen_cache_free, 1)
	FREERDP_LOCAL rdpOffscreenCache* offscreen_cache_new(rdpContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_OFFSCREEN_CACHE_H */
