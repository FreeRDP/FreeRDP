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
	FrameExtents, /* X11 _GTK_FRAME_EXTENTS, published before the resize */
	NativeFrame   /* Win32 sizing frame: the OS adds it outside the client area */
};

/* Drag and resize execution model. */
enum class RailDragModel
{
	None,             /* the server drags the window itself */
	WmMoveResize,     /* X11 _NET_WM_MOVERESIZE, ends with the button release */
	CompositorResize, /* Wayland xdg_toplevel move/resize, the client only tracks the size */
	ModalLoop         /* Win32 SC_MOVE/SC_SIZE loop, blocks until the drag ends */
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

/* Place a window by its client rect where SDL cannot (Win32 native frame); false otherwise. */
bool railPlatformSetWindowRect(SDL_Window* window, const SDL_Rect& rect);

/* ModalLoop: redraw on the native loop's ticks; resize at start (RAIL_WMSZ_* edge) and end
 * of a native-frame resize. A default handler unregisters. */
struct RailNativeLoopHandler
{
	void* userdata = nullptr;
	void (*redraw)(void* userdata) = nullptr;
	void (*resize)(void* userdata, SDL_WindowID id, int moveType, bool end) = nullptr;
};
void railPlatformSetNativeLoopHandler(const RailNativeLoopHandler& handler);

/* Declare the band outside the content (x=L y=T w=R h=B) the way caps.band says. */
bool railPlatformSetBand(SDL_Window* window, const SDL_Rect& insets);

/* Restack windows top-to-bottom without activating them. */
bool railPlatformRestack(const std::vector<SDL_Window*>& topToBottom);

/* Start a local move (RAIL_WMSZ_MOVE) or resize (RAIL_WMSZ_* edge) the way caps.drag says. */
bool railPlatformBeginDrag(SDL_Window* window, uint16_t moveType);

/* ModalLoop platforms: run the native drag; blocks until it ends, so call it unlocked. */
void railPlatformRunModalDrag(SDL_Window* window, uint16_t moveType);

/* The WM kept the left button release of the drag; hand one to SDL. */
bool railPlatformReleaseButton(SDL_Window* window);

/* Put a reattached owned window back above its owner without activating it. */
bool railPlatformRaiseOwned(SDL_Window* window);

void railPlatformEndDrag(SDL_Window* window);
