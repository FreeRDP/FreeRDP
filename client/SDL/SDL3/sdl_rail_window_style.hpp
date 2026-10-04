/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 RAIL window style classification
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

#include <freerdp/window.h>

struct SdlRailWindowStyle
{
	bool popup = false;
	bool layered = false;
	bool overlay = false;
};

/*
 * RAIL carries the window style, but not SetWindowPos flags such as SWP_NOACTIVATE. An unowned,
 * caption-less WS_EX_TOOLWINDOW/WS_POPUP is therefore treated as a popup even when applications
 * add decoration bits such as WS_SYSMENU or WS_MINIMIZEBOX. Keep owned popup/tool windows on their
 * existing path: some applications implement their own drag loop for those windows.
 */
inline SdlRailWindowStyle classifySdlRailWindowStyle(uint32_t style, uint32_t exStyle, bool owned)
{
	/* WS_CAPTION is WS_BORDER | WS_DLGFRAME; a partial match is only a border. */
	const bool captioned = (style & WS_CAPTION) == WS_CAPTION;
	const bool isDialogOrApp =
	    captioned ||
	    ((style & (WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) != 0) ||
	    ((exStyle & WS_EX_APPWINDOW) != 0);
	const bool isToolOrPopup =
	    ((exStyle & (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW)) != 0) || ((style & WS_POPUP) != 0);
	const bool noActivate =
	    ((exStyle & WS_EX_NOACTIVATE) != 0) && ((exStyle & WS_EX_APPWINDOW) == 0);
	const bool unownedPopup =
	    !owned && !captioned && ((style & WS_POPUP) != 0) &&
	    ((exStyle & (WS_EX_TOOLWINDOW | WS_EX_APPWINDOW)) == WS_EX_TOOLWINDOW);

	/* Route drag/feedback overlays through the popup path. */
	constexpr uint32_t overlayEx = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
	const bool overlay = ((exStyle & overlayEx) == overlayEx) &&
	                     ((exStyle & (WS_EX_NOACTIVATE | WS_EX_APPWINDOW)) == 0) && !captioned;

	return { noActivate || overlay || unownedPopup || (isToolOrPopup && !isDialogOrApp),
		     ((exStyle & WS_EX_LAYERED) != 0) && !isDialogOrApp, overlay };
}
