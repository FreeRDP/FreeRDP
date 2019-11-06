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

FREERDP_LOCAL BITMAP_UPDATE* copy_bitmap_update(rdpContext* context, const BITMAP_UPDATE* pointer);
FREERDP_LOCAL void free_bitmap_update(rdpContext* context, BITMAP_UPDATE* pointer);

FREERDP_LOCAL CACHE_BITMAP_ORDER* copy_cache_bitmap_order(rdpContext* context,
                                                          const CACHE_BITMAP_ORDER* order);
FREERDP_LOCAL void free_cache_bitmap_order(rdpContext* context, CACHE_BITMAP_ORDER* order);

FREERDP_LOCAL CACHE_BITMAP_V2_ORDER* copy_cache_bitmap_v2_order(rdpContext* context,
                                                                const CACHE_BITMAP_V2_ORDER* order);
FREERDP_LOCAL void free_cache_bitmap_v2_order(rdpContext* context, CACHE_BITMAP_V2_ORDER* order);

FREERDP_LOCAL CACHE_BITMAP_V3_ORDER* copy_cache_bitmap_v3_order(rdpContext* context,
                                                                const CACHE_BITMAP_V3_ORDER* order);
FREERDP_LOCAL void free_cache_bitmap_v3_order(rdpContext* context, CACHE_BITMAP_V3_ORDER* order);

#endif /* FREERDP_LIB_CACHE_BITMAP_H */
