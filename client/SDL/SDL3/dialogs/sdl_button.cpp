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

static const SDL_Color buttonbackgroundcolor = { 0x69, 0x66, 0x63, 0xff };
static const SDL_Color buttonhighlightcolor = { 0xcd, 0xca, 0x35, 0x60 };
static const SDL_Color buttonmouseovercolor = { 0x66, 0xff, 0x66, 0x60 };
static const SDL_Color buttonfontcolor = { 0xd1, 0xcf, 0xcd, 0xff };

SdlButton::SdlButton(SDL_Renderer* renderer, std::string label, int id, const SDL_FRect& rect)
    : SdlWidget(renderer, rect, false), _name(std::move(label)), _id(id)
{
	assert(renderer);

	update_text(renderer, _name, buttonfontcolor, buttonbackgroundcolor);
}

SdlButton::SdlButton(SdlButton&& other) noexcept
    : SdlWidget(std::move(other)), _name(std::move(other._name)), _id(other._id)
{
}

SdlButton::~SdlButton() = default;

bool SdlButton::highlight(SDL_Renderer* renderer)
{
	assert(renderer);

	std::vector<SDL_Color> colors = { buttonbackgroundcolor, buttonhighlightcolor };
	if (!fill(renderer, colors))
		return false;
	return update_text(renderer, _name, buttonfontcolor);
}

bool SdlButton::mouseover(SDL_Renderer* renderer)
{
	std::vector<SDL_Color> colors = { buttonbackgroundcolor, buttonmouseovercolor };
	if (!fill(renderer, colors))
		return false;
	return update_text(renderer, _name, buttonfontcolor);
}

bool SdlButton::update(SDL_Renderer* renderer)
{
	assert(renderer);

	return update_text(renderer, _name, buttonfontcolor, buttonbackgroundcolor);
}

int SdlButton::id() const
{
	return _id;
}
