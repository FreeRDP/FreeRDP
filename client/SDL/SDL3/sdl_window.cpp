/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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
#include "sdl_window.hpp"
#include "sdl_utils.hpp"

SdlWindow::SdlWindow(const std::string& title, Sint32 startupX, Sint32 startupY, Sint32 width,
                     Sint32 height, Uint32 flags)
{
	auto props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, startupX);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, startupY);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
	// SDL_SetProperty(props, SDL_PROP_WINDOW_CREATE_FL);
	_window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);
}

SdlWindow::SdlWindow(SdlWindow&& other) noexcept
    : _window(other._window), _offset_x(other._offset_x), _offset_y(other._offset_y)
{
	other._window = nullptr;
}

SdlWindow::~SdlWindow()
{
	SDL_DestroyWindow(_window);
}

Uint32 SdlWindow::id() const
{
	if (!_window)
		return 0;
	return SDL_GetWindowID(_window);
}

int SdlWindow::displayIndex() const
{
	if (!_window)
		return 0;
	return SDL_GetDisplayForWindow(_window);
}

SDL_Rect SdlWindow::rect() const
{
	SDL_Rect rect = {};
	if (_window)
	{
		SDL_GetWindowPosition(_window, &rect.x, &rect.y);
		SDL_GetWindowSize(_window, &rect.w, &rect.h);
	}
	return rect;
}

SDL_Window* SdlWindow::window() const
{
	return _window;
}

Sint32 SdlWindow::offsetX() const
{
	return _offset_x;
}

void SdlWindow::setOffsetX(Sint32 x)
{
	_offset_x = x;
}

void SdlWindow::setOffsetY(Sint32 y)
{
	_offset_y = y;
}

Sint32 SdlWindow::offsetY() const
{
	return _offset_y;
}

bool SdlWindow::grabKeyboard(bool enable)
{
	if (!_window)
		return false;
	SDL_SetWindowKeyboardGrab(_window, enable);
	return true;
}

bool SdlWindow::grabMouse(bool enable)
{
	if (!_window)
		return false;
	SDL_SetWindowMouseGrab(_window, enable);
	return true;
}

void SdlWindow::setBordered(bool bordered)
{
	if (_window)
		SDL_SetWindowBordered(_window, bordered);
}

void SdlWindow::raise()
{
	SDL_RaiseWindow(_window);
}

void SdlWindow::resizeable(bool use)
{
	SDL_SetWindowResizable(_window, use);
}

void SdlWindow::fullscreen(bool enter)
{
	auto curFlags = SDL_GetWindowFlags(_window);

	if (enter)
	{
		if (!(curFlags & SDL_WINDOW_BORDERLESS))
		{
			auto idx = SDL_GetDisplayForWindow(_window);
			auto mode = SDL_GetCurrentDisplayMode(idx);

			SDL_RestoreWindow(_window); // Maximize so we can see the caption and
			                            // bits
			SDL_SetWindowBordered(_window, false);
			SDL_SetWindowPosition(_window, 0, 0);
			SDL_SetWindowAlwaysOnTop(_window, true);
			SDL_RaiseWindow(_window);
			if (mode)
				SDL_SetWindowSize(_window, mode->w, mode->h);
		}
	}
	else
	{
		if (curFlags & SDL_WINDOW_BORDERLESS)
		{

			SDL_SetWindowBordered(_window, true);
			SDL_SetWindowAlwaysOnTop(_window, false);
			SDL_RaiseWindow(_window);
			SDL_MinimizeWindow(_window); // Maximize so we can see the caption and bits
			SDL_MaximizeWindow(_window); // Maximize so we can see the caption and bits
		}
	}
}

void SdlWindow::minimize()
{
	SDL_MinimizeWindow(_window);
}

bool SdlWindow::fill(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	auto surface = SDL_GetWindowSurface(_window);
	if (!surface)
		return false;
	SDL_Rect rect = { 0, 0, surface->w, surface->h };
	auto color = SDL_MapSurfaceRGBA(surface, r, g, b, a);

	SDL_FillSurfaceRect(surface, &rect, color);
	return true;
}

bool SdlWindow::blit(SDL_Surface* surface, const SDL_Rect& srcRect, SDL_Rect& dstRect)
{
	auto screen = SDL_GetWindowSurface(_window);
	if (!screen || !surface)
		return false;
	if (!SDL_SetSurfaceClipRect(surface, &srcRect))
		return true;
	if (!SDL_SetSurfaceClipRect(screen, &dstRect))
		return true;
	if (!SDL_BlitSurfaceScaled(surface, &srcRect, screen, &dstRect, SDL_SCALEMODE_LINEAR))
	{
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "SDL_BlitScaled: %s", SDL_GetError());
		return false;
	}
	return true;
}

void SdlWindow::updateSurface()
{
	SDL_UpdateWindowSurface(_window);
}
