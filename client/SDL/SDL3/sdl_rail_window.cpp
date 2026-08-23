/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client RAIL window
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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
#include <algorithm>
#include <utility>

#include <winpr/string.h>

#include <freerdp/log.h>
#include <freerdp/window.h>

#include "sdl_rail_window.hpp"
#include "sdl_window.hpp"

#define TAG CLIENT_TAG("sdl.rail.window")

/* Short role tag so a window's whole lifecycle greps by id in the log. */
static const char* railRole(bool popup, bool layered)
{
	return popup ? "popup" : (layered ? "layered" : "app");
}

SdlRailWindow::SdlRailWindow(uint64_t id, const SDL_Rect& rect) : _id(id), _windowRect(rect)
{
}

/* Out of line: SdlWindow is only forward-declared in the header. */
SdlRailWindow::~SdlRailWindow() = default;

uint64_t SdlRailWindow::id() const
{
	return _id;
}

SDL_WindowID SdlRailWindow::sdlId() const
{
	return _win ? _win->id() : 0;
}

SDL_Window* SdlRailWindow::window() const
{
	return _win ? _win->window() : nullptr;
}

SDL_Renderer* SdlRailWindow::renderer() const
{
	return _win ? _win->renderer() : nullptr;
}

void SdlRailWindow::updateWindowRect(const SDL_Rect& rect)
{
	std::unique_lock lock(_gfxLock);
	/* A resize recreates the render target, so the content needs a full re-copy. */
	if ((rect.w != _windowRect.w) || (rect.h != _windowRect.h))
		_painted = false;
	_windowRect = rect;
	_geometryDirty = true;
}

SDL_Rect SdlRailWindow::windowRect() const
{
	std::unique_lock lock(_gfxLock);
	return _windowRect;
}

void SdlRailWindow::markDeleted()
{
	std::unique_lock lock(_gfxLock);
	_deleted = true;
}

bool SdlRailWindow::isDeleted() const
{
	std::unique_lock lock(_gfxLock);
	return _deleted;
}

void SdlRailWindow::setVisibilityRects(std::vector<SDL_Rect> rects)
{
	std::unique_lock lock(_gfxLock);
	_visRects = std::move(rects);
}

void SdlRailWindow::setOwner(uint64_t ownerId)
{
	std::unique_lock lock(_gfxLock);
	_ownerId = ownerId;
}

uint64_t SdlRailWindow::owner() const
{
	std::unique_lock lock(_gfxLock);
	return _ownerId;
}

bool SdlRailWindow::isPopup() const
{
	std::unique_lock lock(_gfxLock);
	return _isPopup;
}

void SdlRailWindow::setStyle(uint32_t style, uint32_t exStyle)
{
	std::unique_lock lock(_gfxLock);
	_style = style;
	/* Classify popup and layered window types once on creation. */
	if (!_popupClassified)
	{
		_isPopup = ((style & WS_POPUP) != 0) && ((style & WS_CAPTION) != WS_CAPTION);
		/* WS_EX_LAYERED = shadow/glass decorations; no usable alpha, so skip them (else black
		 * frames). */
		_layered = (exStyle & WS_EX_LAYERED) != 0;
		_popupClassified = true;
	}
}

void SdlRailWindow::setTitle(const std::string& title)
{
	std::unique_lock lock(_gfxLock);
	_title = title;
	_titleDirty = true;
}

void SdlRailWindow::setVisible(bool visible)
{
	std::unique_lock lock(_gfxLock);
	_visible = visible;
}

void SdlRailWindow::setTitle(const char16_t* str, size_t lenBytes)
{
	const size_t chars = lenBytes / sizeof(char16_t);
	size_t outLen = 0;
	char* utf8 = ConvertWCharNToUtf8Alloc(reinterpret_cast<const WCHAR*>(str), chars, &outLen);
	if (!utf8)
		return;
	setTitle(std::string(utf8, outLen));
	free(utf8);
}

/* Caller holds _gfxLock. */
bool SdlRailWindow::create(SDL_Window* parent, const SDL_Rect& parentRect)
{
	if (_win)
		return true;

	const char* driver = SDL_GetCurrentVideoDriver();
	const bool wayland = driver && (strcmp(driver, "wayland") == 0);

	if (_isPopup && parent)
	{
		/* SDL popup relative to the owner: no taskbar entry, no focus steal, Wayland-positionable.
		 */
		const SDL_Rect rel = { _windowRect.x - parentRect.x, _windowRect.y - parentRect.y,
			                   _windowRect.w, _windowRect.h };
		_win = std::make_unique<SdlWindow>(SdlWindow::createPopup(parent, rel));
	}
	else if (_isPopup && wayland)
	{
		/* No owner yet: a Wayland popup needs a parent; retry once an app window exists. */
		return false;
	}
	else
	{
		/* Borderless: the RemoteApp window carries its own server-drawn frame as content. */
		_win = std::make_unique<SdlWindow>(
		    SdlWindow::create(SDL_GetPrimaryDisplay(), _title, SDL_WINDOW_BORDERLESS, _windowRect));
	}
	if (!_win || !_win->window() || !_win->renderer())
	{
		_win.reset();
		WLog_WARN(TAG, "create failed id=0x%08" PRIx32 " %s", static_cast<uint32_t>(_id),
		          railRole(_isPopup, _layered));
		return false;
	}
	return true;
}

bool SdlRailWindow::reconcile(SDL_Window* parent, const SDL_Rect& parentRect)
{
	std::unique_lock lock(_gfxLock);

	/* Realize only visible app windows. */
	const bool drawable = _visible && !_layered && (_windowRect.w > 0) && (_windowRect.h > 0);

	if (!drawable)
	{
		if (_win)
		{
			if ((SDL_GetWindowFlags(_win->window()) & SDL_WINDOW_HIDDEN) == 0)
				WLog_DBG(TAG, "hide id=0x%08" PRIx32 " %s", static_cast<uint32_t>(_id),
				         railRole(_isPopup, _layered));
			SDL_HideWindow(_win->window());
		}
		return false;
	}

	if (!_win)
	{
		if (!create(parent, parentRect))
			return false;
		_geometryDirty = false; /* created at the current rect already */
		if (!_isPopup && !_layered)
			_win->raise(); /* new app windows come to the front; popups are above by design */
	}
	else if (_geometryDirty)
	{
		/* Popup coordinates are relative to the parent window origin. */
		if (_isPopup && parent)
			SDL_SetWindowPosition(_win->window(), _windowRect.x - parentRect.x,
			                      _windowRect.y - parentRect.y);
		else
			SDL_SetWindowPosition(_win->window(), _windowRect.x, _windowRect.y);
		SDL_SetWindowSize(_win->window(), _windowRect.w, _windowRect.h);
		_geometryDirty = false;
	}
	if (_titleDirty)
	{
		if (!_isPopup)
			SDL_SetWindowTitle(_win->window(), _title.c_str());
		_titleDirty = false;
	}
	SDL_ShowWindow(_win->window());
	return true;
}

void SdlRailWindow::updateGfxSurface(const void* data, uint32_t stride, uint32_t width,
                                     uint32_t height)
{
	std::unique_lock lock(_gfxLock);
	_gfxData = data;
	_gfxStride = stride;
	_gfxW = width;
	_gfxH = height;
	_hasGfx = (data != nullptr) && (width > 0) && (height > 0);
}

bool SdlRailWindow::hasGfx()
{
	std::unique_lock lock(_gfxLock);
	return _hasGfx;
}

bool SdlRailWindow::paint(SDL_Surface* primary, SDL_PixelFormat fallbackFormat,
                          const std::vector<SDL_Rect>& damage, SDL_Window* parent,
                          const SDL_Rect& parentRect)
{
	if (!reconcile(parent, parentRect))
		return false;

	std::unique_lock lock(_gfxLock);
	if (_hasGfx)
		return paintGfx(fallbackFormat);
	lock.unlock();
	return paintLegacy(primary, damage);
}

/* Caller holds _gfxLock. Blits the window-mapped GFX surface via the shared SdlWindow path. */
bool SdlRailWindow::paintGfx(SDL_PixelFormat format)
{
	SDL_Surface* s =
	    SDL_CreateSurfaceFrom(static_cast<int>(_gfxW), static_cast<int>(_gfxH), format,
	                          const_cast<void*>(_gfxData), static_cast<int>(_gfxStride));
	if (!s)
		return false;

	int ww = 0;
	int wh = 0;
	SDL_GetWindowSizeInPixels(_win->window(), &ww, &wh);
	/* Scale the mapped surface to fill the window (honours MapSurfaceToScaledWindow). */
	const SDL_FPoint scale = { _gfxW ? static_cast<float>(ww) / static_cast<float>(_gfxW) : 1.0f,
		                       _gfxH ? static_cast<float>(wh) / static_cast<float>(_gfxH) : 1.0f };
	std::ignore =
	    _win->drawScaledRect(s, scale, { 0, 0, static_cast<int>(_gfxW), static_cast<int>(_gfxH) });
	_win->updateSurface();
	SDL_DestroySurface(s);
	return true;
}

bool SdlRailWindow::paintLegacy(SDL_Surface* primary, const std::vector<SDL_Rect>& damage)
{
	SDL_Rect rect{};
	std::vector<SDL_Rect> vis;
	bool full = false;
	{
		std::unique_lock lock(_gfxLock);
		/* Layered decorations have no meaningful content in the shared primary. */
		if (!primary || _layered || (_windowRect.w <= 0) || (_windowRect.h <= 0))
			return true;

		/* Damage-driven: re-copy only server-updated regions, keep the last frame elsewhere. */
		full = !_painted;
		if (!full && damage.empty())
			return true;

		rect = _windowRect;
		vis = _visRects;
	}

	if (vis.empty())
		vis.push_back({ 0, 0, rect.w, rect.h });

	/* Paint only the visible sub-rects (stacked windows don't bleed through), clamped and 1:1. */
	const SDL_Rect bounds = { 0, 0, primary->w, primary->h };
	bool blitted = false;
	for (const auto& v : vis)
	{
		SDL_Rect src = { rect.x + v.x, rect.y + v.y, v.w, v.h };
		SDL_Rect clipped{};
		if (!SDL_GetRectIntersection(&src, &bounds, &clipped))
			continue;

		if (full)
		{
			SDL_Rect dst = { clipped.x - rect.x, clipped.y - rect.y, clipped.w, clipped.h };
			if (_win->blit(primary, clipped, dst))
				blitted = true;
			continue;
		}
		for (const auto& d : damage)
		{
			SDL_Rect part{};
			if (!SDL_GetRectIntersection(&clipped, &d, &part))
				continue;
			SDL_Rect dst = { part.x - rect.x, part.y - rect.y, part.w, part.h };
			if (_win->blit(primary, part, dst))
				blitted = true;
		}
	}
	if (blitted)
	{
		{
			std::unique_lock lock(_gfxLock);
			_painted = true;
		}
		_win->updateSurface();
	}
	return true;
}
