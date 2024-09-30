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
#include <cassert>
#include <thread>

#include "sdl_connection_dialog.hpp"
#include "../sdl_utils.hpp"
#include "../sdl_freerdp.hpp"
#include "res/sdl3_resource_manager.hpp"

static const SDL_Color backgroundcolor = { 0x38, 0x36, 0x35, 0xff };
static const SDL_Color textcolor = { 0xd1, 0xcf, 0xcd, 0xff };
static const SDL_Color infocolor = { 0x43, 0xe0, 0x0f, 0x60 };
static const SDL_Color warncolor = { 0xcd, 0xca, 0x35, 0x60 };
static const SDL_Color errorcolor = { 0xf7, 0x22, 0x30, 0x60 };

static const Uint32 vpadding = 5;
static const Uint32 hpadding = 5;

SDLConnectionDialog::SDLConnectionDialog(rdpContext* context) : _context(context)
{
	SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
	hide();
}

SDLConnectionDialog::~SDLConnectionDialog()
{
	resetTimer();
	destroyWindow();
	SDL_Quit();
}

bool SDLConnectionDialog::visible() const
{
	if (!_window || !_renderer)
		return false;

	auto flags = SDL_GetWindowFlags(_window);
	return (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) == 0;
}

bool SDLConnectionDialog::setTitle(const char* fmt, ...)
{
	std::lock_guard lock(_mux);
	va_list ap = {};
	va_start(ap, fmt);
	_title = print(fmt, ap);
	va_end(ap);

	return show(MSG_NONE);
}

bool SDLConnectionDialog::showInfo(const char* fmt, ...)
{
	va_list ap = {};
	va_start(ap, fmt);
	auto rc = show(MSG_INFO, fmt, ap);
	va_end(ap);
	return rc;
}

bool SDLConnectionDialog::showWarn(const char* fmt, ...)
{
	va_list ap = {};
	va_start(ap, fmt);
	auto rc = show(MSG_WARN, fmt, ap);
	va_end(ap);
	return rc;
}

bool SDLConnectionDialog::showError(const char* fmt, ...)
{
	va_list ap = {};
	va_start(ap, fmt);
	auto rc = show(MSG_ERROR, fmt, ap);
	va_end(ap);
	if (!rc)
		return rc;
	return setTimer();
}

bool SDLConnectionDialog::show()
{
	std::lock_guard lock(_mux);
	return show(_type_active);
}

bool SDLConnectionDialog::hide()
{
	std::lock_guard lock(_mux);
	return show(MSG_DISCARD);
}

bool SDLConnectionDialog::running() const
{
	std::lock_guard lock(_mux);
	return _running;
}

bool SDLConnectionDialog::update()
{
	std::lock_guard lock(_mux);
	switch (_type)
	{
		case MSG_INFO:
		case MSG_WARN:
		case MSG_ERROR:
			_type_active = _type;
			createWindow();
			break;
		case MSG_DISCARD:
			resetTimer();
			destroyWindow();
			break;
		default:
			if (_window)
			{
				SDL_SetWindowTitle(_window, _title.c_str());
			}
			break;
	}
	_type = MSG_NONE;
	return true;
}

bool SDLConnectionDialog::setModal()
{
	if (_window)
	{
		auto sdl = get_context(_context);
		if (sdl->windows.empty())
			return true;

		auto parent = sdl->windows.begin()->second.window();
		SDL_SetWindowParent(_window, parent);
		SDL_SetWindowModal(_window, true);
		SDL_RaiseWindow(_window);
	}
	return true;
}

bool SDLConnectionDialog::clearWindow(SDL_Renderer* renderer)
{
	assert(renderer);

	const int drc = SDL_SetRenderDrawColor(renderer, backgroundcolor.r, backgroundcolor.g,
	                                       backgroundcolor.b, backgroundcolor.a);
	if (widget_log_error(drc, "SDL_SetRenderDrawColor"))
		return false;

	const int rcls = SDL_RenderClear(renderer);
	return !widget_log_error(rcls, "SDL_RenderClear");
}

bool SDLConnectionDialog::update(SDL_Renderer* renderer)
{
	std::lock_guard lock(_mux);
	if (!renderer)
		return false;

	if (!clearWindow(renderer))
		return false;

	for (auto& btn : _list)
	{
		if (!btn.widget.update_text(renderer, _msg, btn.fgcolor, btn.bgcolor))
			return false;
	}

	if (!_buttons.update(renderer))
		return false;

	SDL_RenderPresent(renderer);
	return true;
}

bool SDLConnectionDialog::wait(bool ignoreRdpContext)
{
	while (running())
	{
		if (!ignoreRdpContext)
		{
			if (freerdp_shall_disconnect_context(_context))
				return false;
		}
		std::this_thread::yield();
	}
	return true;
}

bool SDLConnectionDialog::handle(const SDL_Event& event)
{
	Uint32 windowID = 0;
	if (_window)
	{
		windowID = SDL_GetWindowID(_window);
	}

	switch (event.type)
	{
		case SDL_EVENT_USER_RETRY_DIALOG:
			return update();
		case SDL_EVENT_QUIT:
			resetTimer();
			destroyWindow();
			return false;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			if (visible())
			{
				auto& ev = reinterpret_cast<const SDL_KeyboardEvent&>(event);
				update(_renderer);
				switch (event.key.key)
				{
					case SDLK_RETURN:
					case SDLK_RETURN2:
					case SDLK_ESCAPE:
					case SDLK_KP_ENTER:
						if (event.type == SDL_EVENT_KEY_UP)
						{
							freerdp_abort_event(_context);
							sdl_push_quit();
						}
						break;
					case SDLK_TAB:
						_buttons.set_highlight_next();
						break;
					default:
						break;
				}

				return windowID == ev.windowID;
			}
			return false;
		case SDL_EVENT_MOUSE_MOTION:
			if (visible())
			{
				auto& ev = reinterpret_cast<const SDL_MouseMotionEvent&>(event);

				_buttons.set_mouseover(event.button.x, event.button.y);
				update(_renderer);
				return windowID == ev.windowID;
			}
			return false;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (visible())
			{
				auto& ev = reinterpret_cast<const SDL_MouseButtonEvent&>(event);
				update(_renderer);

				auto button = _buttons.get_selected(event.button);
				if (button)
				{
					if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
					{
						freerdp_abort_event(_context);
						sdl_push_quit();
					}
				}

				return windowID == ev.windowID;
			}
			return false;
		case SDL_EVENT_MOUSE_WHEEL:
			if (visible())
			{
				auto& ev = reinterpret_cast<const SDL_MouseWheelEvent&>(event);
				update(_renderer);
				return windowID == ev.windowID;
			}
			return false;
		case SDL_EVENT_FINGER_UP:
		case SDL_EVENT_FINGER_DOWN:
			if (visible())
			{
				auto& ev = reinterpret_cast<const SDL_TouchFingerEvent&>(event);
				update(_renderer);
				return windowID == ev.windowID;
			}
			return false;
		default:
			if ((event.type >= SDL_EVENT_WINDOW_FIRST) && (event.type <= SDL_EVENT_WINDOW_LAST))
			{
				auto& ev = reinterpret_cast<const SDL_WindowEvent&>(event);
				switch (ev.type)
				{
					case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
						if (windowID == ev.windowID)
						{
							freerdp_abort_event(_context);
							sdl_push_quit();
						}
						break;
					default:
						update(_renderer);
						setModal();
						break;
				}

				return windowID == ev.windowID;
			}
			return false;
	}
}

bool SDLConnectionDialog::createWindow()
{
	destroyWindow();

	const size_t widget_height = 50;
	const size_t widget_width = 600;
	const size_t total_height = 300;

	auto rc = SDL_CreateWindowAndRenderer(_title.c_str(), widget_width, total_height,
	                                      SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_MOUSE_FOCUS |
	                                          SDL_WINDOW_INPUT_FOCUS,
	                                      &_window, &_renderer);
	if (rc != 0)
	{
		widget_log_error(rc, "SDL_CreateWindowAndRenderer");
		return false;
	}
	setModal();

	SDL_Color res_bgcolor;
	switch (_type_active)
	{
		case MSG_INFO:
			res_bgcolor = infocolor;
			break;
		case MSG_WARN:
			res_bgcolor = warncolor;
			break;
		case MSG_ERROR:
			res_bgcolor = errorcolor;
			break;
		case MSG_DISCARD:
		default:
			res_bgcolor = backgroundcolor;
			break;
	}

#if defined(WITH_SDL_IMAGE_DIALOGS)
	std::string res_name;
	switch (_type_active)
	{
		case MSG_INFO:
			res_name = "icon_info.svg";
			break;
		case MSG_WARN:
			res_name = "icon_warning.svg";
			break;
		case MSG_ERROR:
			res_name = "icon_error.svg";
			break;
		case MSG_DISCARD:
		default:
			res_name = "";
			break;
	}

	int height = (total_height - 3ul * vpadding) / 2ul;
	SDL_FRect iconRect{ hpadding, vpadding, widget_width / 4ul - 2ul * hpadding,
		                static_cast<float>(height) };
	widget_cfg_t icon{ textcolor,
		               res_bgcolor,
		               { _renderer, iconRect,
		                 SDL3ResourceManager::get(SDLResourceManager::typeImages(), res_name) } };
	_list.emplace_back(std::move(icon));

	iconRect.y += height;

	widget_cfg_t logo{ textcolor,
		               backgroundcolor,
		               { _renderer, iconRect,
		                 SDL3ResourceManager::get(SDLResourceManager::typeImages(),
		                                          "FreeRDP_Icon.svg") } };
	_list.emplace_back(std::move(logo));

	SDL_FRect rect = { widget_width / 4ul, vpadding, widget_width * 3ul / 4ul,
		               total_height - 3ul * vpadding - widget_height };
#else
	SDL_FRect rect = { hpadding, vpadding, widget_width - 2ul * hpadding,
		               total_height - 2ul * vpadding };
#endif

	widget_cfg_t w{ textcolor, backgroundcolor, { _renderer, rect, false } };
	w.widget.set_wrap(true, widget_width);
	_list.emplace_back(std::move(w));
	rect.y += widget_height + vpadding;

	const std::vector<int> buttonids = { 1 };
	const std::vector<std::string> buttonlabels = { "cancel" };
	_buttons.populate(_renderer, buttonlabels, buttonids, widget_width,
	                  total_height - widget_height - vpadding,
	                  static_cast<Sint32>(widget_width / 2), static_cast<Sint32>(widget_height));
	_buttons.set_highlight(0);

	SDL_ShowWindow(_window);
	SDL_RaiseWindow(_window);

	return true;
}

void SDLConnectionDialog::destroyWindow()
{
	_buttons.clear();
	_list.clear();
	SDL_DestroyRenderer(_renderer);
	SDL_DestroyWindow(_window);
	_renderer = nullptr;
	_window = nullptr;
}

bool SDLConnectionDialog::show(MsgType type, const char* fmt, va_list ap)
{
	std::lock_guard lock(_mux);
	_msg = print(fmt, ap);
	return show(type);
}

bool SDLConnectionDialog::show(MsgType type)
{
	_type = type;
	return sdl_push_user_event(SDL_EVENT_USER_RETRY_DIALOG);
}

std::string SDLConnectionDialog::print(const char* fmt, va_list ap)
{
	int size = -1;
	std::string res;

	do
	{
		res.resize(128);
		if (size > 0)
			res.resize(size);

		va_list copy;
		va_copy(copy, ap);
		WINPR_PRAGMA_DIAG_PUSH
		WINPR_PRAGMA_DIAG_IGNORED_FORMAT_NONLITERAL
		size = vsnprintf(res.data(), res.size(), fmt, copy);
		WINPR_PRAGMA_DIAG_POP
		va_end(copy);

	} while ((size > 0) && (static_cast<size_t>(size) > res.size()));

	return res;
}

bool SDLConnectionDialog::setTimer(Uint32 timeoutMS)
{
	std::lock_guard lock(_mux);
	resetTimer();

	_timer = SDL_AddTimer(timeoutMS, &SDLConnectionDialog::timeout, this);
	_running = true;
	return true;
}

void SDLConnectionDialog::resetTimer()
{
	if (_running)
		SDL_RemoveTimer(_timer);
	_running = false;
}

Uint32 SDLConnectionDialog::timeout(void* pvthis, SDL_TimerID timerID, Uint32 intervalMS)
{
	auto ths = static_cast<SDLConnectionDialog*>(pvthis);
	ths->hide();
	ths->_running = false;
	return 0;
}

SDLConnectionDialogHider::SDLConnectionDialogHider(freerdp* instance)
    : SDLConnectionDialogHider(get(instance))
{
}

SDLConnectionDialogHider::SDLConnectionDialogHider(rdpContext* context)
    : SDLConnectionDialogHider(get(context))
{
}

SDLConnectionDialogHider::SDLConnectionDialogHider(SDLConnectionDialog* dialog) : _dialog(dialog)
{
	if (_dialog)
	{
		_visible = _dialog->visible();
		if (_visible)
		{
			_dialog->hide();
		}
	}
}

SDLConnectionDialogHider::~SDLConnectionDialogHider()
{
	if (_dialog && _visible)
	{
		_dialog->show();
	}
}

SDLConnectionDialog* SDLConnectionDialogHider::get(freerdp* instance)
{
	if (!instance)
		return nullptr;
	return get(instance->context);
}

SDLConnectionDialog* SDLConnectionDialogHider::get(rdpContext* context)
{
	auto sdl = get_context(context);
	if (!sdl)
		return nullptr;
	return sdl->connection_dialog.get();
}
