/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include <memory>
#include <thread>
#include <map>
#include <atomic>

#include <freerdp/freerdp.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>

#include <SDL.h>
#include <SDL_video.h>

#include "sdl_types.hpp"
#include "sdl_disp.hpp"
#include "sdl_kbd.hpp"
#include "sdl_utils.hpp"
#include "sdl_window.hpp"
#include "dialogs/sdl_connection_dialog.hpp"

using SDLSurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
using SDLPixelFormatPtr = std::unique_ptr<SDL_PixelFormat, decltype(&SDL_FreeFormat)>;

class SdlContext
{
  public:
	explicit SdlContext(rdpContext* context);
	SdlContext(const SdlContext& other) = delete;
	SdlContext(SdlContext&& other) = delete;

	SdlContext& operator=(const SdlContext& other) = delete;
	SdlContext& operator=(SdlContext&& other) = delete;

  private:
	rdpContext* _context;

  public:
	wLog* log;

	/* SDL */
	bool fullscreen = false;
	bool resizeable = false;
	bool grab_mouse = false;
	bool grab_kbd = false;
	bool grab_kbd_enabled = true;

	std::map<Uint32, SdlWindow> windows;

	CriticalSection critical;
	std::thread thread;
	WinPREvent initialize;
	WinPREvent initialized;
	WinPREvent update_complete;
	WinPREvent windows_created;
	int exit_code = -1;

	sdlDispContext disp;
	sdlInput input;

	SDLSurfacePtr primary;
	SDLPixelFormatPtr primary_format;

	Uint32 sdl_pixel_format = 0;

	std::unique_ptr<SDLConnectionDialog> connection_dialog;

	std::atomic<bool> rdp_thread_running;

	BOOL update_resizeable(BOOL enable);
	BOOL update_fullscreen(BOOL enter);
	BOOL update_minimize();

	[[nodiscard]] rdpContext* context() const;
	[[nodiscard]] rdpClientContext* common() const;
};
