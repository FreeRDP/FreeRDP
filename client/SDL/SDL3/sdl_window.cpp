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
#include <cmath>
#include <limits>
#include <sstream>

#include "sdl_window.hpp"
#include "sdl_utils.hpp"

SdlWindow::SdlWindow(SDL_DisplayID id, const std::string& title, const SDL_Rect& rect,
                     [[maybe_unused]] Uint32 flags)
    : _displayID(id)
{
	// since scale not known until surface is mapped,
	// create 1×1 and sync to get scale, then resize to logical size
	auto props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, rect.x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, rect.y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 1);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 1);

	if (flags & SDL_WINDOW_HIGH_PIXEL_DENSITY)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);

	if (flags & SDL_WINDOW_FULLSCREEN)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);

	if (flags & SDL_WINDOW_BORDERLESS)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);

	_window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);

	std::ignore = SDL_SyncWindow(_window);

	if (!(flags & SDL_WINDOW_FULLSCREEN))
	{
		// windowed: convert physical pixels to logical coords
		auto sc = scale();
		if (sc < 1.0f)
			sc = 1.0f;
		auto logicalW = static_cast<int>(std::lroundf(static_cast<float>(rect.w) / sc));
		auto logicalH = static_cast<int>(std::lroundf(static_cast<float>(rect.h) / sc));
		std::ignore = resize({ logicalW, logicalH });
		std::ignore = SDL_SyncWindow(_window);
	}
	// fullscreen: SDL sizes the window to the display automatically

	SDL_SetHint(SDL_HINT_APP_NAME, "");
}

SdlWindow::SdlWindow(SdlWindow&& other) noexcept
    : _window(other._window), _displayID(other._displayID), _offset_x(other._offset_x),
      _offset_y(other._offset_y)
{
	other._window = nullptr;
}

SdlWindow::~SdlWindow()
{
	SDL_DestroyWindow(_window);
}

SDL_WindowID SdlWindow::id() const
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

SDL_Rect SdlWindow::bounds() const
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

rdpMonitor SdlWindow::monitor(bool isPrimary) const
{
	rdpMonitor mon{};

	const auto factor = scale();
	const auto dsf = static_cast<UINT32>(100 * factor);
	mon.attributes.desktopScaleFactor = dsf;
	mon.attributes.deviceScaleFactor = 100;

	const auto r = rect();
	mon.width = r.w;
	mon.height = r.h;

	mon.attributes.physicalWidth = WINPR_ASSERTING_INT_CAST(uint32_t, r.w);
	mon.attributes.physicalHeight = WINPR_ASSERTING_INT_CAST(uint32_t, r.h);

	SDL_Rect rect = {};
	auto did = SDL_GetDisplayForWindow(_window);
	auto rc = SDL_GetDisplayBounds(did, &rect);

	if (rc)
	{
		mon.x = rect.x;
		mon.y = rect.y;
	}

	const auto orient = orientation();
	mon.attributes.orientation = sdl::utils::orientaion_to_rdp(orient);

	auto primary = SDL_GetPrimaryDisplay();
	mon.is_primary = isPrimary || (SDL_GetWindowID(_window) == primary);
	mon.orig_screen = did;
	if (mon.is_primary)
	{
		mon.x = 0;
		mon.y = 0;
	}
	return mon;
}

float SdlWindow::scale() const
{
	return SDL_GetWindowDisplayScale(_window);
}

SDL_DisplayOrientation SdlWindow::orientation() const
{
	const auto did = displayIndex();
	return SDL_GetCurrentDisplayOrientation(did);
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
	std::ignore = SDL_SyncWindow(_window);
}

void SdlWindow::raise()
{
	SDL_RaiseWindow(_window);
	std::ignore = SDL_SyncWindow(_window);
}

void SdlWindow::resizeable(bool use)
{
	SDL_SetWindowResizable(_window, use);
	std::ignore = SDL_SyncWindow(_window);
}

void SdlWindow::fullscreen(bool enter, bool forceOriginalDisplay)
{
	if (enter && forceOriginalDisplay && _displayID != 0)
	{
		/* Move the window to the desired display. We should not wait
		 * for the window to be moved, because some backends can refuse
		 * the move. The intent of moving the window is enough for SDL
		 * to decide which display will be used for fullscreen. */
		SDL_Rect rect = {};
		std::ignore = SDL_GetDisplayBounds(_displayID, &rect);
		std::ignore = SDL_SetWindowPosition(_window, rect.x, rect.y);
	}
	std::ignore = SDL_SetWindowFullscreen(_window, enter);
	std::ignore = SDL_SyncWindow(_window);
}

void SdlWindow::minimize()
{
	SDL_MinimizeWindow(_window);
	std::ignore = SDL_SyncWindow(_window);
}

bool SdlWindow::resize(const SDL_Point& size)
{
	return SDL_SetWindowSize(_window, size.x, size.y);
}

bool SdlWindow::drawRect(SDL_Surface* surface, SDL_Point offset, const SDL_Rect& srcRect)
{
	WINPR_ASSERT(surface);
	SDL_Rect dstRect = { offset.x + srcRect.x, offset.y + srcRect.y, srcRect.w, srcRect.h };
	return blit(surface, srcRect, dstRect);
}

bool SdlWindow::drawRects(SDL_Surface* surface, SDL_Point offset,
                          const std::vector<SDL_Rect>& rects)
{
	if (rects.empty())
	{
		return drawRect(surface, offset, { 0, 0, surface->w, surface->h });
	}
	for (auto& srcRect : rects)
	{
		if (!drawRect(surface, offset, srcRect))
			return false;
	}
	return true;
}

bool SdlWindow::drawScaledRect(SDL_Surface* surface, const SDL_FPoint& scale,
                               const SDL_Rect& srcRect)
{
	SDL_Rect dstRect = srcRect;
	dstRect.x = static_cast<Sint32>(static_cast<float>(dstRect.x) * scale.x);
	dstRect.w = static_cast<Sint32>(static_cast<float>(dstRect.w) * scale.x);
	dstRect.y = static_cast<Sint32>(static_cast<float>(dstRect.y) * scale.y);
	dstRect.h = static_cast<Sint32>(static_cast<float>(dstRect.h) * scale.y);
	return blit(surface, srcRect, dstRect);
}

bool SdlWindow::drawScaledRects(SDL_Surface* surface, const SDL_FPoint& scale,
                                const std::vector<SDL_Rect>& rects)
{
	if (rects.empty())
	{
		return drawScaledRect(surface, scale, { 0, 0, surface->w, surface->h });
	}
	for (const auto& srcRect : rects)
	{
		if (!drawScaledRect(surface, scale, srcRect))
			return false;
	}
	return true;
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

SdlWindow SdlWindow::create(SDL_DisplayID id, const std::string& title, Uint32 flags, Uint32 width,
                            Uint32 height)
{
	flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

	// width/height from caller is actual physical pixels
	SDL_Rect rect = { static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(id)),
		              static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(id)), static_cast<int>(width),
		              static_cast<int>(height) };

	if ((flags & SDL_WINDOW_FULLSCREEN) != 0)
	{
		// only use display bounds for positioning, let SDL handle fullscreen size
		SDL_Rect bounds = {};
		std::ignore = SDL_GetDisplayBounds(id, &bounds);
		rect.x = bounds.x;
		rect.y = bounds.y;
	}

	SdlWindow window{ id, title, rect, flags };

	if ((flags & (SDL_WINDOW_FULLSCREEN)) != 0)
	{
		window.setOffsetX(rect.x);
		window.setOffsetY(rect.y);
	}

	return window;
}
