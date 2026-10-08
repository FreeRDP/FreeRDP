/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native macOS display helpers
 *
 * Copyright 2026 Ocean <25319668+oceanzhang88@users.noreply.github.com>
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

#include <SDL3/SDL.h>

/* Points AppKit keeps free at the top of the display with the given SDL bounds when a window
 * goes full screen in its own Space: the menu bar strip on a display with a camera housing
 * (notch), 0 on any other display or before macOS 12. */
[[nodiscard]] int sdl_macos_fullscreen_space_top_inset(const SDL_Rect& bounds);
