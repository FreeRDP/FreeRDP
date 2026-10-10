/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
 *
 * Copyright 2026 Daniel Nylander <1206564+yeager@users.noreply.github.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include "sdl_floatbar.hpp"

SdlFloatbar::~SdlFloatbar()
{
	hide();
}

bool SdlFloatbar::show(SDL_Window* parent, bool sticky, bool defaultVisible)
{
	if (!parent)
		return false;
	if (_window && (_parent != parent))
		hide();

	if (!_window)
	{
		int parentWidth = 0;
		if (!SDL_GetWindowSize(parent, &parentWidth, nullptr))
			return false;
		const int x = (parentWidth > 240) ? (parentWidth - 240) / 2 : 0;
		_window = SDL_CreatePopupWindow(parent, x, 0, 240, 32,
		                                SDL_WINDOW_POPUP_MENU | SDL_WINDOW_ALWAYS_ON_TOP);
		if (!_window)
			return false;
		_parent = parent;

		_renderer = SDL_CreateRenderer(_window, nullptr);
		if (!_renderer)
		{
			hide();
			return false;
		}
	}

	_active = true;
	_sticky = sticky;
	if (!render())
		return false;
	return setShown(_sticky || defaultVisible);
}

bool SdlFloatbar::render()
{
	if (!_renderer)
		return false;
	if (!SDL_SetRenderDrawColor(_renderer, 45, 45, 45, 255) || !SDL_RenderClear(_renderer))
		return false;
	const SDL_FRect buttons[] = { { 0, 0, 80, 32 }, { 80, 0, 80, 32 }, { 160, 0, 80, 32 } };
	const SDL_Color colors[] = { { 80, 80, 80, 255 }, { 70, 100, 160, 255 }, { 170, 70, 70, 255 } };
	for (size_t x = 0; x < SDL_arraysize(buttons); x++)
	{
		if (!SDL_SetRenderDrawColor(_renderer, colors[x].r, colors[x].g, colors[x].b, colors[x].a) ||
		    !SDL_RenderFillRect(_renderer, &buttons[x]))
			return false;
	}
	if (!SDL_SetRenderDrawColor(_renderer, 255, 255, 255, 255) ||
	    !SDL_RenderDebugText(_renderer, 4, 12, "Minimize") ||
	    !SDL_RenderDebugText(_renderer, 84, 12, "Window") ||
	    !SDL_RenderDebugText(_renderer, 164, 12, "Disconnect"))
		return false;
	return SDL_RenderPresent(_renderer);
}

bool SdlFloatbar::setShown(bool shown)
{
	if (!_window)
		return !shown;
	if (SDL_WindowFlags flags = SDL_GetWindowFlags(_window);
	    ((flags & SDL_WINDOW_HIDDEN) == 0) == shown)
		return true;
	return shown ? SDL_ShowWindow(_window) : SDL_HideWindow(_window);
}

bool SdlFloatbar::handleParentMotion(const SDL_MouseMotionEvent& ev)
{
	if (!_active || _sticky || !ownsParent(ev.windowID))
		return true;
	return setShown(ev.y <= 8.0f);
}

bool SdlFloatbar::redraw()
{
	if (!_active || !_renderer)
		return true;
	return render();
}

void SdlFloatbar::hide()
{
	_active = false;
	if (_renderer)
		SDL_DestroyRenderer(_renderer);
	if (_window)
		SDL_DestroyWindow(_window);
	_parent = nullptr;
	_renderer = nullptr;
	_window = nullptr;
}

bool SdlFloatbar::owns(SDL_WindowID windowId) const
{
	return _window && (windowId == SDL_GetWindowID(_window));
}

bool SdlFloatbar::ownsParent(SDL_WindowID windowId) const
{
	return _parent && (windowId == SDL_GetWindowID(_parent));
}

SdlFloatbar::Action SdlFloatbar::handleEvent(const SDL_MouseButtonEvent& ev) const
{
	if (!owns(ev.windowID) || (ev.type != SDL_EVENT_MOUSE_BUTTON_UP) ||
	    (ev.button != SDL_BUTTON_LEFT))
		return Action::None;

	if (ev.x < 80)
		return Action::Minimize;
	if (ev.x < 160)
		return Action::ToggleFullscreen;
	return Action::Disconnect;
}
