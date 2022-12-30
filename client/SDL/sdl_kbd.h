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

#ifndef FREERDP_CLIENT_SDL_KBD_H
#define FREERDP_CLIENT_SDL_KBD_H

#include <winpr/wtypes.h>
#include <freerdp/freerdp.h>
#include <SDL.h>

#include "sdl_freerdp.h"

BOOL sdl_sync_kbd_state(rdpContext* context);
BOOL sdl_keyboard_focus_in(rdpContext* context);

BOOL sdl_keyboard_set_indicators(rdpContext* context, UINT16 led_flags);
BOOL sdl_keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                 UINT32 imeConvMode);

BOOL sdl_handle_keyboard_event(sdlContext* sdl, const SDL_KeyboardEvent* ev);

#endif /* FREERDP_CLIENT_SDL_KBD_H */
