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
	bool supportsWindowPositions = true;
	bool supportsTransparentWindows = true;
	bool supportsRestack = false;
	RailBandMechanism band = RailBandMechanism::None;
	RailDragModel drag = RailDragModel::None;
};

/* Detected capabilities of the running SDL video backend (cached). */
[[nodiscard]] const RailPlatformCaps& railPlatformCaps();
