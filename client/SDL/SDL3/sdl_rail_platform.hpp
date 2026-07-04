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
#pragma once

/**
 * Windowing-system capabilities the RAIL code must adapt to.
 *
 * The common RAIL code is written for the "positionable, composited" case
 * (X11, Windows, macOS) and never asks "which driver am I on?" - it asks what
 * the platform can do. Only two axes actually diverge across the supported
 * backends, and they are independent:
 *
 *   - positionsReadable is false only on Wayland (toplevels can't be placed or
 *     queried); it drives popup parenting (geometric vs focus fallback), the
 *     parentless-popup guard, the min/max-size cap, and the compositor-grab
 *     move/resize completion path.
 *   - supportsTransparentWindows is false only on non-composited X11 (an ARGB
 *     window would show a black background); it drives the SDL_WINDOW_TRANSPARENT
 *     flag used for the translucent resize placeholder.
 *
 * A new platform is added by extending railPlatformCaps(), not by sprinkling
 * driver checks through the common code.
 */
struct RailPlatformCaps
{
	bool positionsReadable = true;
	bool supportsTransparentWindows = true;
};

/** Capabilities of the running SDL video backend. Detected once on first call
 *  (the backend is fixed for the process lifetime) and cached. */
[[nodiscard]] const RailPlatformCaps& railPlatformCaps();
