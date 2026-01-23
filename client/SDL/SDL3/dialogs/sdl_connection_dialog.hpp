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

#include <SDL3/SDL.h>

#include "sdl_buttons.hpp"
#include "sdl_connection_dialog_wrapper.hpp"
#include "sdl_widget.hpp"
#include "sdl_widget_list.hpp"

class SDLConnectionDialog : public SdlWidgetList
{
  public:
	explicit SDLConnectionDialog(rdpContext* context);
	SDLConnectionDialog(const SDLConnectionDialog& other) = delete;
	SDLConnectionDialog(const SDLConnectionDialog&& other) = delete;
	~SDLConnectionDialog() override;

	SDLConnectionDialog& operator=(const SDLConnectionDialog& other) = delete;
	SDLConnectionDialog& operator=(SDLConnectionDialog&& other) = delete;

	[[nodiscard]] bool setTitle(const char* fmt, ...);
	[[nodiscard]] bool showInfo(const char* fmt, ...);
	[[nodiscard]] bool showWarn(const char* fmt, ...);
	[[nodiscard]] bool showError(const char* fmt, ...);

	[[nodiscard]] bool show();
	[[nodiscard]] bool hide();

	[[nodiscard]] bool running() const;
	[[nodiscard]] bool wait(bool ignoreRdpContextQuit = false);

	[[nodiscard]] bool handle(const SDL_Event& event);

	[[nodiscard]] bool visible() const override;

  protected:
	[[nodiscard]] bool updateInternal() override;

  private:
	[[nodiscard]] bool createWindow();
	void destroyWindow();

	[[nodiscard]] bool updateMsg(SdlConnectionDialogWrapper::MsgType type);

	[[nodiscard]] bool setModal();

	[[nodiscard]] bool show(SdlConnectionDialogWrapper::MsgType type, const char* fmt, va_list ap);
	[[nodiscard]] bool show(SdlConnectionDialogWrapper::MsgType type);

	[[nodiscard]] static std::string print(const char* fmt, va_list ap);
	[[nodiscard]] bool setTimer(Uint32 timeoutMS = 15000);
	void resetTimer();

	[[nodiscard]] static Uint32 timeout(void* pvthis, SDL_TimerID timerID, Uint32 intervalMS);

	struct widget_cfg_t
	{
		SDL_Color fgcolor = {};
		SDL_Color bgcolor = {};
		SdlWidget widget;
	};

	rdpContext* _context = nullptr;
	mutable std::mutex _mux;
	std::string _title;
	std::string _msg;
	SdlConnectionDialogWrapper::MsgType _type_active = SdlConnectionDialogWrapper::MSG_NONE;
	SDL_TimerID _timer = 0;
	bool _running = false;
	std::vector<widget_cfg_t> _list;
};
