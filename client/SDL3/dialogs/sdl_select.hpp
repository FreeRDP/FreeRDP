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

#include <string>
#include <vector>

#include <SDL.h>
#include "sdl_widget.hpp"

class SdlSelectWidget : public SdlWidget
{
  public:
	SdlSelectWidget(SDL_Renderer* renderer, const std::string& label, const SDL_Rect& rect);
	SdlSelectWidget(SdlSelectWidget&& other) noexcept;
	~SdlSelectWidget() override = default;

	bool set_mouseover(SDL_Renderer* renderer, bool mouseOver);
	bool set_highlight(SDL_Renderer* renderer, bool highlight);
	bool update_text(SDL_Renderer* renderer);

  private:
	SdlSelectWidget(const SdlSelectWidget& other) = delete;

  private:
	std::string _text;
	bool _mouseover;
	bool _highlight;
};
