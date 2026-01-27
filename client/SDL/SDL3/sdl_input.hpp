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
#include <freerdp/locale/keyboard.h>
#include <SDL3/SDL.h>

#include "sdl_types.hpp"

class sdlInput
{
  public:
	explicit sdlInput(SdlContext* sdl);
	sdlInput(const sdlInput& other) = delete;
	sdlInput(sdlInput&& other) = delete;
	virtual ~sdlInput();

	sdlInput& operator=(const sdlInput& other) = delete;
	sdlInput& operator=(sdlInput&& other) = delete;

	[[nodiscard]] bool initialize();

	[[nodiscard]] bool keyboard_sync_state();
	[[nodiscard]] bool keyboard_focus_in();

	[[nodiscard]] bool handleEvent(const SDL_KeyboardEvent& ev);

	[[nodiscard]] bool keyboard_grab(Uint32 windowID, bool enable);
	[[nodiscard]] bool mouse_focus(Uint32 windowID);
	[[nodiscard]] bool mouse_grab(Uint32 windowID, bool enable);

	[[nodiscard]] static BOOL keyboard_set_indicators(rdpContext* context, UINT16 led_flags);
	[[nodiscard]] static BOOL keyboard_set_ime_status(rdpContext* context, UINT16 imeId,
	                                                  UINT32 imeState, UINT32 imeConvMode);

	[[nodiscard]] bool prefToEnabled();
	[[nodiscard]] uint32_t prefToMask();
	[[nodiscard]] static uint32_t prefKeyValue(const std::string& key,
	                                           uint32_t fallback = SDL_SCANCODE_UNKNOWN);

  private:
	[[nodiscard]] static std::list<std::string> tokenize(const std::string& data,
	                                                     const std::string& delimiter = ",");
	[[nodiscard]] static bool extract(const std::string& token, uint32_t& key, uint32_t& value);

	[[nodiscard]] UINT32 scancode_to_rdp(Uint32 scancode);

	SdlContext* _sdl = nullptr;
	Uint32 _lastWindowID = 0;

	// hotkey handling
	bool _hotkeysEnabled = false;
	uint32_t _hotkeyModmask = 0; // modifier keys mask
	uint32_t _hotkeyFullscreen = 0;
	uint32_t _hotkeyResizable = 0;
	uint32_t _hotkeyGrab = 0;
	uint32_t _hotkeyDisconnect = 0;
	uint32_t _hotkeyMinimize = 0;
	FREERDP_REMAP_TABLE* _remapTable = nullptr;
};
