/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Display Control Channel
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
#pragma once

#include <vector>

#include <freerdp/types.h>
#include <freerdp/event.h>
#include <freerdp/client/disp.h>

#include "sdl_types.hpp"

#include <SDL3/SDL.h>

class sdlDispContext
{

  public:
	explicit sdlDispContext(SdlContext* sdl);
	sdlDispContext(const sdlDispContext& other) = delete;
	sdlDispContext(sdlDispContext&& other) = delete;
	~sdlDispContext();

	sdlDispContext& operator=(const sdlDispContext& other) = delete;
	sdlDispContext& operator=(sdlDispContext&& other) = delete;

	[[nodiscard]] bool init(DispClientContext* disp);
	[[nodiscard]] bool uninit(DispClientContext* disp);

	[[nodiscard]] bool handle_display_event(const SDL_DisplayEvent* ev);
	[[nodiscard]] bool handle_window_event(const SDL_WindowEvent* ev);

  private:
	UINT DisplayControlCaps(UINT32 maxNumMonitors, UINT32 maxMonitorAreaFactorA,
	                        UINT32 maxMonitorAreaFactorB);
	bool set_window_resizable();

	bool sendResize();
	bool settings_changed(const std::vector<DISPLAY_CONTROL_MONITOR_LAYOUT>& layout);
	UINT sendLayout(const rdpMonitor* monitors, size_t nmonitors);

	bool addTimer();

	bool updateMonitor(SDL_WindowID id);
	bool updateMonitors(SDL_EventType type);

	static UINT DisplayControlCaps(DispClientContext* disp, UINT32 maxNumMonitors,
	                               UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB);
	static void OnActivated(void* context, const ActivatedEventArgs* e);
	static void OnGraphicsReset(void* context, const GraphicsResetEventArgs* e);
	static Uint32 SDLCALL OnTimer(void* param, SDL_TimerID timerID, Uint32 interval);

	SdlContext* _sdl = nullptr;
	DispClientContext* _disp = nullptr;
	UINT64 _lastSentDate = 0;
	bool _activated = false;
	bool _waitingResize = false;
	SDL_TimerID _timer = 0;
	unsigned _timer_retries = 0;
	std::vector<DISPLAY_CONTROL_MONITOR_LAYOUT> _last_sent_layout;
};
