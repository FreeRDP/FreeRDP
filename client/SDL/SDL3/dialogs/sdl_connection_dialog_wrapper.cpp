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

#include "sdl_connection_dialog_wrapper.hpp"
#include "sdl_connection_dialog.hpp"
#include "../sdl_utils.hpp"

SdlConnectionDialogWrapper::SdlConnectionDialogWrapper() = default;

SdlConnectionDialogWrapper::~SdlConnectionDialogWrapper() = default;

void SdlConnectionDialogWrapper::create(rdpContext* context)
{
	std::unique_lock lock(_mux);
	_connection_dialog = std::make_unique<SDLConnectionDialog>(context);
	sdl_push_user_event(SDL_EVENT_USER_UPDATE_CONNECT_DIALOG);
}

void SdlConnectionDialogWrapper::destroy()
{
	std::unique_lock lock(_mux);
	_connection_dialog.reset();
	sdl_push_user_event(SDL_EVENT_USER_UPDATE_CONNECT_DIALOG);
}

bool SdlConnectionDialogWrapper::isRunning() const
{
	std::unique_lock lock(_mux);
	if (!_connection_dialog)
		return false;
	return _connection_dialog->running();
}

bool SdlConnectionDialogWrapper::isVisible() const
{
	std::unique_lock lock(_mux);
	if (!_connection_dialog)
		return false;
	return _connection_dialog->visible();
}

bool SdlConnectionDialogWrapper::handleEvent(const SDL_Event& event)
{
	std::unique_lock lock(_mux);
	if (!_connection_dialog)
		return false;
	return _connection_dialog->handle(event);
}

WINPR_ATTR_FORMAT_ARG(1, 0)
static std::string format(WINPR_FORMAT_ARG const char* fmt, va_list ap)
{
	va_list ap1;
	va_copy(ap1, ap);
	const int size = vsnprintf(nullptr, 0, fmt, ap1);
	va_end(ap1);

	if (size < 0)
		return "";

	std::string msg;
	msg.resize(static_cast<size_t>(size) + 1);

	va_list ap2;
	va_copy(ap2, ap);
	(void)vsnprintf(msg.data(), msg.size(), fmt, ap2);
	va_end(ap2);
	return msg;
}

void SdlConnectionDialogWrapper::setTitle(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	setTitle(format(fmt, ap));
	va_end(ap);
}

void SdlConnectionDialogWrapper::setTitle(const std::string& title)
{
	std::unique_lock lock(_mux);
	_title = title;
	sdl_push_user_event(SDL_EVENT_USER_UPDATE_CONNECT_DIALOG);
}

void SdlConnectionDialogWrapper::showInfo(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	showInfo(format(fmt, ap));
	va_end(ap);
}

void SdlConnectionDialogWrapper::showInfo(const std::string& info)
{
	show(MSG_INFO, info);
}

void SdlConnectionDialogWrapper::showWarn(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	showWarn(format(fmt, ap));
	va_end(ap);
}

void SdlConnectionDialogWrapper::showWarn(const std::string& info)
{
	show(MSG_WARN, info);
}

void SdlConnectionDialogWrapper::showError(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	showError(format(fmt, ap));
	va_end(ap);
}

void SdlConnectionDialogWrapper::showError(const std::string& error)
{
	show(MSG_ERROR, error);
}

void SdlConnectionDialogWrapper::show(SdlConnectionDialogWrapper::MsgType type,
                                      const std::string& msg)
{
	std::unique_lock lock(_mux);
	_message = msg;
	_type = type;
	_visible = true;
	sdl_push_user_event(SDL_EVENT_USER_UPDATE_CONNECT_DIALOG);
}

void SdlConnectionDialogWrapper::show(bool visible)
{
	std::unique_lock lock(_mux);
	_visible = visible;
	sdl_push_user_event(SDL_EVENT_USER_UPDATE_CONNECT_DIALOG);
}

void SdlConnectionDialogWrapper::handleShow()
{
	std::unique_lock lock(_mux);
	if (!_connection_dialog)
		return;

	_connection_dialog->setTitle(_title.c_str());
	if (!_visible)
	{
		_connection_dialog->hide();
		return;
	}

	switch (_type)
	{
		case SdlConnectionDialogWrapper::MSG_INFO:
			_connection_dialog->showInfo(_message.c_str());
			break;
		case SdlConnectionDialogWrapper::MSG_WARN:
			_connection_dialog->showWarn(_message.c_str());
			break;
		case SdlConnectionDialogWrapper::MSG_ERROR:
			_connection_dialog->showError(_message.c_str());
			break;
		default:
			break;
	}

	_connection_dialog->show();
}
