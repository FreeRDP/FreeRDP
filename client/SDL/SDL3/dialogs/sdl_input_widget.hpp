/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include "sdl_selectable_widget.hpp"

class SdlInputWidget : public SdlSelectableWidget
{
  public:
	SdlInputWidget(std::shared_ptr<SDL_Renderer>& renderer, const SDL_FRect& rect);
	SdlInputWidget(std::shared_ptr<SDL_Renderer>& renderer, const SDL_FRect& rect,
	               SDL_IOStream* ops);

	SdlInputWidget(SdlInputWidget&& other) noexcept;
	SdlInputWidget(const SdlInputWidget& other) = delete;

	SdlInputWidget& operator=(const SdlInputWidget& other) = delete;
	SdlInputWidget& operator=(SdlInputWidget&& other) noexcept = delete;

	~SdlInputWidget() override;

	std::string text() const;

  private:
	void init();
};
