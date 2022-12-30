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
#ifndef FREERDP_CLIENT_SDL_DISP_H
#define FREERDP_CLIENT_SDL_DISP_H

#include <freerdp/types.h>
#include <freerdp/client/disp.h>

#include "sdl_freerdp.h"

BOOL sdl_disp_init(sdlDispContext* xfDisp, DispClientContext* disp);
BOOL sdl_disp_uninit(sdlDispContext* xfDisp, DispClientContext* disp);

sdlDispContext* sdl_disp_new(sdlContext* sdl);
void sdl_disp_free(sdlDispContext* disp);

#if SDL_VERSION_ATLEAST(2, 0, 10)
BOOL sdl_disp_handle_display_event(sdlDispContext* disp, const SDL_DisplayEvent* ev);
#endif

BOOL sdl_disp_handle_window_event(sdlDispContext* disp, const SDL_WindowEvent* ev);

BOOL sdl_grab_keyboard(sdlContext* sdl, Uint32 windowID, SDL_bool enable);
BOOL sdl_grab_mouse(sdlContext* sdl, Uint32 windowID, SDL_bool enable);

#endif /* FREERDP_CLIENT_SDL_DISP_H */
