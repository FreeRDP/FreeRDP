/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client RAIL platform capabilities
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
#include "sdl_x11.hpp"

static RailPlatformCaps detectRailPlatformCaps()
{
	RailPlatformCaps caps;
	if (sdl::utils::isWaylandDriver())
	{
		/* Wayland: toplevels can't be positioned or queried; its compositor always composites. */
		caps.positionsReadable = false;
		caps.supportsTransparentWindows = true;
	}
	else
	{
		/* X11 / Windows / macOS: positions are real. Transparency is safe everywhere except a
		 * non-composited X11 server, where an ARGB window would render on black.
		 * sdl_x11_has_compositor() answers true off X11, so it needs no backend guard here. */
		caps.positionsReadable = true;
		caps.supportsTransparentWindows = sdl_x11_has_compositor();
	}
	return caps;
}

const RailPlatformCaps& railPlatformCaps()
{
	static const RailPlatformCaps caps = detectRailPlatformCaps();
	return caps;
}
