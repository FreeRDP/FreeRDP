/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#include <string>
#include <utility>

#include <SDL.h>
#include <SDL_ttf.h>

#include "sdl_select.hpp"
#include "sdl_widget.hpp"
#include "sdl_button.hpp"
#include "sdl_buttons.hpp"
#include "sdl_input_widgets.hpp"

static const SDL_Color labelmouseovercolor = { 0, 0x80, 0, 0x60 };
static const SDL_Color labelbackgroundcolor = { 0x69, 0x66, 0x63, 0xff };
static const SDL_Color labelhighlightcolor = { 0xcd, 0xca, 0x35, 0x60 };
static const SDL_Color labelfontcolor = { 0xd1, 0xcf, 0xcd, 0xff };

SdlSelectWidget::SdlSelectWidget(SDL_Renderer* renderer, std::string label, SDL_Rect rect)
    : SdlWidget(renderer, rect, true), _text(std::move(label)), _mouseover(false), _highlight(false)
{
	update_text(renderer);
}

SdlSelectWidget::SdlSelectWidget(SdlSelectWidget&& other) noexcept
    : SdlWidget(std::move(other)), _text(std::move(other._text)), _mouseover(other._mouseover),
      _highlight(other._highlight)
{
}

SdlSelectWidget::~SdlSelectWidget() = default;

bool SdlSelectWidget::set_mouseover(SDL_Renderer* renderer, bool mouseOver)
{
	_mouseover = mouseOver;
	return update_text(renderer);
}

bool SdlSelectWidget::set_highlight(SDL_Renderer* renderer, bool highlight)
{
	_highlight = highlight;
	return update_text(renderer);
}

bool SdlSelectWidget::update_text(SDL_Renderer* renderer)
{
	assert(renderer);
	std::vector<SDL_Color> colors = { labelbackgroundcolor };
	if (_highlight)
		colors.push_back(labelhighlightcolor);
	if (_mouseover)
		colors.push_back(labelmouseovercolor);
	if (!fill(renderer, colors))
		return false;
	return SdlWidget::update_text(renderer, _text, labelfontcolor);
}
