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

#include <SDL3/SDL.h>
#include "sdl_selectable_widget.hpp"

class SdlSelectWidget : public SdlSelectableWidget
{
  public:
	SdlSelectWidget(std::shared_ptr<SDL_Renderer>& renderer, const std::string& label,
	                SDL_FRect rect);
	SdlSelectWidget(SdlSelectWidget&& other) noexcept;
	SdlSelectWidget(const SdlSelectWidget& other) = delete;
	~SdlSelectWidget() override;

	SdlSelectWidget& operator=(const SdlSelectWidget& other) = delete;
	SdlSelectWidget& operator=(SdlSelectWidget&& other) = delete;
};
