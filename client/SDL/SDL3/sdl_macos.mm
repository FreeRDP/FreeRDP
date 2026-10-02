/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native macOS display helpers
 *
 * Copyright 2026 Ocean <25319668+oceanzhang88@users.noreply.github.com>
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
#include "sdl_macos.hpp"

#include <cmath>

#import <AppKit/AppKit.h>

int sdl_macos_fullscreen_space_top_inset(const SDL_Rect &bounds)
{
	@autoreleasepool
	{
		if (@available(macOS 12.0, *))
		{
			for (NSScreen *screen in [NSScreen screens])
			{
				NSNumber *number = [screen deviceDescription][@"NSScreenNumber"];
				if (!number)
					continue;

				/* SDL's cocoa backend reports CGDisplayBounds() as the display bounds. */
				const CGRect cg = CGDisplayBounds([number unsignedIntValue]);
				if ((static_cast<int>(cg.origin.x) != bounds.x) ||
				    (static_cast<int>(cg.origin.y) != bounds.y) ||
				    (static_cast<int>(cg.size.width) != bounds.w) ||
				    (static_cast<int>(cg.size.height) != bounds.h))
					continue;

				/* No camera housing: a full-screen Space covers the whole display. */
				const CGFloat housing = [screen safeAreaInsets].top;
				if (housing <= 0)
					return 0;

				/* With a camera housing AppKit lays the full-screen window out below the menu
				 * bar, which is at least as tall as the housing. */
				const CGFloat menuBar = NSMaxY([screen frame]) - NSMaxY([screen visibleFrame]);
				return static_cast<int>(std::lround(std::fmax(housing, menuBar)));
			}
		}
		return 0;
	}
}
