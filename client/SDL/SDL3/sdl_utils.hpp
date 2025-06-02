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

#include <winpr/synch.h>
#include <winpr/wlog.h>

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <sdl_common_utils.hpp>

template <typename T> using deleted_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

enum
{
	SDL_EVENT_USER_UPDATE = SDL_EVENT_USER + 1,
	SDL_EVENT_USER_CREATE_WINDOWS,
	SDL_EVENT_USER_WINDOW_RESIZEABLE,
	SDL_EVENT_USER_WINDOW_FULLSCREEN,
	SDL_EVENT_USER_WINDOW_MINIMIZE,
	SDL_EVENT_USER_POINTER_NULL,
	SDL_EVENT_USER_POINTER_DEFAULT,
	SDL_EVENT_USER_POINTER_POSITION,
	SDL_EVENT_USER_POINTER_SET,
	SDL_EVENT_USER_QUIT,
	SDL_EVENT_USER_CERT_DIALOG,
	SDL_EVENT_USER_SHOW_DIALOG,
	SDL_EVENT_USER_AUTH_DIALOG,
	SDL_EVENT_USER_SCARD_DIALOG,
	SDL_EVENT_USER_RETRY_DIALOG,

	SDL_EVENT_USER_CERT_RESULT,
	SDL_EVENT_USER_SHOW_RESULT,
	SDL_EVENT_USER_AUTH_RESULT,
	SDL_EVENT_USER_SCARD_RESULT
};

typedef struct
{
	Uint32 type;
	Uint32 timestamp;
	char* title;
	char* user;
	char* domain;
	char* password;
	Sint32 result;
} SDL_UserAuthArg;

bool sdl_push_user_event(Uint32 type, ...);

bool sdl_push_quit();

std::string sdl_window_event_str(Uint32 ev);
const char* sdl_event_type_str(Uint32 type);
const char* sdl_error_string(Sint32 res);

#define sdl_log_error(res, log, what) sdl_log_error_ex(res, log, what, __FILE__, __LINE__, __func__)
BOOL sdl_log_error_ex(Sint32 res, wLog* log, const char* what, const char* file, size_t line,
                      const char* fkt);

namespace sdl::utils
{
	std::string rdp_orientation_to_str(uint32_t orientation);
	std::string sdl_orientation_to_str(SDL_DisplayOrientation orientation);
	UINT32 orientaion_to_rdp(SDL_DisplayOrientation orientation);

	std::string generate_uuid_v4();
} // namespace sdl::utils
