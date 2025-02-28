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

#include <SDL.h>

class sdlDispContext
{

  public:
	explicit sdlDispContext(SdlContext* sdl);
	sdlDispContext(const sdlDispContext& other) = delete;
	sdlDispContext(sdlDispContext&& other) = delete;
	~sdlDispContext();

	sdlDispContext& operator=(const sdlDispContext& other) = delete;
	sdlDispContext& operator=(sdlDispContext&& other) = delete;

	BOOL init(DispClientContext* disp);
	BOOL uninit(DispClientContext* disp);

#if SDL_VERSION_ATLEAST(2, 0, 10)
	BOOL handle_display_event(const SDL_DisplayEvent* ev);
#endif

	BOOL handle_window_event(const SDL_WindowEvent* ev);

	BOOL addTimer();

  private:
	[[nodiscard]] std::vector<rdpMonitor> currentMonitorLayout() const;

	UINT DisplayControlCaps(UINT32 maxNumMonitors, UINT32 maxMonitorAreaFactorA,
	                        UINT32 maxMonitorAreaFactorB);
	BOOL set_window_resizable();

	BOOL sendResize();
	BOOL settings_changed();
	UINT sendLayout(const std::vector<rdpMonitor>& monitors);

	static UINT DisplayControlCaps(DispClientContext* disp, UINT32 maxNumMonitors,
	                               UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB);
	static void OnActivated(void* context, const ActivatedEventArgs* e);
	static void OnGraphicsReset(void* context, const GraphicsResetEventArgs* e);
	static Uint32 SDLCALL OnTimer(Uint32 interval, void* param);

	SdlContext* _sdl = nullptr;
	DispClientContext* _disp = nullptr;
	UINT64 _lastSentDate = 0;
	int _targetWidth = -1;
	int _targetHeight = -1;
	BOOL _activated = FALSE;
	BOOL _waitingResize = FALSE;

	SDL_TimerID _timer = 0;
	unsigned _timer_retries = 0;
	std::vector<rdpMonitor> _lastSentLayout;
};
