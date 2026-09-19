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

bool SdlFloatbar::show(SDL_Window* parent)
{
	if (_window)
		return true;

	_window = SDL_CreatePopupWindow(parent, 0, 0, 240, 32,
	                                SDL_WINDOW_POPUP_MENU | SDL_WINDOW_BORDERLESS |
	                                    SDL_WINDOW_ALWAYS_ON_TOP);
	if (!_window)
		return false;

	_renderer = SDL_CreateRenderer(_window, nullptr);
	if (!_renderer)
	{
		hide();
		return false;
	}

	SDL_SetRenderDrawColor(_renderer, 45, 45, 45, 255);
	SDL_RenderClear(_renderer);
	const SDL_FRect buttons[] = { { 0, 0, 80, 32 }, { 80, 0, 80, 32 }, { 160, 0, 80, 32 } };
	const SDL_Color colors[] = { { 80, 80, 80, 255 }, { 70, 100, 160, 255 }, { 170, 70, 70, 255 } };
	for (size_t x = 0; x < SDL_arraysize(buttons); x++)
	{
		SDL_SetRenderDrawColor(_renderer, colors[x].r, colors[x].g, colors[x].b, colors[x].a);
		SDL_RenderFillRect(_renderer, &buttons[x]);
	}
	SDL_SetRenderDrawColor(_renderer, 255, 255, 255, 255);
	SDL_RenderDebugText(_renderer, 4, 12, "Minimize");
	SDL_RenderDebugText(_renderer, 84, 12, "Window");
	SDL_RenderDebugText(_renderer, 164, 12, "Disconnect");
	SDL_RenderPresent(_renderer);
	return true;
}

void SdlFloatbar::hide()
{
	if (_renderer)
		SDL_DestroyRenderer(_renderer);
	if (_window)
		SDL_DestroyWindow(_window);
	_renderer = nullptr;
	_window = nullptr;
}

bool SdlFloatbar::owns(SDL_WindowID windowId) const
{
	return _window && (windowId == SDL_GetWindowID(_window));
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
