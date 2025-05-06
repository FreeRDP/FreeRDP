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

#pragma once

#include <vector>
#include <string>

#include <SDL3/SDL.h>
#include "sdl_widget.hpp"
#include "sdl_input_widget.hpp"

class SdlInputWidgetPair
{
  public:
	enum
	{
		SDL_INPUT_MASK = 1,
		SDL_INPUT_READONLY = 2
	};

	SdlInputWidgetPair(std::shared_ptr<SDL_Renderer>& renderer, const std::string& label,
	                   const std::string& initial, Uint32 flags, size_t offset, size_t width,
	                   size_t height);
	SdlInputWidgetPair(SdlInputWidgetPair&& other) noexcept;
	SdlInputWidgetPair(const SdlInputWidgetPair& other) = delete;
	~SdlInputWidgetPair() = default;

	SdlInputWidgetPair& operator=(const SdlInputWidgetPair& other) = delete;
	SdlInputWidgetPair& operator=(SdlInputWidgetPair&& other) = delete;

	bool set_mouseover(bool mouseOver);
	bool set_highlight(bool highlight);

	bool set_str(const std::string& text);
	bool remove_str(size_t count);
	bool append_str(const std::string& text);

	[[nodiscard]] const SDL_FRect& input_rect() const;
	[[nodiscard]] std::string value() const;

	[[nodiscard]] bool readonly() const;

	bool update();

  protected:
	bool update_input_text(const std::string& txt);

	Uint32 _vpadding = 5;
	Uint32 _hpadding = 10;

  private:
	Uint32 _flags{};
	SdlWidget _label;
	SdlInputWidget _input;
	std::string _text;
};
