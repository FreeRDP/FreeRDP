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

#pragma once

#include <cstdlib>
#include <vector>
#include "sdl_rail_icon.hpp"

class SdlRailIconCache
{
  public:
	SdlRailIconCache();
	virtual ~SdlRailIconCache();

	SdlRailIcon* lookup(uint8_t cacheId, uint16_t chacheEntry);

	bool prepare(size_t num_chaches, size_t num_cache_entries);
	void clear();

  private:
	SdlRailIconCache(const SdlRailIconCache& other) = delete;
	SdlRailIconCache(SdlRailIconCache&& other) = delete;

  private:
	size_t _num_caches;
	size_t _num_cache_entries;
	std::vector<SdlRailIcon> _entries;
	SdlRailIcon _scratch;
};
