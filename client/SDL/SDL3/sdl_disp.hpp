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

#include <stdint.h>

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

	bool init(DispClientContext* disp);
	bool uninit(DispClientContext* disp);

	bool handle_display_event(const SDL_DisplayEvent* ev);

	bool handle_window_event(const SDL_WindowEvent* ev);

	bool addTimer();

  private:
	[[nodiscard]] std::vector<rdpMonitor> currentMonitorLayout() const;
	uint32_t DisplayControlCaps(uint32_t maxNumMonitors, uint32_t maxMonitorAreaFactorA,
	                            uint32_t maxMonitorAreaFactorB);
	bool set_window_resizable();

	bool sendResize();
	bool settings_changed();
	bool update_last_sent();
	uint32_t sendLayout(const rdpMonitor* monitors, size_t nmonitors);

	static UINT DisplayControlCaps(DispClientContext* disp, uint32_t maxNumMonitors,
	                               uint32_t maxMonitorAreaFactorA, uint32_t maxMonitorAreaFactorB);
	static void OnActivated(void* context, const ActivatedEventArgs* e);
	static void OnGraphicsReset(void* context, const GraphicsResetEventArgs* e);
	static Uint32 SDLCALL OnTimer(void* param, SDL_TimerID timerID, Uint32 interval);

	SdlContext* _sdl = nullptr;
	DispClientContext* _disp = nullptr;
	int _lastSentWidth = -1;
	int _lastSentHeight = -1;
	uint64_t _lastSentDate = 0;
	int _targetWidth = -1;
	int _targetHeight = -1;
	bool _activated = false;
	bool _waitingResize = false;
	bool _lastSentFullscreen = false;
	std::vector<rdpMonitor> _lastSentLayout;
	uint16_t _lastSentDesktopOrientation = 0;
	uint32_t _lastSentDesktopScaleFactor = 0;
	uint32_t _lastSentDeviceScaleFactor = 0;
	SDL_TimerID _timer = 0;
	unsigned _timer_retries = 0;
};
