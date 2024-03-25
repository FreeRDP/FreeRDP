/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL RAIL
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include "sdl_rail_icon_cache.hpp"

SdlRailIconCache::SdlRailIconCache() : _num_caches(0), _num_cache_entries(0)
{
}

SdlRailIconCache::~SdlRailIconCache()
{
}

SdlRailIcon* SdlRailIconCache::lookup(uint8_t cacheId, uint16_t cacheEntry)
{
	/*
	 * MS-RDPERP 2.2.1.2.3 Icon Info (TS_ICON_INFO)
	 *
	 * CacheId (1 byte):
	 *     If the value is 0xFFFF, the icon SHOULD NOT be cached.
	 *
	 * Yes, the spec says "0xFFFF" in the 2018-03-16 revision,
	 * but the actual protocol field is 1-byte wide.
	 */
	if (cacheId == 0xFF)
		return &_scratch;

	if (cacheId >= _num_caches)
		return nullptr;

	if (cacheEntry >= _num_cache_entries)
		return nullptr;

	return &_entries[_num_cache_entries * cacheId + cacheEntry];
}

bool SdlRailIconCache::prepare(size_t num_caches, size_t num_cache_entries)
{
	clear();
	_num_caches = num_caches;
	_num_cache_entries = num_cache_entries;

	_entries.resize(_num_caches * _num_cache_entries);
	return true;
}

void SdlRailIconCache::clear()
{
	_entries.clear();
	_num_caches = 0;
	_num_cache_entries = 0;
}
