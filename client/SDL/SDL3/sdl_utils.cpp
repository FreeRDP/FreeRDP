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

#include <cassert>
#include <sstream>
#include <iomanip>
#include <random>

#include "sdl_utils.hpp"

#include "sdl_freerdp.hpp"

#include <SDL3/SDL.h>

#include <freerdp/version.h>
#include <freerdp/utils/string.h>

#define STR(x) #x
#define EV_CASE_STR(x) \
	case x:            \
		return STR(x)

const char* sdl_error_string(Sint32 res)
{
	if (res == 0)
		return nullptr;

	return SDL_GetError();
}

BOOL sdl_log_error_ex(Sint32 res, wLog* log, const char* what, const char* file, size_t line,
                      const char* fkt)
{
	const char* msg = sdl_error_string(res);

	WINPR_UNUSED(file);

	if (!msg)
		return FALSE;

	WLog_Print(log, WLOG_ERROR, "[%s:%" PRIuz "][%s]: %s", fkt, line, what, msg);
	return TRUE;
}

bool sdl_push_user_event(Uint32 type, ...)
{
	SDL_Event ev = {};
	SDL_UserEvent* event = &ev.user;

	va_list ap = {};
	va_start(ap, type);
	event->type = type;
	switch (type)
	{
		case SDL_EVENT_USER_AUTH_RESULT:
		{
			auto arg = reinterpret_cast<SDL_UserAuthArg*>(ev.padding);
			arg->user = va_arg(ap, char*);
			arg->domain = va_arg(ap, char*);
			arg->password = va_arg(ap, char*);
			arg->result = va_arg(ap, Sint32);
		}
		break;
		case SDL_EVENT_USER_AUTH_DIALOG:
		{
			auto arg = reinterpret_cast<SDL_UserAuthArg*>(ev.padding);

			arg->title = va_arg(ap, char*);
			arg->user = va_arg(ap, char*);
			arg->domain = va_arg(ap, char*);
			arg->password = va_arg(ap, char*);
			arg->result = va_arg(ap, Sint32);
		}
		break;
		case SDL_EVENT_USER_SCARD_DIALOG:
		{
			event->data1 = va_arg(ap, char*);
			event->data2 = va_arg(ap, char**);
			event->code = va_arg(ap, Sint32);
		}
		break;
		case SDL_EVENT_USER_RETRY_DIALOG:
			event->code = va_arg(ap, Sint32);
			break;
		case SDL_EVENT_USER_SCARD_RESULT:
		case SDL_EVENT_USER_SHOW_RESULT:
		case SDL_EVENT_USER_CERT_RESULT:
			event->code = va_arg(ap, Sint32);
			break;

		case SDL_EVENT_USER_SHOW_DIALOG:
			event->data1 = va_arg(ap, char*);
			event->data2 = va_arg(ap, char*);
			event->code = va_arg(ap, Sint32);
			break;
		case SDL_EVENT_USER_CERT_DIALOG:
			event->data1 = va_arg(ap, char*);
			event->data2 = va_arg(ap, char*);
			break;
		case SDL_EVENT_USER_UPDATE:
			event->data1 = va_arg(ap, void*);
			break;
		case SDL_EVENT_USER_POINTER_POSITION:
			event->data1 = reinterpret_cast<void*>(static_cast<uintptr_t>(va_arg(ap, UINT32)));
			event->data2 = reinterpret_cast<void*>(static_cast<uintptr_t>(va_arg(ap, UINT32)));
			break;
		case SDL_EVENT_USER_POINTER_SET:
			event->data1 = va_arg(ap, void*);
			event->data2 = va_arg(ap, void*);
			break;
		case SDL_EVENT_USER_CREATE_WINDOWS:
			event->data1 = va_arg(ap, void*);
			break;
		case SDL_EVENT_USER_WINDOW_FULLSCREEN:
			event->data1 = va_arg(ap, void*);
			event->code = va_arg(ap, int);
			event->data2 = reinterpret_cast<void*>(static_cast<uintptr_t>(va_arg(ap, int)));
			break;
		case SDL_EVENT_USER_WINDOW_RESIZEABLE:
			event->data1 = va_arg(ap, void*);
			event->code = va_arg(ap, int);
			break;
		case SDL_EVENT_USER_WINDOW_MINIMIZE:
		case SDL_EVENT_USER_QUIT:
		case SDL_EVENT_USER_POINTER_NULL:
		case SDL_EVENT_USER_POINTER_DEFAULT:
		case SDL_EVENT_CLIPBOARD_UPDATE:
			break;
		default:
			va_end(ap);
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[%s] unsupported type %u", __func__, type);
			return false;
	}
	va_end(ap);
	const auto rc = SDL_PushEvent(&ev);
	if (rc != 1)
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_PushEvent returned %d", __func__, rc);
	return rc == 1;
}

bool sdl_push_quit()
{
	SDL_Event ev = {};
	ev.type = SDL_EVENT_QUIT;
	SDL_PushEvent(&ev);
	return true;
}

namespace sdl::utils
{
	UINT32 orientaion_to_rdp(SDL_DisplayOrientation orientation)
	{
		switch (orientation)
		{
			case SDL_ORIENTATION_LANDSCAPE:
				return ORIENTATION_LANDSCAPE;
			case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
				return ORIENTATION_LANDSCAPE_FLIPPED;
			case SDL_ORIENTATION_PORTRAIT_FLIPPED:
				return ORIENTATION_PORTRAIT_FLIPPED;
			case SDL_ORIENTATION_PORTRAIT:
			default:
				return ORIENTATION_PORTRAIT;
		}
	}

	std::string touchFlagsToString(Uint32 flags)
	{
		std::stringstream ss;
		ss << "{";
		bool first = true;
		for (size_t x = 0; x < 32; x++)
		{
			const Uint32 mask = 1u << x;
			if (flags & mask)
			{
				if (!first)
					ss << "|";
				first = false;
				ss << freerdp_input_touch_state_string(mask);
			}
		}
		ss << "}";
		return ss.str();
	}

	std::string toString(SDL_DisplayOrientation orientation)
	{
		switch (orientation)
		{
			case SDL_ORIENTATION_LANDSCAPE:
				return "SDL_ORIENTATION_LANDSCAPE";
			case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
				return "SDL_ORIENTATION_LANDSCAPE_FLIPPED";
			case SDL_ORIENTATION_PORTRAIT_FLIPPED:
				return "SDL_ORIENTATION_PORTRAIT_FLIPPED";
			case SDL_ORIENTATION_PORTRAIT:
				return "SDL_ORIENTATION_PORTRAIT";
			default:
			{
				std::stringstream ss;
				ss << "SDL_ORIENTATION_UNKNOWN[0x" << std::hex << std::setw(8) << std::setfill('0')
				   << orientation << "]";
				return ss.str();
			}
		}
	}

	std::string toString(FreeRDP_DesktopRotationFlags orientation)
	{
		return freerdp_desktop_rotation_flags_to_string(orientation);
	}

	std::string toString(const SDL_DisplayMode* mode)
	{
		if (!mode)
			return "SDL_DisplayMode=null";

		std::stringstream ss;

		ss << "["
		   << "id=" << mode->displayID << ","
		   << "fmt=" << mode->format << ","
		   << "w=" << mode->w << ","
		   << "h=" << mode->h << ","
		   << "dpi=" << mode->pixel_density << ","
		   << "refresh=" << mode->refresh_rate << ","
		   << "num=" << mode->refresh_rate_numerator << ","
		   << "denom=" << mode->refresh_rate_denominator << "]";

		return ss.str();
	}

	std::string toString(Uint32 type)
	{
		switch (type)
		{
			EV_CASE_STR(SDL_EVENT_FIRST);
			EV_CASE_STR(SDL_EVENT_QUIT);
			EV_CASE_STR(SDL_EVENT_TERMINATING);
			EV_CASE_STR(SDL_EVENT_LOW_MEMORY);
			EV_CASE_STR(SDL_EVENT_WILL_ENTER_BACKGROUND);
			EV_CASE_STR(SDL_EVENT_DID_ENTER_BACKGROUND);
			EV_CASE_STR(SDL_EVENT_WILL_ENTER_FOREGROUND);
			EV_CASE_STR(SDL_EVENT_DID_ENTER_FOREGROUND);
			EV_CASE_STR(SDL_EVENT_LOCALE_CHANGED);
			EV_CASE_STR(SDL_EVENT_SYSTEM_THEME_CHANGED);
			EV_CASE_STR(SDL_EVENT_DISPLAY_ORIENTATION);
			EV_CASE_STR(SDL_EVENT_DISPLAY_ADDED);
			EV_CASE_STR(SDL_EVENT_DISPLAY_REMOVED);
			EV_CASE_STR(SDL_EVENT_DISPLAY_MOVED);
			EV_CASE_STR(SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED);
			EV_CASE_STR(SDL_EVENT_WINDOW_SHOWN);
			EV_CASE_STR(SDL_EVENT_WINDOW_HIDDEN);
			EV_CASE_STR(SDL_EVENT_WINDOW_EXPOSED);
			EV_CASE_STR(SDL_EVENT_WINDOW_MOVED);
			EV_CASE_STR(SDL_EVENT_WINDOW_RESIZED);
			EV_CASE_STR(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
			EV_CASE_STR(SDL_EVENT_WINDOW_MINIMIZED);
			EV_CASE_STR(SDL_EVENT_WINDOW_MAXIMIZED);
			EV_CASE_STR(SDL_EVENT_WINDOW_RESTORED);
			EV_CASE_STR(SDL_EVENT_WINDOW_MOUSE_ENTER);
			EV_CASE_STR(SDL_EVENT_WINDOW_MOUSE_LEAVE);
			EV_CASE_STR(SDL_EVENT_WINDOW_FOCUS_GAINED);
			EV_CASE_STR(SDL_EVENT_WINDOW_FOCUS_LOST);
			EV_CASE_STR(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
			EV_CASE_STR(SDL_EVENT_WINDOW_HIT_TEST);
			EV_CASE_STR(SDL_EVENT_WINDOW_ICCPROF_CHANGED);
			EV_CASE_STR(SDL_EVENT_WINDOW_DISPLAY_CHANGED);
			EV_CASE_STR(SDL_EVENT_WINDOW_SAFE_AREA_CHANGED);
			EV_CASE_STR(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED);
			EV_CASE_STR(SDL_EVENT_WINDOW_OCCLUDED);
			EV_CASE_STR(SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
			EV_CASE_STR(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN);
			EV_CASE_STR(SDL_EVENT_WINDOW_DESTROYED);

			EV_CASE_STR(SDL_EVENT_KEY_DOWN);
			EV_CASE_STR(SDL_EVENT_KEY_UP);
			EV_CASE_STR(SDL_EVENT_TEXT_EDITING);
			EV_CASE_STR(SDL_EVENT_TEXT_INPUT);
			EV_CASE_STR(SDL_EVENT_KEYMAP_CHANGED);
			EV_CASE_STR(SDL_EVENT_KEYBOARD_ADDED);
			EV_CASE_STR(SDL_EVENT_KEYBOARD_REMOVED);

			EV_CASE_STR(SDL_EVENT_MOUSE_MOTION);
			EV_CASE_STR(SDL_EVENT_MOUSE_BUTTON_DOWN);
			EV_CASE_STR(SDL_EVENT_MOUSE_BUTTON_UP);
			EV_CASE_STR(SDL_EVENT_MOUSE_WHEEL);
			EV_CASE_STR(SDL_EVENT_MOUSE_ADDED);
			EV_CASE_STR(SDL_EVENT_MOUSE_REMOVED);

			EV_CASE_STR(SDL_EVENT_JOYSTICK_AXIS_MOTION);
			EV_CASE_STR(SDL_EVENT_JOYSTICK_BALL_MOTION);
			EV_CASE_STR(SDL_EVENT_JOYSTICK_HAT_MOTION);
			EV_CASE_STR(SDL_EVENT_JOYSTICK_BUTTON_DOWN);
			EV_CASE_STR(SDL_EVENT_JOYSTICK_BUTTON_UP);
			EV_CASE_STR(SDL_EVENT_JOYSTICK_ADDED);
			EV_CASE_STR(SDL_EVENT_JOYSTICK_REMOVED);
			EV_CASE_STR(SDL_EVENT_JOYSTICK_BATTERY_UPDATED);
			EV_CASE_STR(SDL_EVENT_JOYSTICK_UPDATE_COMPLETE);

			EV_CASE_STR(SDL_EVENT_GAMEPAD_AXIS_MOTION);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_BUTTON_DOWN);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_BUTTON_UP);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_ADDED);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_REMOVED);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_REMAPPED);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_TOUCHPAD_UP);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_SENSOR_UPDATE);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_UPDATE_COMPLETE);
			EV_CASE_STR(SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED);

			EV_CASE_STR(SDL_EVENT_FINGER_DOWN);
			EV_CASE_STR(SDL_EVENT_FINGER_UP);
			EV_CASE_STR(SDL_EVENT_FINGER_MOTION);

			EV_CASE_STR(SDL_EVENT_CLIPBOARD_UPDATE);

			EV_CASE_STR(SDL_EVENT_DROP_FILE);
			EV_CASE_STR(SDL_EVENT_DROP_TEXT);
			EV_CASE_STR(SDL_EVENT_DROP_BEGIN);
			EV_CASE_STR(SDL_EVENT_DROP_COMPLETE);
			EV_CASE_STR(SDL_EVENT_DROP_POSITION);

			EV_CASE_STR(SDL_EVENT_AUDIO_DEVICE_ADDED);
			EV_CASE_STR(SDL_EVENT_AUDIO_DEVICE_REMOVED);
			EV_CASE_STR(SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED);

			EV_CASE_STR(SDL_EVENT_SENSOR_UPDATE);

			EV_CASE_STR(SDL_EVENT_PEN_DOWN);
			EV_CASE_STR(SDL_EVENT_PEN_UP);
			EV_CASE_STR(SDL_EVENT_PEN_MOTION);
			EV_CASE_STR(SDL_EVENT_PEN_BUTTON_DOWN);
			EV_CASE_STR(SDL_EVENT_PEN_BUTTON_UP);
			EV_CASE_STR(SDL_EVENT_CAMERA_DEVICE_ADDED);
			EV_CASE_STR(SDL_EVENT_CAMERA_DEVICE_REMOVED);
			EV_CASE_STR(SDL_EVENT_CAMERA_DEVICE_APPROVED);
			EV_CASE_STR(SDL_EVENT_CAMERA_DEVICE_DENIED);

			EV_CASE_STR(SDL_EVENT_RENDER_TARGETS_RESET);
			EV_CASE_STR(SDL_EVENT_RENDER_DEVICE_RESET);
			EV_CASE_STR(SDL_EVENT_POLL_SENTINEL);

			EV_CASE_STR(SDL_EVENT_USER);

			EV_CASE_STR(SDL_EVENT_USER_CERT_DIALOG);
			EV_CASE_STR(SDL_EVENT_USER_CERT_RESULT);
			EV_CASE_STR(SDL_EVENT_USER_SHOW_DIALOG);
			EV_CASE_STR(SDL_EVENT_USER_SHOW_RESULT);
			EV_CASE_STR(SDL_EVENT_USER_AUTH_DIALOG);
			EV_CASE_STR(SDL_EVENT_USER_AUTH_RESULT);
			EV_CASE_STR(SDL_EVENT_USER_SCARD_DIALOG);
			EV_CASE_STR(SDL_EVENT_USER_RETRY_DIALOG);
			EV_CASE_STR(SDL_EVENT_USER_SCARD_RESULT);
			EV_CASE_STR(SDL_EVENT_USER_UPDATE);
			EV_CASE_STR(SDL_EVENT_USER_CREATE_WINDOWS);
			EV_CASE_STR(SDL_EVENT_USER_WINDOW_RESIZEABLE);
			EV_CASE_STR(SDL_EVENT_USER_WINDOW_FULLSCREEN);
			EV_CASE_STR(SDL_EVENT_USER_WINDOW_MINIMIZE);
			EV_CASE_STR(SDL_EVENT_USER_POINTER_NULL);
			EV_CASE_STR(SDL_EVENT_USER_POINTER_DEFAULT);
			EV_CASE_STR(SDL_EVENT_USER_POINTER_POSITION);
			EV_CASE_STR(SDL_EVENT_USER_POINTER_SET);
			EV_CASE_STR(SDL_EVENT_USER_QUIT);

			EV_CASE_STR(SDL_EVENT_LAST);
			default:
			{
				std::stringstream ss;
				ss << "SDL_UNKNOWNEVENT[0x" << std::hex << std::setw(8) << std::setfill('0') << type
				   << "]";
				return ss.str();
			}
		}
	}

	std::string generate_uuid_v4()
	{
		static std::random_device rd;
		static std::mt19937 gen(rd());
		static std::uniform_int_distribution<> dis(0, 255);
		std::stringstream ss;
		ss << std::hex << std::setfill('0') << std::setw(2);
		for (int i = 0; i < 4; i++)
		{
			ss << dis(gen);
		}
		ss << "-";
		for (int i = 0; i < 2; i++)
		{
			ss << dis(gen);
		}
		ss << "-";
		for (int i = 0; i < 2; i++)
		{
			ss << dis(gen);
		}
		ss << "-";
		for (int i = 0; i < 2; i++)
		{
			ss << dis(gen);
		}
		ss << "-";
		for (int i = 0; i < 6; i++)
		{
			ss << dis(gen);
		}
		return ss.str();
	}

	HighDpiScaleMode platformScaleMode()
	{
		const auto platform = SDL_GetPlatform();
		if (!platform)
			return SCALE_MODE_INVALID;
		if (strcmp("Windows", platform) == 0)
			return SCALE_MODE_X11;
		if (strcmp("macOS", platform) == 0)
			return SCALE_MODE_WAYLAND;
		if (strcmp("Linux", platform) == 0)
		{
			const auto driver = SDL_GetCurrentVideoDriver();
			if (!driver)
				return SCALE_MODE_WAYLAND;
			if (strcmp("x11", driver) == 0)
				return SCALE_MODE_X11;
			if (strcmp("wayland", driver) == 0)
				return SCALE_MODE_WAYLAND;
		}
		return SCALE_MODE_INVALID;
	}

	std::string windowTitle(const rdpSettings* settings)
	{
		const char* prefix = "FreeRDP:";

		if (!settings)
			return {};

		const auto windowTitle = freerdp_settings_get_string(settings, FreeRDP_WindowTitle);
		if (windowTitle)
			return {};

		const auto name = freerdp_settings_get_server_name(settings);
		const auto port = freerdp_settings_get_uint32(settings, FreeRDP_ServerPort);

		const auto addPort = (port != 3389);

		std::stringstream ss;
		ss << prefix << " ";
		if (!addPort)
			ss << name;
		else
			ss << name << ":" << port;

		return ss.str();
	}

	std::string toString(SDL_Rect rect)
	{
		std::stringstream ss;
		ss << "SDL_Rect{" << rect.x << "x" << rect.y << "-" << rect.w << "x" << rect.h << "}";
		return ss.str();
	}

	std::string toString(SDL_FRect rect)
	{
		std::stringstream ss;
		ss << "SDL_Rect{" << rect.x << "x" << rect.y << "-" << rect.w << "x" << rect.h << "}";
		return ss.str();
	}

} // namespace sdl::utils

namespace sdl::error
{
	struct sdl_exitCode_map_t
	{
		DWORD error;
		int code;
		const char* code_tag;
	};

#define ENTRY(x, y) { x, y, #y }
	static const struct sdl_exitCode_map_t sdl_exitCode_map[] = {
		ENTRY(FREERDP_ERROR_SUCCESS, SUCCESS), ENTRY(FREERDP_ERROR_NONE, DISCONNECT),
		ENTRY(FREERDP_ERROR_NONE, LOGOFF), ENTRY(FREERDP_ERROR_NONE, IDLE_TIMEOUT),
		ENTRY(FREERDP_ERROR_NONE, LOGON_TIMEOUT), ENTRY(FREERDP_ERROR_NONE, CONN_REPLACED),
		ENTRY(FREERDP_ERROR_NONE, OUT_OF_MEMORY), ENTRY(FREERDP_ERROR_NONE, CONN_DENIED),
		ENTRY(FREERDP_ERROR_NONE, CONN_DENIED_FIPS), ENTRY(FREERDP_ERROR_NONE, USER_PRIVILEGES),
		ENTRY(FREERDP_ERROR_NONE, FRESH_CREDENTIALS_REQUIRED),
		ENTRY(ERRINFO_LOGOFF_BY_USER, DISCONNECT_BY_USER), ENTRY(FREERDP_ERROR_NONE, UNKNOWN),

		/* section 16-31: license error set */
		ENTRY(FREERDP_ERROR_NONE, LICENSE_INTERNAL),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_NO_LICENSE_SERVER),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_NO_LICENSE),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_BAD_CLIENT_MSG),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_HWID_DOESNT_MATCH),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_BAD_CLIENT),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_CANT_FINISH_PROTOCOL),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_CLIENT_ENDED_PROTOCOL),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_BAD_CLIENT_ENCRYPTION),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_CANT_UPGRADE),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_NO_REMOTE_CONNECTIONS),
		ENTRY(FREERDP_ERROR_NONE, LICENSE_CANT_UPGRADE),

		/* section 32-127: RDP protocol error set */
		ENTRY(FREERDP_ERROR_NONE, RDP),

		/* section 128-254: xfreerdp specific exit codes */
		ENTRY(FREERDP_ERROR_NONE, PARSE_ARGUMENTS), ENTRY(FREERDP_ERROR_NONE, MEMORY),
		ENTRY(FREERDP_ERROR_NONE, PROTOCOL), ENTRY(FREERDP_ERROR_NONE, CONN_FAILED),

		ENTRY(FREERDP_ERROR_AUTHENTICATION_FAILED, AUTH_FAILURE),
		ENTRY(FREERDP_ERROR_SECURITY_NEGO_CONNECT_FAILED, NEGO_FAILURE),
		ENTRY(FREERDP_ERROR_CONNECT_LOGON_FAILURE, LOGON_FAILURE),
		ENTRY(FREERDP_ERROR_CONNECT_TARGET_BOOTING, CONNECT_TARGET_BOOTING),
		ENTRY(FREERDP_ERROR_CONNECT_ACCOUNT_LOCKED_OUT, ACCOUNT_LOCKED_OUT),
		ENTRY(FREERDP_ERROR_PRE_CONNECT_FAILED, PRE_CONNECT_FAILED),
		ENTRY(FREERDP_ERROR_CONNECT_UNDEFINED, CONNECT_UNDEFINED),
		ENTRY(FREERDP_ERROR_POST_CONNECT_FAILED, POST_CONNECT_FAILED),
		ENTRY(FREERDP_ERROR_DNS_ERROR, DNS_ERROR),
		ENTRY(FREERDP_ERROR_DNS_NAME_NOT_FOUND, DNS_NAME_NOT_FOUND),
		ENTRY(FREERDP_ERROR_CONNECT_FAILED, CONNECT_FAILED),
		ENTRY(FREERDP_ERROR_MCS_CONNECT_INITIAL_ERROR, MCS_CONNECT_INITIAL_ERROR),
		ENTRY(FREERDP_ERROR_TLS_CONNECT_FAILED, TLS_CONNECT_FAILED),
		ENTRY(FREERDP_ERROR_INSUFFICIENT_PRIVILEGES, INSUFFICIENT_PRIVILEGES),
		ENTRY(FREERDP_ERROR_CONNECT_CANCELLED, CONNECT_CANCELLED),
		ENTRY(FREERDP_ERROR_CONNECT_TRANSPORT_FAILED, CONNECT_TRANSPORT_FAILED),
		ENTRY(FREERDP_ERROR_CONNECT_PASSWORD_EXPIRED, CONNECT_PASSWORD_EXPIRED),
		ENTRY(FREERDP_ERROR_CONNECT_PASSWORD_MUST_CHANGE, CONNECT_PASSWORD_MUST_CHANGE),
		ENTRY(FREERDP_ERROR_CONNECT_KDC_UNREACHABLE, CONNECT_KDC_UNREACHABLE),
		ENTRY(FREERDP_ERROR_CONNECT_ACCOUNT_DISABLED, CONNECT_ACCOUNT_DISABLED),
		ENTRY(FREERDP_ERROR_CONNECT_PASSWORD_CERTAINLY_EXPIRED, CONNECT_PASSWORD_CERTAINLY_EXPIRED),
		ENTRY(FREERDP_ERROR_CONNECT_CLIENT_REVOKED, CONNECT_CLIENT_REVOKED),
		ENTRY(FREERDP_ERROR_CONNECT_WRONG_PASSWORD, CONNECT_WRONG_PASSWORD),
		ENTRY(FREERDP_ERROR_CONNECT_ACCESS_DENIED, CONNECT_ACCESS_DENIED),
		ENTRY(FREERDP_ERROR_CONNECT_ACCOUNT_RESTRICTION, CONNECT_ACCOUNT_RESTRICTION),
		ENTRY(FREERDP_ERROR_CONNECT_ACCOUNT_EXPIRED, CONNECT_ACCOUNT_EXPIRED),
		ENTRY(FREERDP_ERROR_CONNECT_LOGON_TYPE_NOT_GRANTED, CONNECT_LOGON_TYPE_NOT_GRANTED),
		ENTRY(FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS, CONNECT_NO_OR_MISSING_CREDENTIALS)
	};

	static const sdl_exitCode_map_t* mapEntryByCode(int exit_code)
	{
		for (const auto& x : sdl_exitCode_map)
		{
			auto cur = &x;
			if (cur->code == exit_code)
				return cur;
		}
		return nullptr;
	}

	static const sdl_exitCode_map_t* mapEntryByError(UINT32 error)
	{
		for (const auto& x : sdl_exitCode_map)
		{
			auto cur = &x;
			if (cur->error == error)
				return cur;
		}
		return nullptr;
	}

	int errorToExitCode(DWORD error)
	{
		auto entry = mapEntryByError(error);
		if (entry)
			return entry->code;

		return CONN_FAILED;
	}

	const char* errorToExitCodeTag(UINT32 error)
	{
		auto entry = mapEntryByError(error);
		if (entry)
			return entry->code_tag;
		return nullptr;
	}

	const char* exitCodeToTag(int code)
	{
		auto entry = mapEntryByCode(code);
		if (entry)
			return entry->code_tag;
		return nullptr;
	}
} // namespace sdl::error
