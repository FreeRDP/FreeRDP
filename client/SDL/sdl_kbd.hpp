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

#include <winpr/wtypes.h>
#include <freerdp/freerdp.h>
#include <SDL.h>

#include "sdl_types.hpp"

class sdlInput
{
  public:
	sdlInput(SdlContext* sdl);
	~sdlInput();

	BOOL keyboard_sync_state();
	BOOL keyboard_focus_in();

	BOOL keyboard_handle_event(const SDL_KeyboardEvent* ev);

	BOOL keyboard_grab(Uint32 windowID, SDL_bool enable);
	BOOL mouse_focus(Uint32 windowID);
	BOOL mouse_grab(Uint32 windowID, SDL_bool enable);

  public:
	static BOOL keyboard_set_indicators(rdpContext* context, UINT16 led_flags);
	static BOOL keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
	                                    UINT32 imeConvMode);

  private:
	SdlContext* _sdl;
	Uint32 _lastWindowID;
};
