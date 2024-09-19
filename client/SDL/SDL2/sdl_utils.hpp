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

#include <SDL.h>
#include <string>
#include <vector>

class CriticalSection
{
  public:
	CriticalSection();
	CriticalSection(const CriticalSection& other) = delete;
	CriticalSection(CriticalSection&& other) = delete;
	~CriticalSection();

	CriticalSection& operator=(const CriticalSection& other) = delete;
	CriticalSection& operator=(const CriticalSection&& other) = delete;

	void lock();
	void unlock();

  private:
	CRITICAL_SECTION _section{};
};

class WinPREvent
{
  public:
	explicit WinPREvent(bool initial = false);
	~WinPREvent();

	void set();
	void clear();
	[[nodiscard]] bool isSet() const;

	[[nodiscard]] HANDLE handle() const;

  private:
	HANDLE _handle;
};

enum
{
	SDL_USEREVENT_UPDATE = SDL_USEREVENT + 1,
	SDL_USEREVENT_CREATE_WINDOWS,
	SDL_USEREVENT_WINDOW_RESIZEABLE,
	SDL_USEREVENT_WINDOW_FULLSCREEN,
	SDL_USEREVENT_WINDOW_MINIMIZE,
	SDL_USEREVENT_POINTER_NULL,
	SDL_USEREVENT_POINTER_DEFAULT,
	SDL_USEREVENT_POINTER_POSITION,
	SDL_USEREVENT_POINTER_SET,
	SDL_USEREVENT_QUIT,
	SDL_USEREVENT_CERT_DIALOG,
	SDL_USEREVENT_SHOW_DIALOG,
	SDL_USEREVENT_AUTH_DIALOG,
	SDL_USEREVENT_SCARD_DIALOG,
	SDL_USEREVENT_RETRY_DIALOG,

	SDL_USEREVENT_CERT_RESULT,
	SDL_USEREVENT_SHOW_RESULT,
	SDL_USEREVENT_AUTH_RESULT,
	SDL_USEREVENT_SCARD_RESULT
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

BOOL sdl_push_user_event(Uint32 type, ...);

bool sdl_push_quit();

std::string sdl_window_event_str(Uint8 ev);
const char* sdl_event_type_str(Uint32 type);
const char* sdl_error_string(Uint32 res);

#define sdl_log_error(res, log, what) sdl_log_error_ex(res, log, what, __FILE__, __LINE__, __func__)
BOOL sdl_log_error_ex(Uint32 res, wLog* log, const char* what, const char* file, size_t line,
                      const char* fkt);
