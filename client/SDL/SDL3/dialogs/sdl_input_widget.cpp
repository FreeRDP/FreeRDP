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
#include "sdl_input_widget.hpp"

SdlInputWidget::SdlInputWidget(std::shared_ptr<SDL_Renderer>& renderer, const SDL_FRect& rect)
    : SdlSelectableWidget(renderer, rect)
{
	init();
}

SdlInputWidget::~SdlInputWidget() = default;

std::string SdlInputWidget::text() const
{
	return _text;
}

void SdlInputWidget::init()
{
	_backgroundcolor = { 0x56, 0x56, 0x56, 0xff };
	_fontcolor = { 0xd1, 0xcf, 0xcd, 0xff };
	_highlightcolor = { 0x80, 0, 0, 0x60 };
	_mouseovercolor = { 0, 0x80, 0, 0x60 };
}

#if defined(WITH_SDL_IMAGE_DIALOGS)
SdlInputWidget::SdlInputWidget(std::shared_ptr<SDL_Renderer>& renderer, const SDL_FRect& rect,
                               SDL_IOStream* ops)
    : SdlSelectableWidget(renderer, rect, ops)
{
	init();
}
#endif

SdlInputWidget::SdlInputWidget(SdlInputWidget&& other) noexcept = default;
