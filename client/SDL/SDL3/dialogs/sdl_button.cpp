/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client Channels
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#include <cassert>
#include <utility>

#include "sdl_button.hpp"

SdlButton::SdlButton(std::shared_ptr<SDL_Renderer>& renderer, const std::string& label, int id,
                     const SDL_FRect& rect)
    : SdlSelectableWidget(renderer, rect), _id(id)
{
	_backgroundcolor = { 0x69, 0x66, 0x63, 0xff };
	_highlightcolor = { 0xcd, 0xca, 0x35, 0x60 };
	_mouseovercolor = { 0x66, 0xff, 0x66, 0x60 };
	_fontcolor = { 0xd1, 0xcf, 0xcd, 0xff };
	(void)update_text(label);
	(void)update();
}

SdlButton::SdlButton(SdlButton&& other) noexcept = default;

SdlButton::~SdlButton() = default;

int SdlButton::id() const
{
	return _id;
}
