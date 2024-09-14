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
    : _window(SDL_CreateWindow(title.c_str(), startupX, startupY, width, height, flags))
{
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
	return SDL_GetWindowDisplayIndex(_window);
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
#if SDL_VERSION_ATLEAST(2, 0, 16)
	SDL_SetWindowKeyboardGrab(_window, enable ? SDL_TRUE : SDL_FALSE);
	return true;
#else
	SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Keyboard grabbing not supported by SDL2 < 2.0.16");
	return false;
#endif
}

bool SdlWindow::grabMouse(bool enable)
{
	if (!_window)
		return false;
#if SDL_VERSION_ATLEAST(2, 0, 16)
	SDL_SetWindowMouseGrab(_window, enable ? SDL_TRUE : SDL_FALSE);
#else
	SDL_SetWindowGrab(_window, enable ? SDL_TRUE : SDL_FALSE);
#endif
	return true;
}

void SdlWindow::setBordered(bool bordered)
{
	if (_window)
		SDL_SetWindowBordered(_window, bordered ? SDL_TRUE : SDL_FALSE);
}

void SdlWindow::raise()
{
	SDL_RaiseWindow(_window);
}

void SdlWindow::resizeable(bool use)
{
	SDL_SetWindowResizable(_window, use ? SDL_TRUE : SDL_FALSE);
}

void SdlWindow::fullscreen(bool enter)
{
	auto curFlags = SDL_GetWindowFlags(_window);

	if (enter)
	{
		if (!(curFlags & SDL_WINDOW_BORDERLESS))
		{
			auto idx = SDL_GetWindowDisplayIndex(_window);
			SDL_DisplayMode mode = {};
			SDL_GetCurrentDisplayMode(idx, &mode);

			SDL_RestoreWindow(_window); // Maximize so we can see the caption and
			                            // bits
			SDL_SetWindowBordered(_window, SDL_FALSE);
			SDL_SetWindowPosition(_window, 0, 0);
#if SDL_VERSION_ATLEAST(2, 0, 16)
			SDL_SetWindowAlwaysOnTop(_window, SDL_TRUE);
#endif
			SDL_RaiseWindow(_window);
			SDL_SetWindowSize(_window, mode.w, mode.h);
		}
	}
	else
	{
		if (curFlags & SDL_WINDOW_BORDERLESS)
		{

			SDL_SetWindowBordered(_window, SDL_TRUE);
#if SDL_VERSION_ATLEAST(2, 0, 16)
			SDL_SetWindowAlwaysOnTop(_window, SDL_FALSE);
#endif
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
	auto color = SDL_MapRGBA(surface->format, r, g, b, a);

	SDL_FillRect(surface, &rect, color);
	return true;
}

bool SdlWindow::blit(SDL_Surface* surface, SDL_Rect srcRect, SDL_Rect& dstRect)
{
	auto screen = SDL_GetWindowSurface(_window);
	if (!screen || !surface)
		return false;
	if (!SDL_SetClipRect(surface, &srcRect))
		return true;
	if (!SDL_SetClipRect(screen, &dstRect))
		return true;
	auto rc = SDL_BlitScaled(surface, &srcRect, screen, &dstRect);
	if (rc != 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_RENDER, "SDL_BlitScaled: %s [%d]", sdl_error_string(rc), rc);
	}
	return rc == 0;
}

void SdlWindow::updateSurface()
{
	SDL_UpdateWindowSurface(_window);
}
