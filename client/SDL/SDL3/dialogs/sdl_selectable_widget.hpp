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

#include "sdl_widget.hpp"

class SdlSelectableWidget : public SdlWidget
{
  public:
	SdlSelectableWidget(std::shared_ptr<SDL_Renderer>& renderer, const SDL_FRect& rect);
#if defined(WITH_SDL_IMAGE_DIALOGS)
	SdlSelectableWidget(std::shared_ptr<SDL_Renderer>& renderer, const SDL_FRect& rect,
	                    SDL_IOStream* ops);
#endif
	SdlSelectableWidget(SdlSelectableWidget&& other) noexcept;
	SdlSelectableWidget(const SdlSelectableWidget& other) = delete;
	~SdlSelectableWidget() override;

	SdlSelectableWidget& operator=(const SdlSelectableWidget& other) = delete;
	SdlSelectableWidget& operator=(SdlSelectableWidget&& other) = delete;

	bool highlight(bool enable);
	bool mouseover(bool enable);

  protected:
	bool updateInternal() override;
	SDL_Color _highlightcolor = { 0xcd, 0xca, 0x35, 0x60 };
	SDL_Color _mouseovercolor = { 0x66, 0xff, 0x66, 0x60 };

  private:
	bool _mouseover = false;
	bool _highlight = false;
};
