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
                     Sint32 height, [[maybe_unused]] Uint32 flags)
{
	auto props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, startupX);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, startupY);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);

	if (flags & SDL_WINDOW_HIGH_PIXEL_DENSITY)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);

	if (flags & SDL_WINDOW_FULLSCREEN)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);

	if (flags & SDL_WINDOW_BORDERLESS)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);

	_window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);

	auto scale = SDL_GetWindowPixelDensity(_window);
	const int iscale = static_cast<int>(scale * 100.0f);
	auto w = 100 * width / iscale;
	auto h = 100 * height / iscale;
	(void)SDL_SetWindowSize(_window, w, h);
	(void)SDL_SyncWindow(_window);
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

SDL_DisplayID SdlWindow::displayIndex() const
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
		SDL_GetWindowSizeInPixels(_window, &rect.w, &rect.h);
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

rdpMonitor SdlWindow::monitor() const
{
	rdpMonitor mon{};

	const auto factor = SDL_GetWindowDisplayScale(_window);
	const auto dsf = static_cast<UINT32>(100 * factor);
	mon.attributes.desktopScaleFactor = dsf;
	mon.attributes.deviceScaleFactor = 100;

	int pixelWidth = 0;
	int pixelHeight = 0;
	auto prc = SDL_GetWindowSizeInPixels(_window, &pixelWidth, &pixelHeight);

	if (prc)
	{
		mon.width = pixelWidth;
		mon.height = pixelHeight;

		mon.attributes.physicalWidth = WINPR_ASSERTING_INT_CAST(uint32_t, pixelWidth);
		mon.attributes.physicalHeight = WINPR_ASSERTING_INT_CAST(uint32_t, pixelHeight);
	}

	SDL_Rect rect = {};
	auto did = SDL_GetDisplayForWindow(_window);
	auto rc = SDL_GetDisplayBounds(did, &rect);

	if (rc)
	{
		mon.x = rect.x;
		mon.y = rect.y;
	}

	auto orientation = SDL_GetCurrentDisplayOrientation(did);
	mon.attributes.orientation = sdl::utils::orientaion_to_rdp(orientation);

	auto primary = SDL_GetPrimaryDisplay();
	mon.is_primary = SDL_GetWindowID(_window) == primary;
	mon.orig_screen = did;
	return mon;
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
	(void)SDL_SyncWindow(_window);
}

void SdlWindow::raise()
{
	SDL_RaiseWindow(_window);
	(void)SDL_SyncWindow(_window);
}

void SdlWindow::resizeable(bool use)
{
	SDL_SetWindowResizable(_window, use);
	(void)SDL_SyncWindow(_window);
}

void SdlWindow::fullscreen(bool enter)
{
	(void)SDL_SetWindowFullscreen(_window, enter);
	(void)SDL_SyncWindow(_window);
}

void SdlWindow::minimize()
{
	SDL_MinimizeWindow(_window);
	(void)SDL_SyncWindow(_window);
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
