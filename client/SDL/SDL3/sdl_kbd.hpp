/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client keyboard helper
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

#include <string>
#include <map>
#include <list>
#include <atomic>

#include <winpr/wtypes.h>
#include <freerdp/freerdp.h>
#include <SDL3/SDL.h>

#include "sdl_types.hpp"

class sdlInput
{
  public:
	explicit sdlInput(SdlContext* sdl);
	sdlInput(const sdlInput& other) = delete;
	sdlInput(sdlInput&& other) = delete;
	~sdlInput() = default;

	sdlInput& operator=(const sdlInput& other) = delete;
	sdlInput& operator=(sdlInput&& other) = delete;

	BOOL keyboard_sync_state();
	BOOL keyboard_focus_in();

	BOOL keyboard_handle_event(const SDL_KeyboardEvent* ev);

	BOOL keyboard_grab(Uint32 windowID, bool enable);
	BOOL mouse_focus(Uint32 windowID);
	BOOL mouse_grab(Uint32 windowID, bool enable);

	static BOOL keyboard_set_indicators(rdpContext* context, UINT16 led_flags);
	static BOOL keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
	                                    UINT32 imeConvMode);

	static uint32_t prefToMask();
	static uint32_t prefKeyValue(const std::string& key, uint32_t fallback = SDL_SCANCODE_UNKNOWN);

  private:
	static std::list<std::string> tokenize(const std::string& data,
	                                       const std::string& delimiter = ",");
	static bool extract(const std::string& token, uint32_t& key, uint32_t& value);

	uint32_t remapScancode(uint32_t scancode);
	void remapInitialize();

	SdlContext* _sdl;
	Uint32 _lastWindowID;
	std::map<uint32_t, uint32_t> _remapList;
	std::atomic<bool> _remapInitialized = false;

	// hotkey handling
	uint32_t _hotkeyModmask; // modifier keys mask
	uint32_t _hotkeyFullscreen;
	uint32_t _hotkeyResizable;
	uint32_t _hotkeyGrab;
	uint32_t _hotkeyDisconnect;
	uint32_t _hotkeyMinimize;
};
