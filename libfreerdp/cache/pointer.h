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

#ifndef FREERDP_LIB_CACHE_POINTER_H
#define FREERDP_LIB_CACHE_POINTER_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/pointer.h>

FREERDP_LOCAL POINTER_COLOR_UPDATE* copy_pointer_color_update(rdpContext* context,
                                                              const POINTER_COLOR_UPDATE* pointer);
FREERDP_LOCAL void free_pointer_color_update(rdpContext* context, POINTER_COLOR_UPDATE* pointer);

FREERDP_LOCAL POINTER_LARGE_UPDATE* copy_pointer_large_update(rdpContext* context,
                                                              const POINTER_LARGE_UPDATE* pointer);
FREERDP_LOCAL void free_pointer_large_update(rdpContext* context, POINTER_LARGE_UPDATE* pointer);

FREERDP_LOCAL POINTER_NEW_UPDATE* copy_pointer_new_update(rdpContext* context,
                                                          const POINTER_NEW_UPDATE* pointer);
FREERDP_LOCAL void free_pointer_new_update(rdpContext* context, POINTER_NEW_UPDATE* pointer);

FREERDP_LOCAL POINTER_CACHED_UPDATE*
copy_pointer_cached_update(rdpContext* context, const POINTER_CACHED_UPDATE* pointer);
FREERDP_LOCAL void free_pointer_cached_update(rdpContext* context, POINTER_CACHED_UPDATE* pointer);

FREERDP_LOCAL POINTER_POSITION_UPDATE*
copy_pointer_position_update(rdpContext* context, const POINTER_POSITION_UPDATE* pointer);
FREERDP_LOCAL void free_pointer_position_update(rdpContext* context,
                                                POINTER_POSITION_UPDATE* pointer);

FREERDP_LOCAL POINTER_SYSTEM_UPDATE*
copy_pointer_system_update(rdpContext* context, const POINTER_SYSTEM_UPDATE* pointer);
FREERDP_LOCAL void free_pointer_system_update(rdpContext* context, POINTER_SYSTEM_UPDATE* pointer);

#endif /* FREERDP_LIB_CACHE_POINTER_H */
