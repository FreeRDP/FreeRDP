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

#ifndef FREERDP_CLIENT_SDL_UTILS_H
#define FREERDP_CLIENT_SDL_UTILS_H

#include <winpr/wlog.h>

#include <stdbool.h>
#include <SDL.h>

enum
{
	SDL_USEREVENT_UPDATE = SDL_USEREVENT + 1,
	SDL_USEREVENT_CREATE_WINDOWS,
	SDL_USEREVENT_WINDOW_RESIZEABLE,
	SDL_USEREVENT_WINDOW_FULLSCREEN,
	SDL_USEREVENT_POINTER_NULL,
	SDL_USEREVENT_POINTER_DEFAULT,
	SDL_USEREVENT_POINTER_POSITION,
	SDL_USEREVENT_POINTER_SET
};

BOOL sdl_push_user_event(Uint32 type, ...);

const char* sdl_event_type_str(Uint32 type);
const char* sdl_error_string(Uint32 res);

#define sdl_log_error(res, log, what) \
	sdl_log_error_ex(res, log, what, __FILE__, __LINE__, __FUNCTION__)
BOOL sdl_log_error_ex(Uint32 res, wLog* log, const char* what, const char* file, size_t line,
                      const char* fkt);

#endif /* FREERDP_CLIENT_SDL_UTILS_H */
