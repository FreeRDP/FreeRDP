/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NineGrid Cache
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_NINE_GRID_CACHE_H
#define FREERDP_LIB_NINE_GRID_CACHE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/update.h>

#include <winpr/stream.h>

typedef struct rdp_nine_grid_cache rdpNineGridCache;

#include "nine_grid.h"

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL void nine_grid_cache_register_callbacks(rdpUpdate* update);

	FREERDP_LOCAL void nine_grid_cache_free(rdpNineGridCache* nine_grid);

	WINPR_ATTR_MALLOC(nine_grid_cache_free, 1)
	FREERDP_LOCAL rdpNineGridCache* nine_grid_cache_new(rdpContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_NINE_GRID_CACHE_H */
