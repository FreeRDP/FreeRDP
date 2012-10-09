/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Memory Pool
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

#ifndef __RFX_POOL_H
#define __RFX_POOL_H

#include <freerdp/codec/rfx.h>

struct _RFX_POOL
{
	int size;
	int count;
	RFX_TILE** tiles;
};
typedef struct _RFX_POOL RFX_POOL;

RFX_POOL* rfx_pool_new();
void rfx_pool_free(RFX_POOL* pool);
void rfx_pool_put_tile(RFX_POOL* pool, RFX_TILE* tile);
RFX_TILE* rfx_pool_get_tile(RFX_POOL* pool);
void rfx_pool_put_tiles(RFX_POOL* pool, RFX_TILE** tiles, int count);
RFX_TILE** rfx_pool_get_tiles(RFX_POOL* pool, int count);

#endif /* __RFX_POOL_H */
