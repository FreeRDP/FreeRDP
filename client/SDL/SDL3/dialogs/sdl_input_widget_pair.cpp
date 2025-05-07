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

#include "sdl_input_widget_pair.hpp"

#include <cassert>
#include <algorithm>
#include <string>
#include <utility>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "sdl_widget.hpp"
#include "sdl_button.hpp"
#include "sdl_buttons.hpp"

SdlInputWidgetPair::SdlInputWidgetPair(std::shared_ptr<SDL_Renderer>& renderer,
                                       const std::string& label, const std::string& initial,
                                       Uint32 flags, size_t offset, size_t width, size_t height)
    : _flags(flags), _label(renderer, { 0, static_cast<float>(offset * (height + _vpadding)),
                                        static_cast<float>(width), static_cast<float>(height) }),
      _input(renderer, { static_cast<float>(width + _hpadding),
                         static_cast<float>(offset * (height + _vpadding)),
                         static_cast<float>(width), static_cast<float>(height) })
{
	_label.update_text(label);
	update_input_text(initial);
}

SdlInputWidgetPair::SdlInputWidgetPair(SdlInputWidgetPair&& other) noexcept = default;

bool SdlInputWidgetPair::set_mouseover(bool mouseOver)
{
	if (readonly())
		return true;
	return _input.mouseover(mouseOver);
}

bool SdlInputWidgetPair::set_highlight(bool highlight)
{
	if (readonly())
		return true;
	return _input.highlight(highlight);
}

bool SdlInputWidgetPair::set_str(const std::string& text)
{
	if (readonly())
		return true;
	return update_input_text(text);
}

bool SdlInputWidgetPair::remove_str(size_t count)
{
	if (readonly())
		return true;

	auto text = _text;
	if (text.empty())
		return true;

	auto newsize = text.size() - std::min<size_t>(text.size(), count);
	return update_input_text(text.substr(0, newsize));
}

bool SdlInputWidgetPair::append_str(const std::string& text)
{
	if (readonly())
		return true;

	auto itext = _text;
	itext.append(text);
	return update_input_text(itext);
}

const SDL_FRect& SdlInputWidgetPair::input_rect() const
{
	return _input.rect();
}

std::string SdlInputWidgetPair::value() const
{
	return _text;
}

bool SdlInputWidgetPair::readonly() const
{
	return (_flags & SDL_INPUT_READONLY) != 0;
}

bool SdlInputWidgetPair::update()
{
	// TODO: Draw the pair area
	if (!_label.update())
		return false;
	if (!_input.update())
		return false;
	return true;
}

bool SdlInputWidgetPair::update_input_text(const std::string& txt)
{
	_text = txt;
	auto text = txt;

	if (_flags & SDL_INPUT_MASK)
	{
		std::fill(text.begin(), text.end(), '*');
	}

	return _input.update_text(text);
}
