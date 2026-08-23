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
#include "sdl_x11.hpp"

#define TAG CLIENT_TAG("sdl.rail.window")

/* Short role tag so a window's whole lifecycle greps by id in the log. */
static const char* railRole(bool popup, bool layered)
{
	return layered ? "layered" : (popup ? "popup" : "app");
}

/* Check for real per-pixel alpha in region. */
static bool regionHasAlpha(const uint8_t* base, uint32_t stride, const SDL_Rect& r)
{
	for (int y = r.y; y < r.y + r.h; y++)
	{
		const uint8_t* row = base + static_cast<size_t>(y) * stride;
		for (int x = r.x; x < r.x + r.w; x++)
			if (row[static_cast<size_t>(x) * 4 + 3] != 0)
				return true;
	}
	return false;
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
	/* Skip geometry apply while maximized. */
	if (!_maxState.rail)
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
	_visDirty = true;
}

void SdlRailWindow::setVisibleOffset(SDL_Point offset)
{
	std::unique_lock lock(_gfxLock);
	if (_visOffsetSet && (offset.x == _visOffset.x) && (offset.y == _visOffset.y))
		return;
	_visOffset = offset;
	_visOffsetSet = true;
	_visDirty = true;
}

void SdlRailWindow::setMinMaxSize(SDL_Point minSize, SDL_Point maxSize)
{
	std::unique_lock lock(_gfxLock);
	_minSize = { std::max(0, minSize.x), std::max(0, minSize.y) };
	_maxSize = { std::max(0, maxSize.x), std::max(0, maxSize.y) };
	_minMaxDirty = true;
	/* Re-apply geometry on min/max change. */
	_geometryDirty = true;
}

void SdlRailWindow::setResizeMargins(int left, int top, int right, int bottom)
{
	std::unique_lock lock(_gfxLock);
	const SDL_Rect m = { left, top, right, bottom };
	if (SDL_RectsEqual(&m, &_resizeMargins))
		return;
	_resizeMargins = m;
	/* Margins usually arrive after the first frame; the window must regrow to cover them. */
	if (!_maxState.rail)
		_geometryDirty = true;
}

SDL_Rect SdlRailWindow::resizeMargins() const
{
	std::unique_lock lock(_gfxLock);
	return _resizeMargins;
}

void SdlRailWindow::setFrameMargins(const SDL_Rect& m)
{
	std::unique_lock lock(_gfxLock);
	_frameMargins = m;
}

SDL_Rect SdlRailWindow::frameMargins() const
{
	std::unique_lock lock(_gfxLock);
	return _frameMargins;
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
	/* Once resizable, keep band eligibility to avoid oscillation. */
	return _everResizable || ((_style & (WS_THICKFRAME | WS_MAXIMIZEBOX)) != 0);
}

/* Inflate window by resize margins for hit-testing. */
/* Minimum grabbable width for each outside-band resize edge. */
SDL_Rect SdlRailWindow::bandMargins() const
{
	if (!_visible || _isPopup || _layered || !styleResizable() || effectivelyMaximized())
		return { 0, 0, 0, 0 };
	return _resizeMargins;
}

SDL_Rect SdlRailWindow::bandInsets() const
{
	/* X11-only transparent resize ring. */
	const auto& caps = railPlatformCaps();
	if (!caps.positionsReadable || !caps.supportsTransparentWindows)
		return { 0, 0, 0, 0 };
	return bandMargins();
}

/* Add edge insets (x=L y=T w=R h=B) to a server rect to get its outer (band-inclusive) rect. */
SDL_Rect SdlRailWindow::targetOuterRect() const
{
	const SDL_Rect i = bandInsets();
	return { _windowRect.x - i.x, _windowRect.y - i.y, _windowRect.w + i.x + i.w,
		     _windowRect.h + i.y + i.h };
}

/* The insets baked into the window ON SCREEN (not the freshly recomputed target). */
SDL_Rect SdlRailWindow::insets() const
{
	std::unique_lock lock(_gfxLock);
	return _appliedInsets;
}

/* On-screen outer geometry: _windowRect inflated by the applied insets. */
SDL_Rect SdlRailWindow::outerRect() const
{
	std::unique_lock lock(_gfxLock);
	const SDL_Rect& i = _appliedInsets;
	return { _windowRect.x - i.x, _windowRect.y - i.y, _windowRect.w + i.x + i.w,
		     _windowRect.h + i.y + i.h };
}

SDL_Rect SdlRailWindow::serverRect(const SDL_Rect& outer) const
{
	const SDL_Rect i = insets();
	return { outer.x + i.x, outer.y + i.y, outer.w - i.x - i.w, outer.h - i.y - i.h };
}

/* Surface blit offset within the local window (content anchor when maximized). */
SDL_Point SdlRailWindow::blitOffset() const
{
	if (!effectivelyMaximized())
		return { _appliedInsets.x, _appliedInsets.y };
	SDL_Point content = { _frameMargins.x, _frameMargins.y };
	if (_visOffsetSet)
		content = { _visOffset.x - _windowRect.x, _visOffset.y - _windowRect.y };
	return { -std::clamp(content.x, 0, static_cast<int>(_gfxW)),
		     -std::clamp(content.y, 0, static_cast<int>(_gfxH)) };
}

SDL_Point SdlRailWindow::serverOrigin() const
{
	std::unique_lock lock(_gfxLock);
	/* Derive server coordinate from blit offset to match on-screen pixels. */
	const SDL_Point dst = blitOffset();
	return { _windowRect.x - dst.x, _windowRect.y - dst.y };
}

void SdlRailWindow::setStyle(uint32_t style, uint32_t exStyle)
{
	std::unique_lock lock(_gfxLock);
	const bool wasResizable = styleResizable();
	_style = style;
	if ((style & (WS_THICKFRAME | WS_MAXIMIZEBOX)) != 0)
		_everResizable = true;
	/* Update resizability on style change. */
	if (styleResizable() != wasResizable)
		_styleDirty = true;
	/* Classify popup and layered window types once on creation. */
	if (!_popupClassified)
	{
		const bool isDialogOrApp = ((style & (WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
		                                      WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) != 0) ||
		                           ((exStyle & (WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW)) != 0);
		const bool isToolOrPopup =
		    ((exStyle & (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW)) != 0) || ((style & WS_POPUP) != 0);
		_isPopup = isToolOrPopup && !isDialogOrApp;
		_layered = ((exStyle & WS_EX_LAYERED) != 0) && !isDialogOrApp;
		const bool captioned = (style & WS_CAPTION) != 0;
		_layeredApp = ((exStyle & WS_EX_LAYERED) != 0) && captioned;
		_popupClassified = true;
		WLog_INFO(TAG,
		          "[STYLE] id=0x%08" PRIx32 " isPopup=%d layered=%d style=0x%08" PRIx32
		          " ex=0x%08" PRIx32 "",
		          static_cast<uint32_t>(_id), _isPopup ? 1 : 0, _layered ? 1 : 0, style, exStyle);
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

void SdlRailWindow::setIcon(const SdlRailIcon& icon)
{
	std::unique_lock lock(_gfxLock);
	_icon = icon;
	_iconDirty = true;
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

	/* Local window = server rect + band insets (the outside resize band is part of our window). */
	const SDL_Rect vis = targetOuterRect();
	_appliedInsets = bandInsets(); /* the insets baked into the window we are about to create */
	if (_isPopup && parent)
	{
		/* SDL popups position parent-relative (works on Wayland too, via xdg_positioner). */
		const SDL_Rect rel = { vis.x - parentRect.x, vis.y - parentRect.y, vis.w, vis.h };
		_win = std::make_unique<SdlWindow>(
		    SdlWindow::createPopup(parent, rel, caps.supportsTransparentWindows));
	}
	else if (_isPopup && !caps.positionsReadable)
	{
		/* No owner yet: a Wayland popup needs a parent; retry once an app window exists. */
		WLog_VRB(TAG, "popup create deferred id=0x%08" PRIx32 ": no parent yet",
		         static_cast<uint32_t>(_id));
		return false;
	}
	else
	{
		/* Transparent so the band ring outside the content stays invisible. */
		Uint32 flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
		if (caps.supportsTransparentWindows)
			flags |= SDL_WINDOW_TRANSPARENT;
		_win = std::make_unique<SdlWindow>(
		    SdlWindow::create(SDL_GetPrimaryDisplay(), _title, flags, vis));
	}
	if (!_win || !_win->window() || !_win->renderer())
	{
		_win.reset();
		WLog_WARN(TAG, "create failed id=0x%08" PRIx32 " %s", static_cast<uint32_t>(_id),
		          railRole(_isPopup, _layered));
		return false;
	}
	_win->resizeable(styleResizable());

	/* Bind seat/pointer on Wayland. */
	if (!_isPopup && !caps.positionsReadable)
		sdl_wayland_move_prepare(_win->window());

	WLog_DBG(TAG,
	         "create id=0x%08" PRIx32 " sdl=%" PRIu32 " %s vis=%dx%d+%d+%d margins=L%d,T%d,R%d,B%d "
	         "transparent=%d",
	         static_cast<uint32_t>(_id), static_cast<uint32_t>(_win->id()),
	         railRole(_isPopup, _layered), vis.w, vis.h, vis.x, vis.y, _resizeMargins.x,
	         _resizeMargins.y, _resizeMargins.w, _resizeMargins.h,
	         caps.supportsTransparentWindows ? 1 : 0);
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
		_geometryDirty = false;
		/* Shown + raised in paint() after the first frame (created hidden). */
	}

	/* Apply resizability before maximize. */
	if (_win && _styleDirty)
	{
		_win->resizeable(styleResizable());
		_styleDirty = false;
		/* Insets refresh on geometry updates to avoid flicker. */
	}

	/* Apply state before geometry. */
	if (_win)
	{
		applyServerState(_maxState, "maximize", SDL_MaximizeWindow);
		applyServerState(_minState, "minimize", SDL_MinimizeWindow);
	}

	/* Refresh insets on maximize transition. */
	if (_win)
	{
		const bool maxed = effectivelyMaximized();
		if (maxed && !_wasMaximized)
			_appliedInsets = bandInsets();
		_wasMaximized = maxed;
	}

	/* _GTK_FRAME_EXTENTS: tell the WM the band ring is frame, not content (snap/tile geometry). */
	if (_win)
	{
		const SDL_Rect ext = bandInsets();
		if (!SDL_RectsEqual(&ext, &_extentsApplied) &&
		    sdl_x11_set_frame_extents(_win->window(), ext.x, ext.w, ext.y, ext.h))
			_extentsApplied = ext;
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
	if (_win && (_minMaxDirty || _geometryDirty))
	{
		const SDL_Rect vis = targetOuterRect();
		SDL_SetWindowMinimumSize(_win->window(), std::clamp(_minSize.x, 0, vis.w),
		                         std::clamp(_minSize.y, 0, vis.h));
		int maxW = (_maxSize.x > 0) ? std::max(1, _maxSize.x) : 0;
		int maxH = (_maxSize.y > 0) ? std::max(1, _maxSize.y) : 0;
		/* Wayland: cap to the usable area - an oversized window cannot be dragged into reach. */
		SDL_Rect usable{};
		if (!railPlatformCaps().positionsReadable &&
		    SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
		{
			/* The local window is the outer frame: cap to the usable area plus the insets. */

			const int capW = usable.w;
			const int capH = usable.h;
			if ((maxW == 0) || (maxW > capW))
				maxW = capW;
			if ((maxH == 0) || (maxH > capH))
				maxH = capH;
		}
		SDL_SetWindowMaximumSize(_win->window(), maxW, maxH);
		_minMaxDirty = false;
	}

	/* Maximized: WM owns geometry; a server update stays pending until restored. */
	if (_win && _geometryDirty && !_maxState.rail)
	{
		const SDL_Rect vis = targetOuterRect();
		/* The window is (or becomes) vis = _windowRect + fresh insets: record those as the insets
		 * now baked in, so the round-trip back to server coords strips exactly this. */
		_appliedInsets = bandInsets();
		/* Popup coords are relative to the parent's on-screen origin. */
		if (_isPopup && parent)
			SDL_SetWindowPosition(_win->window(), vis.x - parentRect.x, vis.y - parentRect.y);
		else
			SDL_SetWindowPosition(_win->window(), vis.x, vis.y);
		SDL_SetWindowSize(_win->window(), vis.w, vis.h);
		_geometryDirty = false;
		WLog_VRB(TAG, "geom id=0x%08" PRIx32 " vis=%dx%d+%d+%d", static_cast<uint32_t>(_id), vis.w,
		         vis.h, vis.x, vis.y);
	}
	/* Make dialog transient for owner. */
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
	if (_iconDirty)
	{
		/* Apply window icon. */
		if (!_isPopup && !_icon.bgra.empty())
		{
			SDL_Surface* s = SDL_CreateSurfaceFrom(
			    static_cast<int>(_icon.w), static_cast<int>(_icon.h), SDL_PIXELFORMAT_BGRA32,
			    _icon.bgra.data(), static_cast<int>(_icon.w * 4));
			if (s)
			{
				if (!SDL_SetWindowIcon(_win->window(), s))
					SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
					            "SDL_SetWindowIcon failed for window 0x%08" PRIx32 ": %s",
					            static_cast<uint32_t>(_id), SDL_GetError());
				SDL_DestroySurface(s);
			}
		}
		_iconDirty = false;
	}

	/* Defer show until first frame. */
	if (!_minState.rail && _gfxPresented)
		SDL_ShowWindow(_win->window());
	return true;
}

void SdlRailWindow::updateGfxSurface(const void* data, uint32_t stride, uint32_t width,
                                     uint32_t height, const RECTANGLE_16* damage, uint32_t nbDamage,
                                     uint32_t format)
{
	std::unique_lock lock(_gfxLock);
	/* Deep-copy GDI pixels to prevent UAF. */
	const size_t bytes = static_cast<size_t>(stride) * height;
	if (!data || (bytes == 0) || (width == 0) || (height == 0))
	{
		if (_hasGfx)
			WLog_DBG(TAG, "gfx cleared id=0x%08" PRIx32 " (surface unmapped)",
			         static_cast<uint32_t>(_id));
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
		/* Blend real alpha, force opaque otherwise. */
		_gfxHasAlpha = honorsAlpha() &&
		               regionHasAlpha(_gfxBuffer.data(), stride,
		                              { 0, 0, static_cast<int>(width), static_cast<int>(height) });
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
			/* Handle late alpha discovery. */
			if (honorsAlpha() && !_gfxHasAlpha && regionHasAlpha(_gfxBuffer.data(), stride, clip))
				_gfxHasAlpha = true;
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
	WLog_VRB(TAG, "gfx id=0x%08" PRIx32 " %ux%u full=%d nDamage=%u", static_cast<uint32_t>(_id),
	         width, height, full ? 1 : 0, nbDamage);
}

void SdlRailWindow::invalidateAll()
{
	std::unique_lock lock(_gfxLock);
	if (_hasGfx)
		_gfxDamage.assign(1, SDL_Rect{ 0, 0, static_cast<int>(_gfxW), static_cast<int>(_gfxH) });
}

void SdlRailWindow::setServerState(StateSync& s, bool m)
{
	std::unique_lock lock(_gfxLock);
	if (m != s.server)
	{
		s.server = m;
		s.dirty = true;
	}
}

void SdlRailWindow::setServerMaximized(bool m)
{
	setServerState(_maxState, m);
}

void SdlRailWindow::setServerMinimized(bool m)
{
	setServerState(_minState, m);
}

void SdlRailWindow::applyServerState(StateSync& s, const char* what, bool (*enter)(SDL_Window*))
{
	if (!s.dirty)
		return;
	if (s.server && !s.rail)
	{
		s.rail = true;
		WLog_DBG(TAG, "%s id=0x%08" PRIx32 "", what, static_cast<uint32_t>(_id));
		enter(_win->window());
	}
	else if (!s.server && s.rail)
	{
		s.rail = false;
		WLog_DBG(TAG, "restore id=0x%08" PRIx32 " (%s)", static_cast<uint32_t>(_id), what);
		SDL_RestoreWindow(_win->window());
	}
	/* Drain the restore event immediately: without this, an incoming geometry update
	 * applied the restore rect while the window was still maximized. */
	(void)SDL_SyncWindow(_win->window());
	s.dirty = false;
}

bool SdlRailWindow::effectivelyMaximized() const
{
	return _maxState.rail ||
	       (_win && (SDL_GetWindowFlags(_win->window()) & SDL_WINDOW_MAXIMIZED) != 0);
}

void SdlRailWindow::raise()
{
	if (_win)
		SDL_RaiseWindow(_win->window());
}

bool SdlRailWindow::paint(SDL_Surface* primary, SDL_PixelFormat fallbackFormat,
                          const std::vector<SDL_Rect>& damage, SDL_Window* parent,
                          const SDL_Rect& parentRect)
{
	if (!reconcile(parent, parentRect))
		return false;

	bool ok = false;
	{
		std::unique_lock lock(_gfxLock);
		if (_hasGfx)
			ok = paintGfx(fallbackFormat);
		else
		{
			lock.unlock();
			ok = paintLegacy(primary, damage);
		}
	}

	/* Map in the same pass as the first frame, else the window defers a frame (menu lag). */
	if (_gfxPresented && !_mapped && !_minState.rail)
	{
		_mapped = true;
		WLog_DBG(TAG, "map id=0x%08" PRIx32 " %s", static_cast<uint32_t>(_id),
		         railRole(_isPopup, _layered));
		SDL_ShowWindow(_win->window());
		if (!_isPopup && !_layered)
			_win->raise(); /* new app windows come to the front; popups are above by design */
	}
	return ok;
}

/* Caller holds _gfxLock. Blits the window-mapped GFX surface via the shared SdlWindow path. */
bool SdlRailWindow::paintGfx(SDL_PixelFormat format)
{
	/* Blit against the insets actually baked into the window, not a fresh recompute: the content
	 * would otherwise fill over the band ring the instant the server toggles the resizable style.
	 */
	const SDL_Rect bi = _appliedInsets;
	int ww = 0;
	int wh = 0;
	SDL_GetWindowSizeInPixels(_win->window(), &ww, &wh);
	const int cw = ww - bi.x - bi.w; /* content area inside the insets */
	const int ch = wh - bi.y - bi.h;

	/* Detect WM snap divergence. */
	bool localResize = _localMoveActive && _localMoveIsResize;
	if (_localMoveActive && !localResize)
		localResize = (cw != static_cast<int>(_gfxW)) || (ch != static_cast<int>(_gfxH));

	/* Force one repaint on server resize. */
	const bool serverResize = !_localMoveActive && ((ww != _lastWinW) || (wh != _lastWinH));

	/* Skip undamaged frames. */
	if (!localResize && !serverResize && _gfxDamage.empty() && !(_layeredApp && _visDirty))
	{
		WLog_VRB(TAG, "paintGfx skip id=0x%08" PRIx32 " no-damage no-resize",
		         static_cast<uint32_t>(_id));
		return true;
	}

	/* Defer mapping until content arrives. */
	if (_isPopup && !_gfxPresented)
	{
		bool allZero = true;
		for (uint32_t y = 0; allZero && (y < _gfxH); y++)
		{
			const uint8_t* row = _gfxBuffer.data() + static_cast<size_t>(y) * _gfxStride;
			for (uint32_t x = 0; x < _gfxW * 4U; x++)
			{
				if (row[x] != 0)
				{
					allZero = false;
					break;
				}
			}
		}
		if (allZero)
		{
			_gfxDamage.clear();
			return true;
		}
	}

	/* RAIL content is mostly opaque (ignore alpha). Exception: layered app windows. */
	SDL_PixelFormat contentFormat = format;
	if (format == SDL_PIXELFORMAT_BGRA32)
	{
		const bool blend = railPlatformCaps().supportsTransparentWindows;
		if (!blend)
			contentFormat = SDL_PIXELFORMAT_BGRX32;
		else if (_isPopup && !_layered && !_gfxHasAlpha)
		{
			/* Force opaque alpha for legacy GDI popups with zeroed alpha channel. */
			for (uint32_t y = 0; y < _gfxH; y++)
			{
				uint32_t* row = reinterpret_cast<uint32_t*>(_gfxBuffer.data() +
				                                            static_cast<size_t>(y) * _gfxStride);
				for (uint32_t x = 0; x < _gfxW; x++)
					row[x] |= 0xFF000000;
			}
		}
	}

	SDL_Surface* s =
	    SDL_CreateSurfaceFrom(static_cast<int>(_gfxW), static_cast<int>(_gfxH), contentFormat,
	                          _gfxBuffer.data(), static_cast<int>(_gfxStride));
	if (!s)
	{
		WLog_WARN(TAG, "paintGfx id=0x%08" PRIx32 " SDL_CreateSurfaceFrom failed: %s",
		          static_cast<uint32_t>(_id), SDL_GetError());
		return false;
	}

	/* Content blits at the inset offset; the ring outside it is the transparent resize band. */
	if (localResize)
	{
		/* Anchor the stale frame to the fixed corner. */
		const SDL_Point off = { _resizeAnchorRight ? (ww - bi.w - static_cast<int>(_gfxW)) : bi.x,
			                    _resizeAnchorBottom ? (wh - bi.h - static_cast<int>(_gfxH))
			                                        : bi.y };
		/* The "awaiting content" dashes only during a real edge/band resize; a MOVE whose size
		 * the WM changed (snap, untile restore) just shows the clipped stale frame - dashes there
		 * would read as a resize the user never started. */
		std::ignore = _win->paintResizeFrame(s, off, !_gfxDamage.empty(), bi, _localMoveIsResize);
	}
	else
	{
		/* Render accumulated damage or re-blit full surface on bare resize. */
		const SDL_Rect full = { 0, 0, static_cast<int>(_gfxW), static_cast<int>(_gfxH) };
		/* Anchor content to real window position (fixes overhanging maximized borders). */
		SDL_Point dst = { bi.x, bi.y };
		SDL_Point winPos = { 0, 0 };
		const bool maxed = effectivelyMaximized();
		if (maxed)
		{
			dst = { 0, 0 };
			if (railPlatformCaps().positionsReadable)
			{
				SDL_GetWindowPosition(_win->window(), &winPos.x, &winPos.y);
				dst = { _windowRect.x - winPos.x, _windowRect.y - winPos.y };
			}
			/* Inset maximized blit destination for frame margins. */
			if (static_cast<int>(_gfxW) > _windowRect.w)
				dst.x -= _frameMargins.x;
			if (static_cast<int>(_gfxH) > _windowRect.h)
				dst.y -= _frameMargins.y;
		}
		if (_layeredApp && !_visRects.empty() && !maxed)
		{
			/* The layered surface is only defined inside the visibility rects (xf shapes the X
			 * window to them, MS-RDPERP); outside is garbage that would paint a black ring. Clip
			 * the blit to them and leave the ring transparent. A maximized window has no shadow
			 * ring and its visibility rect is inset by the (now-dropped) resize margin, so clipping
			 * to it would cut the top-left edge; draw the full surface instead (handled below). */
			const bool logClip = _visDirty;
			if (_visDirty)
			{
				/* Wipe so newly-excluded regions don't keep stale pixels. */
				std::ignore = _win->fill(static_cast<Uint8>(0), 0, 0, 0);
				_gfxDamage.assign(1, full);
				_visDirty = false;
			}
			const SDL_Point off = { _visOffsetSet ? (_visOffset.x - _windowRect.x) : 0,
				                    _visOffsetSet ? (_visOffset.y - _windowRect.y) : 0 };
			if (logClip)
				WLog_VRB(TAG,
				         "clip id=0x%08" PRIx32 " off=%d,%d nVis=%zu vis0=%dx%d+%d+%d win=%dx%d "
				         "gfx=%ux%u",
				         static_cast<uint32_t>(_id), off.x, off.y, _visRects.size(), _visRects[0].w,
				         _visRects[0].h, _visRects[0].x, _visRects[0].y, ww, wh, _gfxW, _gfxH);
			std::vector<SDL_Rect> fullVec;
			if (_gfxDamage.empty())
				fullVec.push_back(full);
			const std::vector<SDL_Rect>& damageRects = _gfxDamage.empty() ? fullVec : _gfxDamage;
			std::vector<SDL_Rect> draw;
			draw.reserve(damageRects.size() * _visRects.size());
			for (const auto& d : damageRects)
			{
				for (auto v : _visRects)
				{
					v.x += off.x;
					v.y += off.y;
					SDL_Rect part{};
					if (SDL_GetRectIntersection(&d, &v, &part))
						draw.push_back(part);
				}
			}
			if (!draw.empty()) /* empty vector would mean "draw everything" to drawRects */
				std::ignore = _win->drawRects(s, dst, draw);
		}
		else if (_gfxDamage.empty())
			std::ignore = _win->drawRects(s, dst, { full });
		else
			std::ignore = _win->drawRects(s, dst, _gfxDamage);

		_win->updateSurface();
		_gfxPresented = true; /* real content on screen: paint() may now map the window */
	}
	SDL_DestroySurface(s);
	WLog_VRB(TAG, "paintGfx id=0x%08" PRIx32 " mode=%s win=%dx%d dmg=%zu",
	         static_cast<uint32_t>(_id),
	         localResize ? "resize" : (serverResize ? "server-resize" : "gfx"), ww, wh,
	         _gfxDamage.size());
	_gfxDamage.clear();
	_lastWinW = ww;
	_lastWinH = wh;
	return true;
}

bool SdlRailWindow::paintLegacy(SDL_Surface* primary, const std::vector<SDL_Rect>& damage)
{
	SDL_Rect rect{};
	SDL_Rect bi{};
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
			WLog_VRB(TAG, "paintLegacy skip id=0x%08" PRIx32 " no-damage",
			         static_cast<uint32_t>(_id));
			return true;
		}

		rect = _windowRect;
		bi = bandInsets();
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
			SDL_Rect dst = { clipped.x - rect.x + bi.x, clipped.y - rect.y + bi.y, clipped.w,
				             clipped.h };
			if (_win->blit(primary, clipped, dst))
				blitted = true;
			continue;
		}
		for (const auto& d : damage)
		{
			SDL_Rect part{};
			if (!SDL_GetRectIntersection(&clipped, &d, &part))
				continue;
			SDL_Rect dst = { part.x - rect.x + bi.x, part.y - rect.y + bi.y, part.w, part.h };
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
		WLog_VRB(TAG, "paintLegacy id=0x%08" PRIx32 " full=%d visRects=%zu",
		         static_cast<uint32_t>(_id), full ? 1 : 0, vis.size());
		/* Defer mapping until GFX frame arrives to avoid desktop flash. */
	}
	return true;
}
