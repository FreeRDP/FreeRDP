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
#include <queue>
#include <mutex>

#include <freerdp/freerdp.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include "sdl_types.hpp"
#include "sdl_disp.hpp"
#include "sdl_kbd.hpp"
#include "sdl_clip.hpp"
#include "sdl_utils.hpp"
#include "sdl_window.hpp"
#include "dialogs/sdl_connection_dialog_wrapper.hpp"

using SDLSurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

class SdlContext
{
  public:
	explicit SdlContext(rdpContext* context);
	SdlContext(const SdlContext& other) = delete;
	SdlContext(SdlContext&& other) = delete;
	~SdlContext() = default;

	SdlContext& operator=(const SdlContext& other) = delete;
	SdlContext& operator=(SdlContext&& other) = delete;

	[[nodiscard]] bool redraw(bool suppress = false) const;

	void setConnected(bool val);
	[[nodiscard]] bool isConnected() const;

	[[nodiscard]] bool update_resizeable(bool enable);
	[[nodiscard]] bool update_fullscreen(bool enter);
	[[nodiscard]] bool update_minimize();

	[[nodiscard]] rdpContext* context() const;
	[[nodiscard]] rdpClientContext* common() const;

	void setCursor(rdpPointer* cursor);
	[[nodiscard]] rdpPointer* cursor() const;

	void setMonitorIds(const std::vector<SDL_DisplayID>& ids);
	const std::vector<SDL_DisplayID>& monitorIds() const;
	int64_t monitorId(uint32_t index) const;

	void push(std::vector<SDL_Rect>&& rects);
	std::vector<SDL_Rect> pop();

	void setHasCursor(bool val);
	[[nodiscard]] bool hasCursor() const;

  private:
	rdpContext* _context;
	std::atomic<bool> _connected = false;
	bool _cursor_visible = true;
	rdpPointer* _cursor = nullptr;
	std::vector<SDL_DisplayID> _monitorIds;
	std::mutex _queue_mux;
	std::queue<std::vector<SDL_Rect>> _queue;

  public:
	wLog* log;

	/* SDL */
	bool fullscreen = false;
	bool resizeable = false;
	bool grab_mouse = false;
	bool grab_kbd = false;

	std::map<Uint32, SdlWindow> windows;

	CriticalSection critical;
	std::thread thread;
	WinPREvent initialize;
	WinPREvent initialized;
	WinPREvent windows_created;
	int exit_code = -1;

	sdlDispContext disp;
	sdlInput input;
	sdlClip clip;

	SDLSurfacePtr primary;

	SDL_PixelFormat sdl_pixel_format = SDL_PIXELFORMAT_UNKNOWN;

	std::atomic<bool> rdp_thread_running;
	SdlConnectionDialogWrapper dialog;
};
