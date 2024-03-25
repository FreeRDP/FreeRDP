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

#include "sdl_rail_icon.hpp"

SdlRailIcon::SdlRailIcon() : _bpp(-1), _width(-1), _height(-1)
{
}

SdlRailIcon::SdlRailIcon(SdlRailIcon&& other) noexcept
    : _bpp(std::move(other._bpp)), _width(std::move(other._width)),
      _height(std::move(other._height)), _bitsMask(std::move(other._bitsMask)),
      _colorTable(std::move(other._colorTable)), _bitsColor(std::move(other._bitsColor))
{
}

bool SdlRailIcon::update(const ICON_INFO* info)
{
	if (!info)
		return false;

	_bpp = info->bpp;
	_width = info->width;
	_height = info->height;

	_bitsMask.clear();
	for (size_t x = 0; x < info->cbBitsMask; x++)
		_bitsMask.push_back(info->bitsMask[x]);

	_colorTable.clear();
	for (size_t x = 0; x < info->cbColorTable; x++)
		_colorTable.push_back(info->colorTable[x]);

	_bitsColor.clear();
	for (size_t x = 0; x < info->cbBitsColor; x++)
		_bitsColor.push_back(info->bitsColor[x]);
	return true;
}
