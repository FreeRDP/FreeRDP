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

#include <stdint.h>

#include <vector>

#include <freerdp/window.h>

class SdlRailIcon
{
  public:
	SdlRailIcon();
	SdlRailIcon(SdlRailIcon&& other) noexcept;

	bool update(const ICON_INFO* info);

  private:
	SdlRailIcon(const SdlRailIcon& other) = delete;

  private:
	uint32_t _bpp;
	uint32_t _width;
	uint32_t _height;
	std::vector<uint8_t> _bitsMask;
	std::vector<uint8_t> _colorTable;
	std::vector<uint8_t> _bitsColor;
};
