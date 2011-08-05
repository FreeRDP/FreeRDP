/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Caches
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

#ifndef __CACHE_H
#define __CACHE_H

#include "offscreen.h"

#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

typedef struct rdp_cache rdpCache;

struct rdp_cache
{
	rdpSettings* settings;
	rdpOffscreen* offscreen;
};

rdpCache* cache_new(rdpSettings* settings);
void cache_free(rdpCache* cache);

#endif /* __CACHE_H */
