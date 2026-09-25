/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client RAIL platform: capabilities and native backend dispatch
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

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

enum class RailPlatformKind
{
	None,
	X11,
	Wayland,
	Win32
};

/* Window resize band integration mechanism. */
enum class RailBandMechanism
{
	None,
	FrameExtents /* X11 _GTK_FRAME_EXTENTS, published before the resize */
};

/* Drag and resize execution model. */
enum class RailDragModel
{
	None,            /* the server drags the window itself */
	WmMoveResize,    /* X11 _NET_WM_MOVERESIZE, ends with the button release */
	CompositorResize /* Wayland xdg_toplevel move/resize, the client only tracks the size */
};

/**
 * Windowing system capabilities for RAIL platform abstraction.
 */
struct RailPlatformCaps
{
	RailPlatformKind kind = RailPlatformKind::None;
	bool supportsWindowPositions = true;
	bool supportsTransparentWindows = true;
	bool supportsRestack = false;
	RailBandMechanism band = RailBandMechanism::None;
	RailDragModel drag = RailDragModel::None;
};

/* Detected capabilities of the running SDL video backend (cached). */
[[nodiscard]] const RailPlatformCaps& railPlatformCaps();

/* Active drag edges for RAIL_WMSZ_*. */
struct RailEdges
{
	bool left = false;
	bool right = false;
	bool top = false;
	bool bottom = false;
};
[[nodiscard]] RailEdges railEdges(uint16_t moveType);

/* Per-window setup before the first drag (Wayland: bind the seat for the press serial). */
void railPlatformPrepareWindow(SDL_Window* window);

/* Declare the band outside the content (x=L y=T w=R h=B) the way caps.band says. */
bool railPlatformSetBand(SDL_Window* window, const SDL_Rect& insets);

/* Restack windows top-to-bottom without activating them. */
bool railPlatformRestack(const std::vector<SDL_Window*>& topToBottom);

/* Start a local move (RAIL_WMSZ_MOVE) or resize (RAIL_WMSZ_* edge) the way caps.drag says. */
bool railPlatformBeginDrag(SDL_Window* window, uint16_t moveType);

/* The WM kept the left button release of the drag; hand one to SDL. */
bool railPlatformReleaseButton(SDL_Window* window);

/* Put a reattached owned window back above its owner without activating it. */
bool railPlatformRaiseOwned(SDL_Window* window);

void railPlatformEndDrag(SDL_Window* window);
