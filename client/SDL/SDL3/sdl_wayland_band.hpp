/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native Wayland RAIL resize band (wl_subsurface ring)
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
#include <cstdint>

/*
 * Native Wayland outside resize band for RAIL windows, WITHOUT the SDL border-inset upstream
 * patch. A self-managed wl_subsurface ring is placed below the SDL surface over the server's
 * invisible resize margins; it takes the pointer input the SDL window (sized to the visible rect)
 * cannot, and starts a compositor-interactive xdg_toplevel_resize on press.
 *
 * SWAP SEAM: the rest of the RAIL client only calls sdl_wayland_band_sync / _remove. When SDL ships
 * the border-inset API upstream, replace THIS FILE's implementation with one that sets the four
 * SDL.window.wayland.border_inset_* properties (see feat/sdl3-rail-inset for reference) and drops
 * the subsurface machinery - callers stay unchanged, so the diff is confined here.
 *
 * Self-contained: binds its own wl_registry/seat/pointer/subcompositor/shm (does not share the
 * move code's plumbing in sdl_wayland.cpp), so this module is a clean drop-in/drop-out unit.
 * Main thread only; every function is a safe no-op off the Wayland driver / without wayland-client.
 */

/* Create/resize the window's outside resize band: a wl_subsurface ring over the server's invisible
 * resize margins (left/top/right/bottom in px, all-zero removes it). Returns true when the band
 * changed (the caller should repaint so the transparent ring shows). A press on the ring starts the
 * compositor resize and pushes SDL_EVENT_USER_RAIL_BAND(windowID, xdgEdge); code 0 = a pointer
 * enter used to complete a finished resize grab. */
bool sdl_wayland_band_sync(SDL_Window* window, int left, int top, int right, int bottom);

/* Destroy the window's band (call before the SDL window is destroyed). */
void sdl_wayland_band_remove(SDL_Window* window);
