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

#include <vector>

/* Native X11 helpers for the RAIL client (compiled only WITH_SDL_X11). Main thread only; each
 * function is a safe no-op off the X11 backend. */

/* True if a compositing manager owns _NET_WM_CM_S<n> (i.e. transparent windows will be blended
 * instead of showing a black background). Opens a short-lived X connection; call rarely. Returns
 * true off X11 (nothing to detect). */
[[nodiscard]] bool sdl_x11_has_compositor();

/* Hand an interactive move/resize to the WM via _NET_WM_MOVERESIZE (WMs let their own interactive
 * ops overhang the screen where app-initiated ones get clamped). `netDirection` is
 * _NET_WM_MOVERESIZE (0..7 = edges, 8 = move). */
bool sdl_x11_begin_move_size(SDL_Window* window, int netDirection);

/* Restack top-level windows to the given top-to-bottom order via _NET_RESTACK_WINDOW (WM-mediated,
 * so reparenting-safe; unlike raising, it does NOT change input focus). No-op off X11 or with fewer
 * than two windows. Returns true if the restack messages were sent. */
bool sdl_x11_restack_windows(const std::vector<SDL_Window*>& topToBottom);

/* Declare invisible window edges via _GTK_FRAME_EXTENTS (the GTK CSD-shadow convention): the WM
 * snaps/tiles/maximizes the window minus these extents, so the invisible resize band overlaps
 * neighbours instead of leaving gaps. Zero extents clear the declaration. */
bool sdl_x11_set_frame_extents(SDL_Window* window, int left, int right, int top, int bottom);

/* Pin the old window contents to an X gravity corner (NorthWestGravity=1 ... SouthEastGravity=9,
 * 0 = forget) while the WM resizes; keeps the anchored edge steady during opaque drags. */
bool sdl_x11_set_bit_gravity(SDL_Window* window, int gravity);
