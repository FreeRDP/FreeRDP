/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Clipboard Channel
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#include <string>

#include <SDL.h>

#include "sdl_cliprdr_context.hpp"
#include "../sdl_freerdp.hpp"

#if defined(_WIN32)
#include "sdl_win_cliprdr_context.hpp"
#endif
#if defined(__APPLE__)
#include "sdl_apple_cliprdr_context.hpp"
#endif

#if defined(WITH_X11)
#include "sdl_x11_cliprdr_context.hpp"
#endif

#if defined(WITH_WAYLAND)
#include "sdl_wayland_cliprdr_context.hpp"
#endif

#if defined(__ANDROID__)
#include "sdl_android_cliprdr_context.hpp"
#endif

SdlCliprdrContext* SdlCliprdrContext::instance(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);
	const std::string platform(SDL_GetPlatform());
	const std::string driver(SDL_GetCurrentVideoDriver());

#if defined(_WIN32)
	if (platform == "Windows")
		return new SdlWinCliprdrContext(sdl);
#endif
#if defined(__APPLE__)
	if (platform == "Mac OS X")
		return new SdlAppleCliprdrContext(sdl);
	if (platform == "iOS")
		return new SdlAppleCliprdrContext(sdl);
#endif
	if (platform == "Linux")
	{
#if defined(WITH_WAYLAND)
		if (driver == "wayland")
			return new SdlWaylandCliprdrContext(sdl);
#endif
#if defined(WITH_X11)
		if (driver == "x11")
			return new SdlX11CliprdrContext(sdl);
#endif
	}
#if defined(__ANDROID__)
	else if (platform == "Android")
	{
		return new SdlAndroidCliprdrContext(sdl);
	}
#endif

	WLog_Print(sdl->log, WLOG_WARN, "Unsupported platform [%s:%s], no clipboard available",
	           platform.c_str(), driver.c_str());
	return nullptr;
}
