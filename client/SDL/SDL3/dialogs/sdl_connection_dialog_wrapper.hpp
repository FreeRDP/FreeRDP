/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include <mutex>
#include <memory>
#include <string>

#include <winpr/platform.h>

#include "sdl_connection_dialog.hpp"

class SdlConnectionDialogWrapper
{
  public:
	SdlConnectionDialogWrapper() = default;
	~SdlConnectionDialogWrapper() = default;

	SdlConnectionDialogWrapper(const SdlConnectionDialogWrapper& other) = delete;
	SdlConnectionDialogWrapper(SdlConnectionDialogWrapper&& other) = delete;

	SdlConnectionDialogWrapper& operator=(const SdlConnectionDialogWrapper& other) = delete;
	SdlConnectionDialogWrapper& operator=(SdlConnectionDialogWrapper&& other) = delete;

	void create(rdpContext* context);
	void destroy();

	bool isRunning() const;
	bool isVisible() const;

	bool handleEvent(const SDL_Event& event);

	WINPR_ATTR_FORMAT_ARG(2, 3)
	void setTitle(WINPR_FORMAT_ARG const char* fmt, ...);
	void setTitle(const std::string& title);

	WINPR_ATTR_FORMAT_ARG(2, 3)
	void showInfo(WINPR_FORMAT_ARG const char* fmt, ...);
	void showInfo(const std::string& info);

	WINPR_ATTR_FORMAT_ARG(2, 3)
	void showWarn(WINPR_FORMAT_ARG const char* fmt, ...);
	void showWarn(const std::string& info);

	WINPR_ATTR_FORMAT_ARG(2, 3)
	void showError(WINPR_FORMAT_ARG const char* fmt, ...);
	void showError(const std::string& error);

	void show(SDLConnectionDialog::MsgType type, const std::string& msg);

	void show(bool visible = true);

	void handleShow();

  private:
	mutable std::mutex _mux;
	std::string _title;
	std::string _message;
	bool _visible = false;
	SDLConnectionDialog::MsgType _type = SDLConnectionDialog::MSG_NONE;
	std::unique_ptr<SDLConnectionDialog> _connection_dialog;
};
