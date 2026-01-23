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

#include <freerdp/settings.h>

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

[[nodiscard]] bool sdl_push_user_event(Uint32 type, ...);

[[nodiscard]] bool sdl_push_quit();

[[nodiscard]] std::string sdl_window_event_str(Uint32 ev);
[[nodiscard]] const char* sdl_event_type_str(Uint32 type);
[[nodiscard]] const char* sdl_error_string(Sint32 res);

#define sdl_log_error(res, log, what) sdl_log_error_ex(res, log, what, __FILE__, __LINE__, __func__)
[[nodiscard]] BOOL sdl_log_error_ex(Sint32 res, wLog* log, const char* what, const char* file,
                                    size_t line, const char* fkt);

namespace sdl::utils
{
	[[nodiscard]] std::string rdp_orientation_to_str(uint32_t orientation);
	[[nodiscard]] std::string sdl_orientation_to_str(SDL_DisplayOrientation orientation);
	[[nodiscard]] UINT32 orientaion_to_rdp(SDL_DisplayOrientation orientation);

	[[nodiscard]] std::string generate_uuid_v4();

	enum HighDpiScaleMode
	{
		SCALE_MODE_INVALID,
		SCALE_MODE_X11,
		SCALE_MODE_WAYLAND
	};

	[[nodiscard]] HighDpiScaleMode platformScaleMode();

	[[nodiscard]] std::string windowTitle(const rdpSettings* settings);

} // namespace sdl::utils

namespace sdl::error
{
	enum EXIT_CODE
	{
		/* section 0-15: protocol-independent codes */
		SUCCESS = 0,
		DISCONNECT = 1,
		LOGOFF = 2,
		IDLE_TIMEOUT = 3,
		LOGON_TIMEOUT = 4,
		CONN_REPLACED = 5,
		OUT_OF_MEMORY = 6,
		CONN_DENIED = 7,
		CONN_DENIED_FIPS = 8,
		USER_PRIVILEGES = 9,
		FRESH_CREDENTIALS_REQUIRED = 10,
		DISCONNECT_BY_USER = 11,

		/* section 16-31: license error set */
		LICENSE_INTERNAL = 16,
		LICENSE_NO_LICENSE_SERVER = 17,
		LICENSE_NO_LICENSE = 18,
		LICENSE_BAD_CLIENT_MSG = 19,
		LICENSE_HWID_DOESNT_MATCH = 20,
		LICENSE_BAD_CLIENT = 21,
		LICENSE_CANT_FINISH_PROTOCOL = 22,
		LICENSE_CLIENT_ENDED_PROTOCOL = 23,
		LICENSE_BAD_CLIENT_ENCRYPTION = 24,
		LICENSE_CANT_UPGRADE = 25,
		LICENSE_NO_REMOTE_CONNECTIONS = 26,

		/* section 32-127: RDP protocol error set */
		RDP = 32,

		/* section 128-254: xfreerdp specific exit codes */
		PARSE_ARGUMENTS = 128,
		MEMORY = 129,
		PROTOCOL = 130,
		CONN_FAILED = 131,
		AUTH_FAILURE = 132,
		NEGO_FAILURE = 133,
		LOGON_FAILURE = 134,
		ACCOUNT_LOCKED_OUT = 135,
		PRE_CONNECT_FAILED = 136,
		CONNECT_UNDEFINED = 137,
		POST_CONNECT_FAILED = 138,
		DNS_ERROR = 139,
		DNS_NAME_NOT_FOUND = 140,
		CONNECT_FAILED = 141,
		MCS_CONNECT_INITIAL_ERROR = 142,
		TLS_CONNECT_FAILED = 143,
		INSUFFICIENT_PRIVILEGES = 144,
		CONNECT_CANCELLED = 145,

		CONNECT_TRANSPORT_FAILED = 147,
		CONNECT_PASSWORD_EXPIRED = 148,
		CONNECT_PASSWORD_MUST_CHANGE = 149,
		CONNECT_KDC_UNREACHABLE = 150,
		CONNECT_ACCOUNT_DISABLED = 151,
		CONNECT_PASSWORD_CERTAINLY_EXPIRED = 152,
		CONNECT_CLIENT_REVOKED = 153,
		CONNECT_WRONG_PASSWORD = 154,
		CONNECT_ACCESS_DENIED = 155,
		CONNECT_ACCOUNT_RESTRICTION = 156,
		CONNECT_ACCOUNT_EXPIRED = 157,
		CONNECT_LOGON_TYPE_NOT_GRANTED = 158,
		CONNECT_NO_OR_MISSING_CREDENTIALS = 159,
		CONNECT_TARGET_BOOTING = 160,

		UNKNOWN = 255,
	};

	[[nodiscard]] int errorToExitCode(DWORD error);
	[[nodiscard]] const char* errorToExitCodeTag(UINT32 error);
	[[nodiscard]] const char* exitCodeToTag(int code);
} // namespace sdl::error
