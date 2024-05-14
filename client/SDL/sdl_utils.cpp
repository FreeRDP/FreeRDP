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

#include <fstream>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error Could not find system header "<filesystem>" or "<experimental/filesystem>"
#endif

#include <cassert>
#include "sdl_utils.hpp"

#include "sdl_freerdp.hpp"

#include <SDL.h>

#include <winpr/path.h>
#include <winpr/config.h>
#include <freerdp/version.h>
#include <winpr/json.h>

const char* sdl_event_type_str(Uint32 type)
{
#define STR(x) #x
#define EV_CASE_STR(x) \
	case x:            \
		return STR(x)

	switch (type)
	{
		EV_CASE_STR(SDL_FIRSTEVENT);
		EV_CASE_STR(SDL_QUIT);
		EV_CASE_STR(SDL_APP_TERMINATING);
		EV_CASE_STR(SDL_APP_LOWMEMORY);
		EV_CASE_STR(SDL_APP_WILLENTERBACKGROUND);
		EV_CASE_STR(SDL_APP_DIDENTERBACKGROUND);
		EV_CASE_STR(SDL_APP_WILLENTERFOREGROUND);
		EV_CASE_STR(SDL_APP_DIDENTERFOREGROUND);
#if SDL_VERSION_ATLEAST(2, 0, 10)
		EV_CASE_STR(SDL_DISPLAYEVENT);
#endif
		EV_CASE_STR(SDL_WINDOWEVENT);
		EV_CASE_STR(SDL_SYSWMEVENT);
		EV_CASE_STR(SDL_KEYDOWN);
		EV_CASE_STR(SDL_KEYUP);
		EV_CASE_STR(SDL_TEXTEDITING);
		EV_CASE_STR(SDL_TEXTINPUT);
		EV_CASE_STR(SDL_KEYMAPCHANGED);
		EV_CASE_STR(SDL_MOUSEMOTION);
		EV_CASE_STR(SDL_MOUSEBUTTONDOWN);
		EV_CASE_STR(SDL_MOUSEBUTTONUP);
		EV_CASE_STR(SDL_MOUSEWHEEL);
		EV_CASE_STR(SDL_JOYAXISMOTION);
		EV_CASE_STR(SDL_JOYBALLMOTION);
		EV_CASE_STR(SDL_JOYHATMOTION);
		EV_CASE_STR(SDL_JOYBUTTONDOWN);
		EV_CASE_STR(SDL_JOYBUTTONUP);
		EV_CASE_STR(SDL_JOYDEVICEADDED);
		EV_CASE_STR(SDL_JOYDEVICEREMOVED);
		EV_CASE_STR(SDL_CONTROLLERAXISMOTION);
		EV_CASE_STR(SDL_CONTROLLERBUTTONDOWN);
		EV_CASE_STR(SDL_CONTROLLERBUTTONUP);
		EV_CASE_STR(SDL_CONTROLLERDEVICEADDED);
		EV_CASE_STR(SDL_CONTROLLERDEVICEREMOVED);
		EV_CASE_STR(SDL_CONTROLLERDEVICEREMAPPED);
#if SDL_VERSION_ATLEAST(2, 0, 14)
		EV_CASE_STR(SDL_LOCALECHANGED);
		EV_CASE_STR(SDL_CONTROLLERTOUCHPADDOWN);
		EV_CASE_STR(SDL_CONTROLLERTOUCHPADMOTION);
		EV_CASE_STR(SDL_CONTROLLERTOUCHPADUP);
		EV_CASE_STR(SDL_CONTROLLERSENSORUPDATE);
#endif
		EV_CASE_STR(SDL_FINGERDOWN);
		EV_CASE_STR(SDL_FINGERUP);
		EV_CASE_STR(SDL_FINGERMOTION);
		EV_CASE_STR(SDL_DOLLARGESTURE);
		EV_CASE_STR(SDL_DOLLARRECORD);
		EV_CASE_STR(SDL_MULTIGESTURE);
		EV_CASE_STR(SDL_CLIPBOARDUPDATE);
		EV_CASE_STR(SDL_DROPFILE);
		EV_CASE_STR(SDL_DROPTEXT);
		EV_CASE_STR(SDL_DROPBEGIN);
		EV_CASE_STR(SDL_DROPCOMPLETE);
		EV_CASE_STR(SDL_AUDIODEVICEADDED);
		EV_CASE_STR(SDL_AUDIODEVICEREMOVED);
#if SDL_VERSION_ATLEAST(2, 0, 9)
		EV_CASE_STR(SDL_SENSORUPDATE);
#endif
		EV_CASE_STR(SDL_RENDER_TARGETS_RESET);
		EV_CASE_STR(SDL_RENDER_DEVICE_RESET);
		EV_CASE_STR(SDL_USEREVENT);

		EV_CASE_STR(SDL_USEREVENT_CERT_DIALOG);
		EV_CASE_STR(SDL_USEREVENT_CERT_RESULT);
		EV_CASE_STR(SDL_USEREVENT_SHOW_DIALOG);
		EV_CASE_STR(SDL_USEREVENT_SHOW_RESULT);
		EV_CASE_STR(SDL_USEREVENT_AUTH_DIALOG);
		EV_CASE_STR(SDL_USEREVENT_AUTH_RESULT);
		EV_CASE_STR(SDL_USEREVENT_SCARD_DIALOG);
		EV_CASE_STR(SDL_USEREVENT_RETRY_DIALOG);
		EV_CASE_STR(SDL_USEREVENT_SCARD_RESULT);
		EV_CASE_STR(SDL_USEREVENT_UPDATE);
		EV_CASE_STR(SDL_USEREVENT_CREATE_WINDOWS);
		EV_CASE_STR(SDL_USEREVENT_WINDOW_RESIZEABLE);
		EV_CASE_STR(SDL_USEREVENT_WINDOW_FULLSCREEN);
		EV_CASE_STR(SDL_USEREVENT_POINTER_NULL);
		EV_CASE_STR(SDL_USEREVENT_POINTER_DEFAULT);
		EV_CASE_STR(SDL_USEREVENT_POINTER_POSITION);
		EV_CASE_STR(SDL_USEREVENT_POINTER_SET);
		EV_CASE_STR(SDL_USEREVENT_QUIT);

		EV_CASE_STR(SDL_LASTEVENT);
		default:
			return "SDL_UNKNOWNEVENT";
	}
#undef EV_CASE_STR
#undef STR
}

const char* sdl_error_string(Uint32 res)
{
	if (res == 0)
		return nullptr;

	return SDL_GetError();
}

BOOL sdl_log_error_ex(Uint32 res, wLog* log, const char* what, const char* file, size_t line,
                      const char* fkt)
{
	const char* msg = sdl_error_string(res);

	WINPR_UNUSED(file);

	if (!msg)
		return FALSE;

	WLog_Print(log, WLOG_ERROR, "[%s:%" PRIuz "][%s]: %s", fkt, line, what, msg);
	return TRUE;
}

BOOL sdl_push_user_event(Uint32 type, ...)
{
	SDL_Event ev = {};
	SDL_UserEvent* event = &ev.user;

	va_list ap;
	va_start(ap, type);
	event->type = type;
	switch (type)
	{
		case SDL_USEREVENT_AUTH_RESULT:
		{
			auto arg = reinterpret_cast<SDL_UserAuthArg*>(ev.padding);
			arg->user = va_arg(ap, char*);
			arg->domain = va_arg(ap, char*);
			arg->password = va_arg(ap, char*);
			arg->result = va_arg(ap, Sint32);
		}
		break;
		case SDL_USEREVENT_AUTH_DIALOG:
		{
			auto arg = reinterpret_cast<SDL_UserAuthArg*>(ev.padding);

			arg->title = va_arg(ap, char*);
			arg->user = va_arg(ap, char*);
			arg->domain = va_arg(ap, char*);
			arg->password = va_arg(ap, char*);
			arg->result = va_arg(ap, Sint32);
		}
		break;
		case SDL_USEREVENT_SCARD_DIALOG:
		{
			event->data1 = va_arg(ap, char*);
			event->data2 = va_arg(ap, char**);
			event->code = va_arg(ap, Sint32);
		}
		break;
		case SDL_USEREVENT_RETRY_DIALOG:
			break;
		case SDL_USEREVENT_SCARD_RESULT:
		case SDL_USEREVENT_SHOW_RESULT:
		case SDL_USEREVENT_CERT_RESULT:
			event->code = va_arg(ap, Sint32);
			break;

		case SDL_USEREVENT_SHOW_DIALOG:
			event->data1 = va_arg(ap, char*);
			event->data2 = va_arg(ap, char*);
			event->code = va_arg(ap, Sint32);
			break;
		case SDL_USEREVENT_CERT_DIALOG:
			event->data1 = va_arg(ap, char*);
			event->data2 = va_arg(ap, char*);
			break;
		case SDL_USEREVENT_UPDATE:
			event->data1 = va_arg(ap, void*);
			break;
		case SDL_USEREVENT_POINTER_POSITION:
			event->data1 = reinterpret_cast<void*>(static_cast<uintptr_t>(va_arg(ap, UINT32)));
			event->data2 = reinterpret_cast<void*>(static_cast<uintptr_t>(va_arg(ap, UINT32)));
			break;
		case SDL_USEREVENT_POINTER_SET:
			event->data1 = va_arg(ap, void*);
			event->data2 = va_arg(ap, void*);
			break;
		case SDL_USEREVENT_CREATE_WINDOWS:
			event->data1 = reinterpret_cast<void*>(va_arg(ap, void*));
			break;
		case SDL_USEREVENT_WINDOW_FULLSCREEN:
		case SDL_USEREVENT_WINDOW_RESIZEABLE:
			event->data1 = va_arg(ap, void*);
			event->code = va_arg(ap, int);
			break;
		case SDL_USEREVENT_QUIT:
		case SDL_USEREVENT_POINTER_NULL:
		case SDL_USEREVENT_POINTER_DEFAULT:
			break;
		default:
			va_end(ap);
			return FALSE;
	}
	va_end(ap);
	return SDL_PushEvent(&ev) == 1;
}

CriticalSection::CriticalSection()
{
	InitializeCriticalSection(&_section);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&_section);
}

void CriticalSection::lock()
{
	EnterCriticalSection(&_section);
}

void CriticalSection::unlock()
{
	LeaveCriticalSection(&_section);
}

WinPREvent::WinPREvent(bool initial)
    : _handle(CreateEventA(nullptr, TRUE, initial ? TRUE : FALSE, nullptr))
{
}

WinPREvent::~WinPREvent()
{
	CloseHandle(_handle);
}

void WinPREvent::set()
{
	SetEvent(_handle);
}

void WinPREvent::clear()
{
	ResetEvent(_handle);
}

bool WinPREvent::isSet() const
{
	return WaitForSingleObject(_handle, 0) == WAIT_OBJECT_0;
}

HANDLE WinPREvent::handle() const
{
	return _handle;
}

bool sdl_push_quit()
{
	SDL_Event ev = { 0 };
	ev.type = SDL_QUIT;
	SDL_PushEvent(&ev);
	return true;
}

std::string sdl_window_event_str(Uint8 ev)
{
	switch (ev)
	{
		case SDL_WINDOWEVENT_NONE:
			return "SDL_WINDOWEVENT_NONE";
		case SDL_WINDOWEVENT_SHOWN:
			return "SDL_WINDOWEVENT_SHOWN";
		case SDL_WINDOWEVENT_HIDDEN:
			return "SDL_WINDOWEVENT_HIDDEN";
		case SDL_WINDOWEVENT_EXPOSED:
			return "SDL_WINDOWEVENT_EXPOSED";
		case SDL_WINDOWEVENT_MOVED:
			return "SDL_WINDOWEVENT_MOVED";
		case SDL_WINDOWEVENT_RESIZED:
			return "SDL_WINDOWEVENT_RESIZED";
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			return "SDL_WINDOWEVENT_SIZE_CHANGED";
		case SDL_WINDOWEVENT_MINIMIZED:
			return "SDL_WINDOWEVENT_MINIMIZED";
		case SDL_WINDOWEVENT_MAXIMIZED:
			return "SDL_WINDOWEVENT_MAXIMIZED";
		case SDL_WINDOWEVENT_RESTORED:
			return "SDL_WINDOWEVENT_RESTORED";
		case SDL_WINDOWEVENT_ENTER:
			return "SDL_WINDOWEVENT_ENTER";
		case SDL_WINDOWEVENT_LEAVE:
			return "SDL_WINDOWEVENT_LEAVE";
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			return "SDL_WINDOWEVENT_FOCUS_GAINED";
		case SDL_WINDOWEVENT_FOCUS_LOST:
			return "SDL_WINDOWEVENT_FOCUS_LOST";
		case SDL_WINDOWEVENT_CLOSE:
			return "SDL_WINDOWEVENT_CLOSE";
#if SDL_VERSION_ATLEAST(2, 0, 5)
		case SDL_WINDOWEVENT_TAKE_FOCUS:
			return "SDL_WINDOWEVENT_TAKE_FOCUS";
		case SDL_WINDOWEVENT_HIT_TEST:
			return "SDL_WINDOWEVENT_HIT_TEST";
#endif
#if SDL_VERSION_ATLEAST(2, 0, 18)
		case SDL_WINDOWEVENT_ICCPROF_CHANGED:
			return "SDL_WINDOWEVENT_ICCPROF_CHANGED";
		case SDL_WINDOWEVENT_DISPLAY_CHANGED:
			return "SDL_WINDOWEVENT_DISPLAY_CHANGED";
#endif
		default:
			return "SDL_WINDOWEVENT_UNKNOWN";
	}
}

#if defined(WINPR_JSON_FOUND)
using WINPR_JSONPtr = std::unique_ptr<WINPR_JSON, decltype(&WINPR_JSON_Delete)>;

static WINPR_JSONPtr get()
{
	auto config = sdl_get_pref_file();

	std::ifstream ifs(config);
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	return { WINPR_JSON_ParseWithLength(content.c_str(), content.size()), WINPR_JSON_Delete };
}

static WINPR_JSON* get_item(const std::string& key)
{
	static WINPR_JSONPtr config{ nullptr, WINPR_JSON_Delete };
	if (!config)
		config = get();
	if (!config)
		return nullptr;
	return WINPR_JSON_GetObjectItem(config.get(), key.c_str());
}

static std::string item_to_str(WINPR_JSON* item, const std::string& fallback = "")
{
	if (!item || !WINPR_JSON_IsString(item))
		return fallback;
	auto str = WINPR_JSON_GetStringValue(item);
	if (!str)
		return {};
	return str;
}
#endif

std::string sdl_get_pref_string(const std::string& key, const std::string& fallback)
{
#if defined(WINPR_JSON_FOUND)
	auto item = get_item(key);
	return item_to_str(item, fallback);
#else
	return fallback;
#endif
}

bool sdl_get_pref_bool(const std::string& key, bool fallback)
{
#if defined(WINPR_JSON_FOUND)
	auto item = get_item(key);
	if (!item || !WINPR_JSON_IsBool(item))
		return fallback;
	return WINPR_JSON_IsTrue(item);
#else
	return fallback;
#endif
}

int64_t sdl_get_pref_int(const std::string& key, int64_t fallback)
{
#if defined(WINPR_JSON_FOUND)
	auto item = get_item(key);
	if (!item || !WINPR_JSON_IsNumber(item))
		return fallback;
	auto val = WINPR_JSON_GetNumberValue(item);
	return static_cast<int64_t>(val);
#else
	return fallback;
#endif
}

std::vector<std::string> sdl_get_pref_array(const std::string& key,
                                            const std::vector<std::string>& fallback)
{
#if defined(WINPR_JSON_FOUND)
	auto item = get_item(key);
	if (!item || !WINPR_JSON_IsArray(item))
		return fallback;

	std::vector<std::string> values;
	for (int x = 0; x < WINPR_JSON_GetArraySize(item); x++)
	{
		auto cur = WINPR_JSON_GetArrayItem(item, x);
		values.push_back(item_to_str(cur));
	}

	return values;
#else
	return fallback;
#endif
}

std::string sdl_get_pref_dir()
{
	using CStringPtr = std::unique_ptr<char, decltype(&free)>;
	CStringPtr path(GetKnownPath(KNOWN_PATH_XDG_CONFIG_HOME), free);
	if (!path)
		return {};

	fs::path config{ path.get() };
	config /= FREERDP_VENDOR;
	config /= FREERDP_PRODUCT;
	return config.string();
}

std::string sdl_get_pref_file()
{
	fs::path config{ sdl_get_pref_dir() };
	config /= "sdl-freerdp.json";
	return config.string();
}
