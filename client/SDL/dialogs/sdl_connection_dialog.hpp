/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
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

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <SDL.h>

#include <freerdp/freerdp.h>

#include "sdl_widget.hpp"
#include "sdl_buttons.hpp"

class SDLConnectionDialog
{
  public:
	SDLConnectionDialog(rdpContext* context);
	SDLConnectionDialog(const SDLConnectionDialog& other) = delete;
	SDLConnectionDialog(const SDLConnectionDialog&& other) = delete;
	virtual ~SDLConnectionDialog();

	bool visible() const;

	bool setTitle(const char* fmt, ...);
	bool showInfo(const char* fmt, ...);
	bool showWarn(const char* fmt, ...);
	bool showError(const char* fmt, ...);

	bool show();
	bool hide();

	bool running() const;
	bool wait(bool ignoreRdpContextQuit = false);

	bool handle(const SDL_Event& event);

  private:
	enum MsgType
	{
		MSG_NONE,
		MSG_INFO,
		MSG_WARN,
		MSG_ERROR,
		MSG_DISCARD
	};

  private:
	bool createWindow();
	void destroyWindow();

	bool update();

	bool setModal();

	bool clearWindow(SDL_Renderer* renderer);

	bool update(SDL_Renderer* renderer);

	bool show(MsgType type, const char* fmt, va_list ap);
	bool show(MsgType type);

	std::string print(const char* fmt, va_list ap);
	bool setTimer(Uint32 timeoutMS = 15000);
	void resetTimer();

  private:
	static Uint32 timeout(Uint32 intervalMS, void* _this);

  private:
	struct widget_cfg_t
	{
		SDL_Color fgcolor;
		SDL_Color bgcolor;
		SdlWidget widget;
	};

  private:
	rdpContext* _context;
	SDL_Window* _window;
	SDL_Renderer* _renderer;
	mutable std::mutex _mux;
	std::string _title;
	std::string _msg;
	MsgType _type = MSG_NONE;
	MsgType _type_active = MSG_NONE;
	SDL_TimerID _timer = -1;
	bool _running = false;
	std::vector<widget_cfg_t> _list;
	SdlButtonList _buttons;
};

class SDLConnectionDialogHider
{
  public:
	SDLConnectionDialogHider(freerdp* instance);
	SDLConnectionDialogHider(rdpContext* context);

	SDLConnectionDialogHider(SDLConnectionDialog* dialog);

	~SDLConnectionDialogHider();

  private:
	SDLConnectionDialog* get(freerdp* instance);
	SDLConnectionDialog* get(rdpContext* context);

  private:
	SDLConnectionDialog* _dialog;
	bool _visible;
};
