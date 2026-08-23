/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native X11 interactive window move
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

/* Native X11 helpers for RAIL client. */

/* Check if an X11 compositing manager is active. */
[[nodiscard]] bool sdl_x11_has_compositor();

/* Start native interactive window move/resize via _NET_WM_MOVERESIZE. */
bool sdl_x11_begin_move_size(SDL_Window* window, int netDirection);
