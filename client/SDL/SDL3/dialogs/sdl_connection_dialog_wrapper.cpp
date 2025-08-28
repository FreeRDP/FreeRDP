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

#include <sstream>

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/log.h>

#include "../sdl_utils.hpp"
#include "sdl_connection_dialog.hpp"
#include "sdl_connection_dialog_wrapper.hpp"

SdlConnectionDialogWrapper::SdlConnectionDialogWrapper(wLog* log) : _log(log)
{
}

SdlConnectionDialogWrapper::~SdlConnectionDialogWrapper() = default;

void SdlConnectionDialogWrapper::create(rdpContext* context)
{
	const auto enabled =
	    freerdp_settings_get_bool(context->settings, FreeRDP_UseCommonStdioCallbacks);
	_connection_dialog.reset();
	if (!enabled)
		_connection_dialog = std::make_unique<SDLConnectionDialog>(context);
}

void SdlConnectionDialogWrapper::destroy()
{
	_connection_dialog.reset();
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
	push(EventArg{ title });
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
	push({ type, msg, true });
}

void SdlConnectionDialogWrapper::show(bool visible)
{
	push(EventArg{ visible });
}

void SdlConnectionDialogWrapper::handleShow()
{
	std::unique_lock lock(_mux);
	while (!_queue.empty())
	{
		auto arg = _queue.front();
		_queue.pop();

		if (arg.hasTitle() && _connection_dialog)
		{
			_connection_dialog->setTitle(arg.title().c_str());
		}

		if (arg.hasType() && arg.hasMessage())
		{
			switch (arg.type())
			{
				case SdlConnectionDialogWrapper::MSG_INFO:
					if (_connection_dialog)
						_connection_dialog->showInfo(arg.message().c_str());
					else
						WLog_Print(_log, WLOG_INFO, "%s", arg.message().c_str());
					break;
				case SdlConnectionDialogWrapper::MSG_WARN:
					if (_connection_dialog)
						_connection_dialog->showWarn(arg.message().c_str());
					else
						WLog_Print(_log, WLOG_WARN, "%s", arg.message().c_str());
					break;
				case SdlConnectionDialogWrapper::MSG_ERROR:
					if (_connection_dialog)
						_connection_dialog->showError(arg.message().c_str());
					else
						WLog_Print(_log, WLOG_ERROR, "%s", arg.message().c_str());
					break;
				default:
					break;
			}
		}

		if (arg.hasVisibility() && _connection_dialog)
		{
			if (arg.visible())
				_connection_dialog->show();
			else
				_connection_dialog->hide();
		}
	}
}

void SdlConnectionDialogWrapper::push(EventArg&& arg)
{
	{
		std::unique_lock lock(_mux);
		_queue.push(std::move(arg));
	}

	auto rc = SDL_RunOnMainThread(
	    [](void* user)
	    {
		    auto dlg = static_cast<SdlConnectionDialogWrapper*>(user);
		    dlg->handleShow();
	    },
	    this, false);
	if (!rc)
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[%s] SDL_RunOnMainThread failed with %s",
		            __func__, SDL_GetError());
}

SdlConnectionDialogWrapper::EventArg::EventArg(bool visible) : _visible(visible), _mask(8)
{
}

SdlConnectionDialogWrapper::EventArg::EventArg(const std::string& title) : _title(title), _mask(1)
{
}

SdlConnectionDialogWrapper::EventArg::EventArg(MsgType type, const std::string& msg, bool visible)
    : _message(msg), _type(type), _visible(visible), _mask(14)
{
}

bool SdlConnectionDialogWrapper::EventArg::hasTitle() const
{
	return _mask & 0x01;
}

const std::string& SdlConnectionDialogWrapper::EventArg::title() const
{
	return _title;
}

bool SdlConnectionDialogWrapper::EventArg::hasMessage() const
{
	return _mask & 0x02;
}

const std::string& SdlConnectionDialogWrapper::EventArg::message() const
{
	return _message;
}

bool SdlConnectionDialogWrapper::EventArg::hasType() const
{
	return _mask & 0x04;
}

SdlConnectionDialogWrapper::MsgType SdlConnectionDialogWrapper::EventArg::type() const
{
	return _type;
}

bool SdlConnectionDialogWrapper::EventArg::hasVisibility() const
{
	return _mask & 0x08;
}

bool SdlConnectionDialogWrapper::EventArg::visible() const
{
	return _visible;
}

std::string SdlConnectionDialogWrapper::EventArg::str() const
{
	std::stringstream ss;
	ss << "{ title:" << _title << ", message:" << _message << ", type:" << _type
	   << ", visible:" << _visible << ", mask:" << _mask << "}";
	return ss.str();
}
