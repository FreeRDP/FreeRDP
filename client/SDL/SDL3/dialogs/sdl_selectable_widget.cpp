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
#include "sdl_selectable_widget.hpp"
#include "sdl_blend_mode_guard.hpp"

SdlSelectableWidget::SdlSelectableWidget(std::shared_ptr<SDL_Renderer>& renderer,
                                         const SDL_FRect& rect)
    : SdlWidget(renderer, rect)
{
}

#if defined(WITH_SDL_IMAGE_DIALOGS)
SdlSelectableWidget::SdlSelectableWidget(std::shared_ptr<SDL_Renderer>& renderer,
                                         const SDL_FRect& rect, SDL_IOStream* ops)
    : SdlWidget(renderer, rect, ops)
{
}
#endif

SdlSelectableWidget::SdlSelectableWidget(SdlSelectableWidget&& other) noexcept = default;

SdlSelectableWidget::~SdlSelectableWidget() = default;

bool SdlSelectableWidget::highlight(bool enable)
{
	_highlight = enable;
	return update();
}

bool SdlSelectableWidget::mouseover(bool enable)
{
	_mouseover = enable;
	return update();
}

bool SdlSelectableWidget::updateInternal()
{
	SdlBlendModeGuard guard(_renderer, SDL_BLENDMODE_NONE);
	std::vector<SDL_Color> colors;
	colors.push_back(_backgroundcolor);
	if (_highlight)
		colors.push_back(_highlightcolor);

	if (_mouseover)
		colors.push_back(_mouseovercolor);

	if (!fill(colors))
		return false;

	return SdlWidget::updateInternal();
}
