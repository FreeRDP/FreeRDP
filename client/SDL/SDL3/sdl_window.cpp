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
#include <limits>
#include <sstream>
#include <cmath>

#include "sdl_window.hpp"
#include "sdl_utils.hpp"

#include <freerdp/utils/string.h>

SdlWindow::SdlWindow(SDL_DisplayID id, const std::string& title, const SDL_Rect& rect,
                     [[maybe_unused]] Uint32 flags)
    : _displayID(id)
{
	auto props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, rect.x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, rect.y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, rect.w);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, rect.h);

	if (flags & SDL_WINDOW_HIGH_PIXEL_DENSITY)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);

	if (flags & SDL_WINDOW_FULLSCREEN)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);

	if (flags & SDL_WINDOW_BORDERLESS)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);

	_window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);

	auto sc = scale();
	const int iscale = static_cast<int>(sc * 100.0f);
	auto w = 100 * rect.w / iscale;
	auto h = 100 * rect.h / iscale;
	std::ignore = resize({ w, h });
	SDL_SetHint(SDL_HINT_APP_NAME, "");
	std::ignore = SDL_SyncWindow(_window);

	_monitor = query(_window, id, true);
}

SdlWindow::SdlWindow(SdlWindow&& other) noexcept
    : _window(other._window), _displayID(other._displayID), _offset_x(other._offset_x),
      _offset_y(other._offset_y), _monitor(other._monitor)
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
	return rect(_window);
}

SDL_Rect SdlWindow::bounds() const
{
	SDL_Rect rect = {};
	if (_window)
	{
		if (!SDL_GetWindowPosition(_window, &rect.x, &rect.y))
			return {};
		if (!SDL_GetWindowSize(_window, &rect.w, &rect.h))
			return {};
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
	auto m = _monitor;
	if (isPrimary)
	{
		m.x = 0;
		m.y = 0;
	}
	return m;
}

void SdlWindow::setMonitor(rdpMonitor monitor)
{
	_monitor = monitor;
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
	return fill(_window, r, g, b, a);
}

bool SdlWindow::fill(SDL_Window* window, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	auto surface = SDL_GetWindowSurface(window);
	if (!surface)
		return false;
	SDL_Rect rect = { 0, 0, surface->w, surface->h };
	auto color = SDL_MapSurfaceRGBA(surface, r, g, b, a);

	return SDL_FillSurfaceRect(surface, &rect, color);
}

rdpMonitor SdlWindow::query(SDL_Window* window, SDL_DisplayID id, bool forceAsPrimary)
{
	if (!window)
		return {};

	const auto& r = rect(window, forceAsPrimary);
	const float factor = SDL_GetWindowDisplayScale(window);
	const float dpi = std::roundf(factor * 100.0f);

	WINPR_ASSERT(r.w > 0);
	WINPR_ASSERT(r.h > 0);

	const auto primary = SDL_GetPrimaryDisplay();
	const auto orientation = SDL_GetCurrentDisplayOrientation(id);
	const auto rdp_orientation = sdl::utils::orientaion_to_rdp(orientation);

	rdpMonitor monitor{};
	monitor.orig_screen = id;
	monitor.x = r.x;
	monitor.y = r.y;
	monitor.width = r.w;
	monitor.height = r.h;
	monitor.is_primary = forceAsPrimary || (id == primary);
	monitor.attributes.desktopScaleFactor = static_cast<UINT32>(dpi);
	monitor.attributes.deviceScaleFactor = 100;
	monitor.attributes.orientation = rdp_orientation;
	monitor.attributes.physicalWidth = WINPR_ASSERTING_INT_CAST(uint32_t, r.w);
	monitor.attributes.physicalHeight = WINPR_ASSERTING_INT_CAST(uint32_t, r.h);

	const auto cat = SDL_LOG_CATEGORY_APPLICATION;
	SDL_LogDebug(cat, "monitor.orig_screen                   %" PRIu32, monitor.orig_screen);
	SDL_LogDebug(cat, "monitor.x                             %" PRId32, monitor.x);
	SDL_LogDebug(cat, "monitor.y                             %" PRId32, monitor.y);
	SDL_LogDebug(cat, "monitor.width                         %" PRId32, monitor.width);
	SDL_LogDebug(cat, "monitor.height                        %" PRId32, monitor.height);
	SDL_LogDebug(cat, "monitor.is_primary                    %" PRIu32, monitor.is_primary);
	SDL_LogDebug(cat, "monitor.attributes.desktopScaleFactor %" PRIu32,
	             monitor.attributes.desktopScaleFactor);
	SDL_LogDebug(cat, "monitor.attributes.deviceScaleFactor  %" PRIu32,
	             monitor.attributes.deviceScaleFactor);
	SDL_LogDebug(cat, "monitor.attributes.orientation        %s",
	             freerdp_desktop_rotation_flags_to_string(monitor.attributes.orientation));
	SDL_LogDebug(cat, "monitor.attributes.physicalWidth      %" PRIu32,
	             monitor.attributes.physicalWidth);
	SDL_LogDebug(cat, "monitor.attributes.physicalHeight     %" PRIu32,
	             monitor.attributes.physicalHeight);
	return monitor;
}

SDL_Rect SdlWindow::rect(SDL_Window* window, bool forceAsPrimary)
{
	SDL_Rect rect = {};
	if (!window)
		return {};

	if (!forceAsPrimary)
	{
		if (!SDL_GetWindowPosition(window, &rect.x, &rect.y))
			return {};
	}

	if (!SDL_GetWindowSizeInPixels(window, &rect.w, &rect.h))
		return {};

	return rect;
}

SdlWindow::HighDPIMode SdlWindow::isHighDPIWindowsMode(SDL_Window* window)
{
	if (!window)
		return MODE_INVALID;

	const auto id = SDL_GetDisplayForWindow(window);
	if (id == 0)
		return MODE_INVALID;

	const auto cs = SDL_GetDisplayContentScale(id);
	const auto ds = SDL_GetWindowDisplayScale(window);
	const auto pd = SDL_GetWindowPixelDensity(window);

	/* mac os x style, but no HighDPI display */
	if ((cs == 1.0f) && (ds == 1.0f) && (pd == 1.0f))
		return MODE_NONE;

	/* mac os x style HighDPI */
	if ((cs == 1.0f) && (ds > 1.0f) && (pd > 1.0f))
		return MODE_MACOS;

	/* rest is windows style */
	return MODE_WINDOWS;
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

	SDL_Rect rect = { static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(id)),
		              static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(id)), static_cast<int>(width),
		              static_cast<int>(height) };

	if ((flags & SDL_WINDOW_FULLSCREEN) != 0)
	{
		std::ignore = SDL_GetDisplayBounds(id, &rect);
	}

	SdlWindow window{ id, title, rect, flags };

	if ((flags & (SDL_WINDOW_FULLSCREEN)) != 0)
	{
		window.setOffsetX(rect.x);
		window.setOffsetY(rect.y);
	}

	return window;
}

static SDL_Window* createDummy(SDL_DisplayID id)
{
	const auto x = SDL_WINDOWPOS_CENTERED_DISPLAY(id);
	const auto y = SDL_WINDOWPOS_CENTERED_DISPLAY(id);
	const int w = 64;
	const int h = 64;

	auto props = SDL_CreateProperties();
	std::stringstream ss;
	ss << "SdlWindow::query(" << id << ")";
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, ss.str().c_str());
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, w);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, h);

	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, false);

	auto window = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);
	return window;
}

rdpMonitor SdlWindow::query(SDL_DisplayID id, bool forceAsPrimary)
{
	std::unique_ptr<SDL_Window, void (*)(SDL_Window*)> window(createDummy(id), SDL_DestroyWindow);
	if (!window)
		return {};

	std::unique_ptr<SDL_Renderer, void (*)(SDL_Renderer*)> renderer(
	    SDL_CreateRenderer(window.get(), nullptr), SDL_DestroyRenderer);

	if (!SDL_SyncWindow(window.get()))
		return {};

	SDL_Event event{};
	while (SDL_PollEvent(&event))
		;

	return query(window.get(), id, forceAsPrimary);
}

SDL_Rect SdlWindow::rect(SDL_DisplayID id, bool forceAsPrimary)
{
	std::unique_ptr<SDL_Window, void (*)(SDL_Window*)> window(createDummy(id), SDL_DestroyWindow);
	if (!window)
		return {};

	std::unique_ptr<SDL_Renderer, void (*)(SDL_Renderer*)> renderer(
	    SDL_CreateRenderer(window.get(), nullptr), SDL_DestroyRenderer);

	if (!SDL_SyncWindow(window.get()))
		return {};

	SDL_Event event{};
	while (SDL_PollEvent(&event))
		;

	return rect(window.get(), forceAsPrimary);
}
