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
#include "sdl_rail_platform.hpp"
#include "sdl_utils.hpp"
#include "sdl_wayland.hpp"
#include "sdl_x11.hpp"

#include <freerdp/log.h>
#include <freerdp/rail.h>

#define TAG CLIENT_TAG("sdl.rail")

static RailPlatformCaps detectRailPlatformCaps()
{
	RailPlatformCaps caps;
	if (sdl::utils::isWaylandDriver())
	{
		/* Wayland: toplevels can't be positioned or queried; its compositor always composites. */
		caps.kind = RailPlatformKind::Wayland;
		caps.supportsWindowPositions = false;
		caps.supportsTransparentWindows = true;
		caps.supportsRestack = false;
		caps.band = RailBandMechanism::None;
		caps.drag = RailDragModel::CompositorResize;
	}
	else if (sdl::utils::isX11Driver())
	{
		/* X11: transparent resize band requires compositing (_GTK_FRAME_EXTENTS). */
		caps.kind = RailPlatformKind::X11;
		caps.supportsWindowPositions = true;
		caps.supportsTransparentWindows = sdl_x11_has_compositor();
		caps.supportsRestack = true;
		caps.band = caps.supportsTransparentWindows ? RailBandMechanism::FrameExtents
		                                            : RailBandMechanism::None;
		caps.drag = RailDragModel::WmMoveResize;
	}
	return caps;
}

const RailPlatformCaps& railPlatformCaps()
{
	static const RailPlatformCaps caps = detectRailPlatformCaps();
	return caps;
}

/* RAIL_WMSZ_* -> _NET_WM_MOVERESIZE direction (0..7 = resize edges, 8 = move). */
[[nodiscard]] static int railToNetDirection(uint16_t moveType)
{
	switch (moveType)
	{
		case RAIL_WMSZ_TOPLEFT:
			return 0;
		case RAIL_WMSZ_TOP:
			return 1;
		case RAIL_WMSZ_TOPRIGHT:
			return 2;
		case RAIL_WMSZ_RIGHT:
			return 3;
		case RAIL_WMSZ_BOTTOMRIGHT:
			return 4;
		case RAIL_WMSZ_BOTTOM:
			return 5;
		case RAIL_WMSZ_BOTTOMLEFT:
			return 6;
		case RAIL_WMSZ_LEFT:
			return 7;
		case RAIL_WMSZ_MOVE:
		default:
			return 8;
	}
}

static constexpr struct
{
	uint16_t rail;
	uint32_t xdg;
} kRailXdgEdges[] = {
	{ RAIL_WMSZ_TOP, 1 },      { RAIL_WMSZ_BOTTOM, 2 },       { RAIL_WMSZ_LEFT, 4 },
	{ RAIL_WMSZ_TOPLEFT, 5 },  { RAIL_WMSZ_BOTTOMLEFT, 6 },   { RAIL_WMSZ_RIGHT, 8 },
	{ RAIL_WMSZ_TOPRIGHT, 9 }, { RAIL_WMSZ_BOTTOMRIGHT, 10 },
};

/* RAIL_WMSZ_* -> XDG_TOPLEVEL_RESIZE_EDGE_* (0 = none, e.g. RAIL_WMSZ_MOVE). */
[[nodiscard]] static uint32_t railToXdgEdge(uint16_t moveType)
{
	for (const auto& e : kRailXdgEdges)
		if (e.rail == moveType)
			return e.xdg;
	return 0;
}

RailEdges railEdges(uint16_t t)
{
	return { (t == RAIL_WMSZ_TOPLEFT) || (t == RAIL_WMSZ_LEFT) || (t == RAIL_WMSZ_BOTTOMLEFT),
		     (t == RAIL_WMSZ_TOPRIGHT) || (t == RAIL_WMSZ_RIGHT) || (t == RAIL_WMSZ_BOTTOMRIGHT),
		     (t == RAIL_WMSZ_TOPLEFT) || (t == RAIL_WMSZ_TOP) || (t == RAIL_WMSZ_TOPRIGHT),
		     (t == RAIL_WMSZ_BOTTOMLEFT) || (t == RAIL_WMSZ_BOTTOM) ||
		         (t == RAIL_WMSZ_BOTTOMRIGHT) };
}

void railPlatformPrepareWindow(SDL_Window* window)
{
	switch (railPlatformCaps().kind)
	{
		case RailPlatformKind::Wayland:
			sdl_wayland_move_prepare(window);
			break;
		default:
			break;
	}
}

bool railPlatformSetBand(SDL_Window* window, const SDL_Rect& insets)
{
	const auto& caps = railPlatformCaps();
	if (caps.band == RailBandMechanism::None)
		return false;
	switch (caps.kind)
	{
		case RailPlatformKind::X11:
			return sdl_x11_set_frame_extents(window, insets.x, insets.w, insets.y, insets.h);
		default:
			return false;
	}
}

bool railPlatformRestack(const std::vector<SDL_Window*>& topToBottom)
{
	switch (railPlatformCaps().kind)
	{
		case RailPlatformKind::X11:
			return sdl_x11_restack_windows(topToBottom);
		default:
			return false;
	}
}

bool railPlatformBeginDrag(SDL_Window* window, uint16_t moveType)
{
	switch (railPlatformCaps().kind)
	{
		case RailPlatformKind::Wayland:
			if (moveType == RAIL_WMSZ_MOVE)
				return sdl_wayland_begin_move(window);
			return sdl_wayland_begin_resize(window, railToXdgEdge(moveType));
		case RailPlatformKind::X11:
		{
			if (!sdl_x11_begin_move_size(window, railToNetDirection(moveType)))
				return false;
			/* Bit gravity keeps the content at the fixed edge while the WM resizes. */
			const RailEdges e = railEdges(moveType);
			const int row = e.top ? 2 : (e.bottom ? 0 : 1);
			const int col = e.left ? 2 : (e.right ? 0 : 1);
			if (!sdl_x11_set_bit_gravity(window, row * 3 + col + 1))
				WLog_DBG(TAG, "setting the X11 bit gravity failed");
			return true;
		}
		default:
			return false;
	}
}

bool railPlatformReleaseButton(SDL_Window* window)
{
	switch (railPlatformCaps().kind)
	{
		case RailPlatformKind::X11:
			return sdl_x11_send_left_button_release(window);
		default:
			return false;
	}
}

bool railPlatformRaiseOwned(SDL_Window* window)
{
	/* Elsewhere the owner relation keeps the window above its owner. */
	if (railPlatformCaps().kind != RailPlatformKind::X11)
		return true;
	return sdl_x11_raise_window(window);
}

void railPlatformEndDrag(SDL_Window* window)
{
	if (railPlatformCaps().kind == RailPlatformKind::X11)
		if (!sdl_x11_set_bit_gravity(window, 0 /* reset to default */))
			WLog_DBG(TAG, "resetting the X11 bit gravity failed");
}
