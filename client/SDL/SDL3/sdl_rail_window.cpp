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

#include <freerdp/codec/color.h>
#include <freerdp/log.h>
#include <freerdp/window.h>

#include "sdl_rail_platform.hpp"
#include "sdl_rail_window.hpp"
#include "sdl_wayland.hpp"
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
	/* Ignore server updates during local WM move. */
	if (_localMoveActive)
		return;
	/* A resize recreates the render target, so the content needs a full re-copy. */
	if ((rect.w != _windowRect.w) || (rect.h != _windowRect.h))
		_painted = false;
	_windowRect = rect;
	_geometryDirty = true;
}

void SdlRailWindow::setLocalMoveActive(bool active)
{
	std::unique_lock lock(_gfxLock);
	_localMoveActive = active;
	if (active)
		_localMoveIsResize = false; /* default to move; setResizeAnchor marks a resize */
}

void SdlRailWindow::setResizeAnchor(bool right, bool bottom)
{
	std::unique_lock lock(_gfxLock);
	_resizeAnchorRight = right;
	_resizeAnchorBottom = bottom;
	_localMoveIsResize = true;
}

bool SdlRailWindow::localMoveActive() const
{
	std::unique_lock lock(_gfxLock);
	return _localMoveActive;
}

void SdlRailWindow::adoptLocalGeometry(const SDL_Rect& rect)
{
	/* Adopt local geometry so the server's echoing WINDOW_ORDER is a no-op (not a size snap). */
	std::unique_lock lock(_gfxLock);
	_windowRect = rect;
	_geometryDirty = false;
	_localMoveActive = false;
	/* Repaint real content (clear placeholder). */
	if (_hasGfx)
		_gfxDamage.assign(1, SDL_Rect{ 0, 0, static_cast<int>(_gfxW), static_cast<int>(_gfxH) });
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

void SdlRailWindow::setMinMaxSize(SDL_Point minSize, SDL_Point maxSize)
{
	std::unique_lock lock(_gfxLock);
	_minSize = { std::max(0, minSize.x), std::max(0, minSize.y) };
	_maxSize = { std::max(0, maxSize.x), std::max(0, maxSize.y) };
	_minMaxDirty = true;
	/* Re-apply geometry: a size the WM clamped to the OLD min (a shrink below it) must be re-sent
	 * now that the new min is looser, or the window stays stuck at the clamped size. */
	_geometryDirty = true;
}

void SdlRailWindow::setResizeMargins(int left, int top, int right, int bottom)
{
	std::unique_lock lock(_gfxLock);
	_resizeMargins = { left, top, right, bottom };
}

SDL_Rect SdlRailWindow::resizeMargins() const
{
	std::unique_lock lock(_gfxLock);
	return _resizeMargins;
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

bool SdlRailWindow::styleResizable() const
{
	/* Resizable only with a sizing border (WS_THICKFRAME), or the WM refuses resize requests. */
	return (_style & WS_THICKFRAME) != 0;
}

void SdlRailWindow::setStyle(uint32_t style, uint32_t exStyle)
{
	std::unique_lock lock(_gfxLock);
	_style = style;
	/* Classify popup once. */
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

	const RailPlatformCaps& caps = railPlatformCaps();

	if (_isPopup && parent)
	{
		/* SDL popup relative to the owner: no taskbar entry, no focus steal, Wayland-positionable.
		 */
		const SDL_Rect rel = { _windowRect.x - parentRect.x, _windowRect.y - parentRect.y,
			                   _windowRect.w, _windowRect.h };
		_win = std::make_unique<SdlWindow>(SdlWindow::createPopup(parent, rel));
	}
	else if (_isPopup && !caps.positionsReadable)
	{
		/* No owner yet: a Wayland popup needs a parent; retry once an app window exists. */
		WLog_VRB(TAG, "popup create deferred id=0x%08x: no parent yet", static_cast<unsigned>(_id));
		return false;
	}
	else
	{
		/* Borderless: the RemoteApp window carries its own server-drawn frame as content. Add
		 * TRANSPARENT (where the compositor can blend it) so the resize placeholder's revealed area
		 * shows the desktop through. Normal frames stay opaque - the server GFX is opaque and
		 * covers the whole window. */
		Uint32 flags = SDL_WINDOW_BORDERLESS;
		if (caps.supportsTransparentWindows)
			flags |= SDL_WINDOW_TRANSPARENT;
		_win = std::make_unique<SdlWindow>(
		    SdlWindow::create(SDL_GetPrimaryDisplay(), _title, flags, _windowRect));
	}
	if (!_win || !_win->window() || !_win->renderer())
	{
		_win.reset();
		WLog_WARN(TAG, "create failed id=0x%08x %s", static_cast<unsigned>(_id),
		          railRole(_isPopup, _layered));
		return false;
	}
	_win->resizeable(styleResizable());

	/* Bind seat/pointer on Wayland. */
	if (!_isPopup && !caps.positionsReadable)
		sdl_wayland_move_prepare(_win->window());
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
				WLog_DBG(TAG, "hide id=0x%08x %s", static_cast<unsigned>(_id),
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


	/* Min/max BEFORE geometry: the WM clamps SDL_SetWindowSize to the current min. The server's
	 * min-track-size only constrains USER resizing - the app itself drives its window below it
	 * (Windows lets SetWindowPos ignore the track min), so honouring it verbatim would block the
	 * server's authoritative geometry.
	 * Observed: the server sometimes sends a min that is inconsistent with the geometry it also
	 * sends. Opening PowerPoint's Options shrinks the main window to 339px, but ~1 in 5-6 times the
	 * server reports that window's min as 500x400 (a stale, pre-transform GetMinMaxInfo) instead of
	 * the small value it usually sends - Windows does not always give us the right min. Enforced
	 * verbatim, that min pins the window at 500 with a transparent gap + a stale band ring.
	 * So clamp the enforced min to the target outer size: server geometry is never blocked, and the
	 * min re-widens on its own once the server grows the window back. Re-run on a geometry change
	 * too, since a shrink needs the min re-clamped first. */
	if (_win && _minMaxDirty)
	{
		SDL_SetWindowMinimumSize(_win->window(), std::max(0, _minSize.x), std::max(0, _minSize.y));
		int maxW = (_maxSize.x > 0) ? std::max(1, _maxSize.x) : 0;
		int maxH = (_maxSize.y > 0) ? std::max(1, _maxSize.y) : 0;
		/* Wayland: cap to the usable area - an oversized window cannot be dragged into reach. */
		SDL_Rect usable{};
		if (!railPlatformCaps().positionsReadable &&
		    SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
		{
			if ((maxW == 0) || (maxW > usable.w))
				maxW = usable.w;
			if ((maxH == 0) || (maxH > usable.h))
				maxH = usable.h;
		}
		SDL_SetWindowMaximumSize(_win->window(), maxW, maxH);
		_minMaxDirty = false;
	}

	if (_win && _geometryDirty)
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
	/* Owned dialog (About/Open): make it transient-for its owner so the WM keeps it above, instead
	 * of letting a click on the owner raise the owner over it. Set once, when the owner window
	 * exists. */
	if (!_isPopup && parent && !_parentApplied)
	{
		if (SDL_SetWindowParent(_win->window(), parent))
			_parentApplied = true;
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
                                     uint32_t height, const RECTANGLE_16* damage, uint32_t nbDamage)
{
	std::unique_lock lock(_gfxLock);
	/* Deep-copy GDI pixels to prevent UAF. */
	const size_t bytes = static_cast<size_t>(stride) * height;
	if (!data || (bytes == 0) || (width == 0) || (height == 0))
	{
		if (_hasGfx)
			WLog_DBG(TAG, "gfx cleared id=0x%08x (surface unmapped)", static_cast<unsigned>(_id));
		_gfxBuffer.clear();
		_hasGfx = false;
		_gfxDamage.clear();
		_gfxStride = stride;
		_gfxW = width;
		_gfxH = height;
		return;
	}

	const auto* src = static_cast<const uint8_t*>(data);
	/* Geometry/stride change, first frame, or no damage rects: full copy + repaint. */
	const bool full = !_hasGfx || (_gfxBuffer.size() != bytes) || (_gfxStride != stride) ||
	                  (_gfxW != width) || (_gfxH != height) || (nbDamage == 0);
	if (full)
	{
		_gfxBuffer.assign(src, src + bytes);
		_gfxDamage.assign(1, SDL_Rect{ 0, 0, static_cast<int>(width), static_cast<int>(height) });
	}
	else
	{
		const SDL_Rect bounds{ 0, 0, static_cast<int>(width), static_cast<int>(height) };
		for (uint32_t i = 0; i < nbDamage; i++)
		{
			const SDL_Rect r{ damage[i].left, damage[i].top, damage[i].right - damage[i].left,
				              damage[i].bottom - damage[i].top };
			SDL_Rect clip{};
			if (!SDL_GetRectIntersection(&r, &bounds, &clip))
				continue;
			std::ignore = freerdp_image_copy_no_overlap(
			    _gfxBuffer.data(), PIXEL_FORMAT_BGRA32, stride, static_cast<UINT32>(clip.x),
			    static_cast<UINT32>(clip.y), static_cast<UINT32>(clip.w),
			    static_cast<UINT32>(clip.h), src, PIXEL_FORMAT_BGRA32, stride,
			    static_cast<UINT32>(clip.x), static_cast<UINT32>(clip.y), nullptr,
			    FREERDP_FLIP_NONE);
			_gfxDamage.push_back(clip);
		}
		/* A hidden window accumulates rects without ever painting; collapse to one full repaint. */
		if (_gfxDamage.size() > 32)
			_gfxDamage.assign(1,
			                  SDL_Rect{ 0, 0, static_cast<int>(width), static_cast<int>(height) });
	}
	_hasGfx = true;
	_gfxStride = stride;
	_gfxW = width;
	_gfxH = height;
	WLog_VRB(TAG, "gfx id=0x%08x %ux%u full=%d nDamage=%u", static_cast<unsigned>(_id), width,
	         height, full ? 1 : 0, nbDamage);
}

void SdlRailWindow::invalidateAll()
{
	std::unique_lock lock(_gfxLock);
	if (_hasGfx)
		_gfxDamage.assign(1, SDL_Rect{ 0, 0, static_cast<int>(_gfxW), static_cast<int>(_gfxH) });
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
	/* The dashed placeholder is only for a local RESIZE (revealed area awaiting the server frame).
	 * A local MOVE also sets _localMoveActive (to freeze server geometry while the WM drags the
	 * window, X11), but must NOT draw the placeholder - the window just moves, nothing is revealed.
	 * Except a WM snap: it resizes the window mid-move, revealing area exactly like a resize, so
	 * detect it by the window size diverging from the content (a plain move never changes size). */
	bool resizing = _localMoveActive && _localMoveIsResize;
	if (_localMoveActive && !resizing)
	{
		int ww = 0;
		int wh = 0;
		SDL_GetWindowSizeInPixels(_win->window(), &ww, &wh);
		resizing = (ww != static_cast<int>(_gfxW)) || (wh != static_cast<int>(_gfxH));
	}

	/* Nothing changed since the last paint and we're not drawing the resize placeholder: keep the
	 * last presented frame. This is what makes a single window's update repaint only that window
	 * instead of every RAIL window on every USER_UPDATE. */
	if (!resizing && _gfxDamage.empty())
		return true;

	SDL_Surface* s =
	    SDL_CreateSurfaceFrom(static_cast<int>(_gfxW), static_cast<int>(_gfxH), format,
	                          _gfxBuffer.data(), static_cast<int>(_gfxStride));
	if (!s)
		return false;

	/* Blit the mapped surface 1:1 (no scaling: mid-resize aspect mismatch would crumple it). During
	 * a local resize the server has not delivered content at the new size yet, so anchor the stale
	 * frame to the fixed corner (a top/left drag keeps the bottom/right edge fixed) and show a flat
	 * fill + dashed border in the newly revealed area - "you dragged the window here, awaiting the
	 * server frame" (like the Windows low-performance resize). */
	if (resizing)
	{
		int ww = 0;
		int wh = 0;
		SDL_GetWindowSizeInPixels(_win->window(), &ww, &wh);
		const SDL_Point off = { _resizeAnchorRight ? (ww - static_cast<int>(_gfxW)) : 0,
			                    _resizeAnchorBottom ? (wh - static_cast<int>(_gfxH)) : 0 };
		std::ignore = _win->paintResizeFrame(s, off, !_gfxDamage.empty());
	}
	else
	{
		/* Upload + render only the damaged rects (accumulated since the last paint); the persistent
		 * render target keeps the rest. */
		std::ignore = _win->drawRects(s, { 0, 0 }, _gfxDamage);
		_win->updateSurface();
	}
	SDL_DestroySurface(s);
	_gfxDamage.clear();
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
		{
			WLog_VRB(TAG, "paintLegacy skip id=0x%08x no-damage", static_cast<unsigned>(_id));
			return true;
		}

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
		WLog_VRB(TAG, "paintLegacy id=0x%08x full=%d visRects=%zu", static_cast<unsigned>(_id),
		         full ? 1 : 0, vis.size());
	}
	return true;
}
