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

#include <SDL.h>
#include "sdl_widget.hpp"

class SdlInputWidget
{
  public:
	enum
	{
		SDL_INPUT_MASK = 1,
		SDL_INPUT_READONLY = 2
	};

  public:
	SdlInputWidget(SDL_Renderer* renderer, const std::string& label, const std::string& initial,
	               Uint32 flags, size_t offset, size_t width, size_t height);
	SdlInputWidget(SdlInputWidget&& other) noexcept;

	bool fill_label(SDL_Renderer* renderer, SDL_Color color);
	bool update_label(SDL_Renderer* renderer);

	bool set_mouseover(SDL_Renderer* renderer, bool mouseOver);
	bool set_highlight(SDL_Renderer* renderer, bool hightlight);
	bool update_input(SDL_Renderer* renderer);
	bool resize_input(size_t size);

	bool set_str(SDL_Renderer* renderer, const std::string& text);
	bool remove_str(SDL_Renderer* renderer, size_t count);
	bool append_str(SDL_Renderer* renderer, const std::string& text);

	const SDL_Rect& input_rect() const;
	std::string value() const;

	bool readonly() const;

  protected:
	bool update_input(SDL_Renderer* renderer, SDL_Color fgclor);

  private:
	SdlInputWidget(const SdlInputWidget& other) = delete;

  private:
	Uint32 _flags;
	std::string _text;
	std::string _text_label;
	SdlWidget _label;
	SdlWidget _input;
	bool _highlight;
	bool _mouseover;
};
