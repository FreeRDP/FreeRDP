/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP SDL touch/mouse input
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include <winpr/wtypes.h>

#include <SDL3/SDL.h>
#include "sdl_types.hpp"

class SdlTouch
{
  public:
	[[nodiscard]] static bool handleEvent(SdlContext* sdl, const SDL_MouseMotionEvent& ev);
	[[nodiscard]] static bool handleEvent(SdlContext* sdl, const SDL_MouseWheelEvent& ev);
	[[nodiscard]] static bool handleEvent(SdlContext* sdl, const SDL_MouseButtonEvent& ev);

	[[nodiscard]] static bool handleEvent(SdlContext* sdl, const SDL_TouchFingerEvent& ev);

  private:
	[[nodiscard]] static bool touchDown(SdlContext* sdl, const SDL_TouchFingerEvent& ev);
	[[nodiscard]] static bool touchUp(SdlContext* sdl, const SDL_TouchFingerEvent& ev);
	[[nodiscard]] static bool touchCancel(SdlContext* sdl, const SDL_TouchFingerEvent& ev);
	[[nodiscard]] static bool touchMotion(SdlContext* sdl, const SDL_TouchFingerEvent& ev);
};
