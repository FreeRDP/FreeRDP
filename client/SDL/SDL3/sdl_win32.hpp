/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native Win32 helpers for RAIL
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

#include <vector>

/* Restack windows without activation. */
bool sdl_win32_restack_windows(const std::vector<SDL_Window*>& topToBottom);

/* A native move (or size, `wmszEdge` = WMSZ_*) loop can run for this window. */
[[nodiscard]] bool sdl_win32_can_move_size(SDL_Window* window, bool move, int wmszEdge);

/* Run native move/size loop (blocking). */
void sdl_win32_run_move_size(SDL_Window* window, bool move, int wmszEdge);

/* Post the left button release the move/size loop consumed. */
bool sdl_win32_send_left_button_release(SDL_Window* window);
