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
#include "sdl_win32.hpp"
#include "sdl_utils.hpp"

#include <winpr/platform.h>

#if defined(_WIN32)

#include <windows.h>

[[nodiscard]] static HWND sdl_win32_hwnd(SDL_Window* window)
{
	if (!window || !sdl::utils::isWindowsDriver())
		return nullptr;
	return static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),
	                                                SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}

bool sdl_win32_restack_windows(const std::vector<SDL_Window*>& topToBottom)
{
	struct Entry
	{
		HWND hwnd;
		bool topmost;
	};
	std::vector<Entry> stack;
	stack.reserve(topToBottom.size());
	for (auto* window : topToBottom)
	{
		HWND hwnd = sdl_win32_hwnd(window);
		if (hwnd)
			stack.push_back({ hwnd, (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0 });
	}
	if (stack.size() < 2)
		return false;

	HDWP dwp = BeginDeferWindowPos(static_cast<int>(stack.size() - 1));
	if (!dwp)
		return false;
	/* Restack within each band: below a topmost window, a window becomes topmost too. */
	HWND prevTopmost = nullptr;
	HWND prevNormal = nullptr;
	for (const auto& e : stack)
	{
		HWND& prev = e.topmost ? prevTopmost : prevNormal;
		if (prev)
		{
			dwp = DeferWindowPos(dwp, e.hwnd, prev, 0, 0, 0, 0,
			                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
			if (!dwp)
				return false;
		}
		prev = e.hwnd;
	}
	return EndDeferWindowPos(dwp) != FALSE;
}

#else /* !_WIN32 */

bool sdl_win32_restack_windows(WINPR_ATTR_UNUSED const std::vector<SDL_Window*>& topToBottom)
{
	return false;
}

#endif
