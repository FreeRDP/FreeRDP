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

#include <map>
#include <memory>
#include <sstream>
#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>

#include <freerdp/freerdp.h>

#include <SDL3/SDL.h>

#include <sdl_common_utils.hpp>

#include "sdl_window.hpp"
#include "sdl_disp.hpp"
#include "sdl_clip.hpp"
#include "sdl_kbd.hpp"

#include "dialogs/sdl_connection_dialog_wrapper.hpp"

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
	void cleanup();

	[[nodiscard]] bool resizeable() const;
	[[nodiscard]] bool toggleResizeable();
	[[nodiscard]] bool setResizeable(bool enable);

	[[nodiscard]] bool fullscreen() const;
	[[nodiscard]] bool toggleFullscreen();
	[[nodiscard]] bool setFullscreen(bool enter);

	[[nodiscard]] bool setMinimized();

	[[nodiscard]] bool grabMouse() const;
	[[nodiscard]] bool toggleGrabMouse();
	[[nodiscard]] bool setGrabMouse(bool enter);

	[[nodiscard]] bool grabKeyboard() const;
	[[nodiscard]] bool toggleGrabKeyboard();
	[[nodiscard]] bool setGrabKeyboard(bool enter);

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

	void setMetadata();

	[[nodiscard]] int start();
	[[nodiscard]] int join();
	[[nodiscard]] bool shallAbort(bool ignoreDialogs = false);

	[[nodiscard]] bool createWindows();
	[[nodiscard]] bool updateWindowList();

	[[nodiscard]] bool drawToWindows(const std::vector<SDL_Rect>& rects = {});
	[[nodiscard]] bool drawToWindow(SdlWindow& window, const std::vector<SDL_Rect>& rects = {});
	[[nodiscard]] bool minimizeAllWindows();
	[[nodiscard]] int exitCode() const;
	[[nodiscard]] SDL_PixelFormat pixelFormat() const;

	[[nodiscard]] SdlWindow* getWindowForId(SDL_WindowID id);
	[[nodiscard]] SdlWindow* getFirstWindow();

	[[nodiscard]] bool addDisplayWindow(SDL_DisplayID id);
	[[nodiscard]] bool removeDisplay(SDL_DisplayID id);

	[[nodiscard]] sdlDispContext& getDisplayChannelContext();
	[[nodiscard]] sdlInput& getInputChannelContext();
	[[nodiscard]] sdlClip& getClipboardChannelContext();

	[[nodiscard]] SdlConnectionDialogWrapper& getDialog();

	[[nodiscard]] wLog* getWLog();

	[[nodiscard]] bool handleEvent(const SDL_WindowEvent* ev);
	[[nodiscard]] bool handleEvent(const SDL_DisplayEvent* ev);

  private:
	[[nodiscard]] static BOOL preConnect(freerdp* instance);
	[[nodiscard]] static BOOL postConnect(freerdp* instance);
	static void postDisconnect(freerdp* instance);
	static void postFinalDisconnect(freerdp* instance);
	[[nodiscard]] static BOOL desktopResize(rdpContext* context);
	[[nodiscard]] static BOOL playSound(rdpContext* context, const PLAY_SOUND_UPDATE* play_sound);
	[[nodiscard]] static BOOL beginPaint(rdpContext* context);
	[[nodiscard]] static BOOL endPaint(rdpContext* context);
	[[nodiscard]] static DWORD WINAPI rdpThreadRun(SdlContext* sdl);

	[[nodiscard]] SDL_FPoint applyLocalScaling(const SDL_FPoint& val) const;
	void removeLocalScaling(float& x, float& y);

	[[nodiscard]] bool createPrimary();
	[[nodiscard]] std::string windowTitle() const;
	[[nodiscard]] bool waitForWindowsCreated();

	void sdl_client_cleanup(int exit_code, const std::string& error_msg);
	[[nodiscard]] int sdl_client_thread_connect(std::string& error_msg);
	[[nodiscard]] int sdl_client_thread_run(std::string& error_msg);

	[[nodiscard]] int error_info_to_error(DWORD* pcode, char** msg, size_t* len) const;

	rdpContext* _context = nullptr;
	wLog* _log = nullptr;

	std::atomic<bool> _connected = false;
	bool _cursor_visible = true;
	rdpPointer* _cursor = nullptr;
	std::vector<SDL_DisplayID> _monitorIds;
	std::mutex _queue_mux;
	std::queue<std::vector<SDL_Rect>> _queue;
	/* SDL */
	bool _fullscreen = false;
	bool _resizeable = false;
	bool _grabMouse = false;
	bool _grabKeyboard = false;
	int _exitCode = -1;
	std::atomic<bool> _rdpThreadRunning = false;
	SDL_PixelFormat _sdlPixelFormat = SDL_PIXELFORMAT_UNKNOWN;

	CriticalSection _critical;

	using SDLSurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

	SDLSurfacePtr _primary;
	SDL_FPoint _localScale{ 1.0f, 1.0f };

	sdlDispContext _disp;
	sdlInput _input;
	sdlClip _clip;

	SdlConnectionDialogWrapper _dialog;

	std::map<Uint32, SdlWindow> _windows;

	WinPREvent _windowsCreatedEvent;
	std::thread _thread;
};
