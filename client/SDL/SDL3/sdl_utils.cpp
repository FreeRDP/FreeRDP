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
#include "sdl_utils.hpp"

#include "sdl_freerdp.hpp"

#include <SDL3/SDL.h>

#include <freerdp/version.h>

#define STR(x) #x
#define EV_CASE_STR(x) \
	case x:            \
		return STR(x)

const char* sdl_event_type_str(Uint32 type)
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
			return "SDL_UNKNOWNEVENT";
	}
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
			event->data1 = reinterpret_cast<void*>(va_arg(ap, void*));
			break;
		case SDL_EVENT_USER_WINDOW_FULLSCREEN:
		case SDL_EVENT_USER_WINDOW_RESIZEABLE:
			event->data1 = va_arg(ap, void*);
			event->code = va_arg(ap, int);
			break;
		case SDL_EVENT_USER_WINDOW_MINIMIZE:
		case SDL_EVENT_USER_QUIT:
		case SDL_EVENT_USER_POINTER_NULL:
		case SDL_EVENT_USER_POINTER_DEFAULT:
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
	(void)CloseHandle(_handle);
}

void WinPREvent::set()
{
	(void)SetEvent(_handle);
}

void WinPREvent::clear()
{
	(void)ResetEvent(_handle);
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
	SDL_Event ev = {};
	ev.type = SDL_EVENT_QUIT;
	SDL_PushEvent(&ev);
	return true;
}

std::string sdl_window_event_str(Uint32 ev)
{
	if ((ev >= SDL_EVENT_WINDOW_FIRST) && (ev <= SDL_EVENT_WINDOW_LAST))
		return sdl_event_type_str(ev);

	return "SDL_EVENT_WINDOW_UNKNOWN";
}
