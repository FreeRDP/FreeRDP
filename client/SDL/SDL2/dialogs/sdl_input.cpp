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

#include "sdl_input.hpp"

#include <cassert>

#include <string>
#include <utility>

#include <SDL.h>
#include <SDL_ttf.h>

#include "sdl_widget.hpp"
#include "sdl_button.hpp"
#include "sdl_buttons.hpp"

static const SDL_Color inputbackgroundcolor = { 0x56, 0x56, 0x56, 0xff };
static const SDL_Color inputhighlightcolor = { 0x80, 0, 0, 0x60 };
static const SDL_Color inputmouseovercolor = { 0, 0x80, 0, 0x60 };
static const SDL_Color inputfontcolor = { 0xd1, 0xcf, 0xcd, 0xff };
static const SDL_Color labelbackgroundcolor = { 0x56, 0x56, 0x56, 0xff };
static const SDL_Color labelfontcolor = { 0xd1, 0xcf, 0xcd, 0xff };
static const Uint32 vpadding = 5;
static const Uint32 hpadding = 10;

SdlInputWidget::SdlInputWidget(SDL_Renderer* renderer, std::string label, std::string initial,
                               Uint32 flags, size_t offset, size_t width, size_t height)
    : _flags(flags), _text(std::move(initial)), _text_label(std::move(label)),
      _label(renderer,
             { 0, static_cast<int>(offset * (height + vpadding)), static_cast<int>(width),
               static_cast<int>(height) },
             false),
      _input(renderer,
             { static_cast<int>(width + hpadding), static_cast<int>(offset * (height + vpadding)),
               static_cast<int>(width), static_cast<int>(height) },
             true),
      _highlight(false), _mouseover(false)
{
}

SdlInputWidget::SdlInputWidget(SdlInputWidget&& other) noexcept
    : _flags(other._flags), _text(std::move(other._text)),
      _text_label(std::move(other._text_label)), _label(std::move(other._label)),
      _input(std::move(other._input)), _highlight(other._highlight), _mouseover(other._mouseover)
{
}

bool SdlInputWidget::fill_label(SDL_Renderer* renderer, SDL_Color color)
{
	if (!_label.fill(renderer, color))
		return false;
	return _label.update_text(renderer, _text_label, labelfontcolor);
}

bool SdlInputWidget::update_label(SDL_Renderer* renderer)
{
	return _label.update_text(renderer, _text_label, labelfontcolor, labelbackgroundcolor);
}

bool SdlInputWidget::set_mouseover(SDL_Renderer* renderer, bool mouseOver)
{
	if (readonly())
		return true;
	_mouseover = mouseOver;
	return update_input(renderer);
}

bool SdlInputWidget::set_highlight(SDL_Renderer* renderer, bool highlight)
{
	if (readonly())
		return true;
	_highlight = highlight;
	return update_input(renderer);
}

bool SdlInputWidget::update_input(SDL_Renderer* renderer)
{
	std::vector<SDL_Color> colors = { inputbackgroundcolor };
	if (_highlight)
		colors.push_back(inputhighlightcolor);
	if (_mouseover)
		colors.push_back(inputmouseovercolor);

	if (!_input.fill(renderer, colors))
		return false;
	return update_input(renderer, inputfontcolor);
}

bool SdlInputWidget::resize_input(size_t size)
{
	_text.resize(size);

	return true;
}

bool SdlInputWidget::set_str(SDL_Renderer* renderer, const std::string& text)
{
	if (readonly())
		return true;
	_text = text;
	if (!resize_input(_text.size()))
		return false;
	return update_input(renderer);
}

bool SdlInputWidget::remove_str(SDL_Renderer* renderer, size_t count)
{
	if (readonly())
		return true;

	assert(renderer);
	if (_text.empty())
		return true;

	if (!resize_input(_text.size() - count))
		return false;
	return update_input(renderer);
}

bool SdlInputWidget::append_str(SDL_Renderer* renderer, const std::string& text)
{
	assert(renderer);
	if (readonly())
		return true;

	_text.append(text);
	if (!resize_input(_text.size()))
		return false;
	return update_input(renderer);
}

const SDL_Rect& SdlInputWidget::input_rect() const
{
	return _input.rect();
}

std::string SdlInputWidget::value() const
{
	return _text;
}

bool SdlInputWidget::readonly() const
{
	return (_flags & SDL_INPUT_READONLY) != 0;
}

bool SdlInputWidget::update_input(SDL_Renderer* renderer, SDL_Color fgcolor)
{
	std::string text = _text;
	if (!text.empty())
	{
		if (_flags & SDL_INPUT_MASK)
		{
			for (char& x : text)
				x = '*';
		}
	}

	return _input.update_text(renderer, text, fgcolor);
}
