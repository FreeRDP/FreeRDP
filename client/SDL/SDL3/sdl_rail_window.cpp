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
	if (_localMoveActive)
	{
		/* The WM owns the POSITION during a local drag, but during a plain move the SIZE stays
		 * server-owned (the server drag-restores a maximized window mid-move while the WM keeps
		 * the grabbed frame at the old size): adopt size changes so the completion doesn't report
		 * a stale maximized size back as normal geometry. */
		if (!_localMoveIsResize && ((rect.w != _windowRect.w) || (rect.h != _windowRect.h)))
		{
			WLog_DBG(TAG, "size adopted mid-move id=0x%08x %dx%d -> %dx%d at %d,%d",
			         static_cast<unsigned>(_id), _windowRect.w, _windowRect.h, rect.w, rect.h,
			         rect.x, rect.y);
			_windowRect.w = rect.w;
			_windowRect.h = rect.h;
			/* Record where the server re-anchored its restored window: the completion release must
			 * measure its delta from THIS origin, not the local drag-start. */
			_localMoveServerPos = { rect.x, rect.y };
			_localMoveSizeChanged = true;
			_painted = false;
		}
		return;
	}
	/* Post-move echoes are ACCEPTED (server-authoritative, like xf): the modal loop moves the
	 * server window by the POINTER delta, which can differ from the WM's window delta by a few px,
	 * and the server ignores ClientWindowMove corrections after the loop. Adopting re-converges
	 * each move; left alone the slip accumulates and new popups (Office ribbons) land offset. */
	/* A resize recreates the render target, so the content needs a full re-copy. */
	if ((rect.w != _windowRect.w) || (rect.h != _windowRect.h))
		_painted = false;
	_windowRect = rect;
	/* Skip geometry apply while maximized: the maximized rect itself lands here too, and marking
	 * it pending would re-apply it as NORMAL geometry the moment a restore clears the gate -
	 * reported back, it poisons the server's restore rect to near-fullscreen. The server resends
	 * the real geometry after a restore, so nothing is lost. */
	if (!maxDeclared())
		_geometryDirty = true;
}

void SdlRailWindow::setLocalMoveActive(bool active)
{
	std::unique_lock lock(_gfxLock);
	_localMoveActive = active;
	if (active)
	{
		_localMoveIsResize = false; /* default to move; setResizeAnchor marks a resize */
		_localMoveSizeChanged = false;
	}
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

bool SdlRailWindow::localMoveSizeChanged() const
{
	std::unique_lock lock(_gfxLock);
	return _localMoveSizeChanged;
}

SDL_Point SdlRailWindow::localMoveServerPos() const
{
	std::unique_lock lock(_gfxLock);
	return _localMoveServerPos;
}

void SdlRailWindow::adoptLocalGeometry(const SDL_Rect& rect)
{
	/* Adopt local geometry so the server's echoing WINDOW_ORDER is a no-op (not a size snap). */
	std::unique_lock lock(_gfxLock);
	/* A completed local move settles geometry via the completion path, not the reporter. */
	_geomApplyPending = false;
	/* Translate the (frozen) visible offset by the move delta so the layered clip's
	 * (_visOffset - _windowRect) is preserved across the move - a rigid translation keeps it. */
	if (_visOffsetSet)
	{
		_visOffset.x += rect.x - _windowRect.x;
		_visOffset.y += rect.y - _windowRect.y;
	}
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

bool SdlRailWindow::geomApplyPending() const
{
	std::unique_lock lock(_gfxLock);
	return _geomApplyPending;
}

void SdlRailWindow::clearGeomApplyPending()
{
	std::unique_lock lock(_gfxLock);
	_geomApplyPending = false;
}

void SdlRailWindow::armLoopEnd()
{
	std::unique_lock lock(_gfxLock);
	_loopEnd.pending = true;
}

bool SdlRailWindow::loopEndPending() const
{
	std::unique_lock lock(_gfxLock);
	return _loopEnd.pending;
}

void SdlRailWindow::deferMaximize()
{
	std::unique_lock lock(_gfxLock);
	_loopEnd.maximize = true;
}

void SdlRailWindow::deferSnap(const SDL_Rect& serverRect)
{
	std::unique_lock lock(_gfxLock);
	_loopEnd.snap = true;
	_loopEnd.snapRect = serverRect;
}

void SdlRailWindow::clearLoopEnd()
{
	std::unique_lock lock(_gfxLock);
	_loopEnd = {};
}

SdlRailWindow::LoopEndActions SdlRailWindow::takeLoopEnd()
{
	std::unique_lock lock(_gfxLock);
	const LoopEndActions actions{ _loopEnd.maximize, _loopEnd.snap, _loopEnd.snapRect };
	_loopEnd = {};
	return actions;
}

bool SdlRailWindow::takeWmOverride(SDL_Rect& outer)
{
	std::unique_lock lock(_gfxLock);
	if (!_wmRefused)
		return false;
	outer = _wmRefusedOuter;
	_wmRefused = false;
	return true;
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
	/* While _windowRect is frozen during a local (WM) move, the visible offset must freeze with
	 * it, or the layered clip's (_visOffset - _windowRect) shift drifts and the content renders
	 * offset from where input lands. adoptLocalGeometry keeps the two in step across the move. */
	if (_localMoveActive)
		return;
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
	/* Re-apply geometry: a size the WM clamped to the OLD min (a shrink below it) must be re-sent
	 * now that the new min is looser, or the window stays stuck at the clamped size. */
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
	if (!railMaximized())
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

bool SdlRailWindow::isFullscreen() const
{
	std::unique_lock lock(_gfxLock);
	return _fullscreen;
}

bool SdlRailWindow::isLayered() const
{
	std::unique_lock lock(_gfxLock);
	return _layered;
}

void SdlRailWindow::setShadowAnchored(bool anchored)
{
	std::unique_lock lock(_gfxLock);
	_shadowAnchored = anchored;
}

bool SdlRailWindow::styleResizable() const
{
	/* Sizing border or maximize box: WMs refuse to maximize non-resizable windows. Latched: apps
	 * (PowerPoint's Options dialog) drop WS_THICKFRAME on every activation change and re-add it a
	 * beat later. Honouring each flap toggles SDL's resizable flag, which makes the WM re-frame and
	 * shift the window by the band inset - an endless move/resize oscillation. Resizability is a
	 * stable window property, so once a window is seen resizable it stays so for the band. */
	return _everResizable || ((_style & (WS_THICKFRAME | WS_MAXIMIZEBOX)) != 0);
}

/* Minimum grabbable width for each outside-band resize edge. */
static constexpr int kMinGrip = 4;

SDL_Rect SdlRailWindow::bandMargins() const
{
	if (!_visible || _isPopup || _layered || !styleResizable() || effectivelyMaximized())
		return { 0, 0, 0, 0 };
	/* Floor each edge to a minimum grip so a resizable window stays grabbable even where the server
	 * reports a zero margin (e.g. Office draws its own top caption; the DWM frame is intermittently
	 * not realized at announce time). Gated by the eligibility check above - never on popups,
	 * non-resizable, or maximized windows. */
	const SDL_Rect& m = _resizeMargins;
	return { std::max(m.x, kMinGrip), std::max(m.y, kMinGrip), std::max(m.w, kMinGrip),
		     std::max(m.h, kMinGrip) };
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
static SDL_Rect addInsets(const SDL_Rect& r, const SDL_Rect& i)
{
	return { r.x - i.x, r.y - i.y, r.w + i.x + i.w, r.h + i.y + i.h };
}

/* Inverse of addInsets: strip the insets back off an outer rect to recover the server rect.
 */
static SDL_Rect stripInsets(const SDL_Rect& r, const SDL_Rect& i)
{
	return { r.x + i.x, r.y + i.y, r.w - i.x - i.w, r.h - i.y - i.h };
}

/* Server rect inflated by the FRESH band insets (the target the window should become); caller
 * holds _gfxLock. Used by reconcile/create to size the window. */
SDL_Rect SdlRailWindow::targetOuterRect() const
{
	return addInsets(_windowRect, bandInsets());
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
	return addInsets(_windowRect, _appliedInsets);
}

SDL_Rect SdlRailWindow::serverRect(const SDL_Rect& outer) const
{
	return stripInsets(outer, insets());
}

void SdlRailWindow::setStyle(uint32_t style, uint32_t exStyle)
{
	std::unique_lock lock(_gfxLock);
	const bool wasResizable = styleResizable();
	_style = style;
	if ((style & (WS_THICKFRAME | WS_MAXIMIZEBOX)) != 0)
		_everResizable = true;
	/* Re-apply to SDL only on a real (latched) resizability change - not on the per-activation
	 * WS_THICKFRAME flapping, which styleResizable() now absorbs. */
	if (styleResizable() != wasResizable)
		_styleDirty = true;
	/* Popup-ness decides the SDL window type; classify once so it cannot flip after create. */
	if (!_popupClassified)
	{
		_isPopup = ((style & WS_POPUP) != 0) && ((style & WS_CAPTION) != WS_CAPTION);
		/* Layered without a caption/frame = shadow/decoration surface; captioned layered windows
		 * are real app windows. */
		const bool captioned =
		    ((style & WS_CAPTION) == WS_CAPTION) || ((style & WS_THICKFRAME) != 0);
		_layered = ((exStyle & WS_EX_LAYERED) != 0) && !captioned;
		/* Preserve alpha on layered app windows. */
		_layeredApp = ((exStyle & WS_EX_LAYERED) != 0) && captioned;
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
	/* A WS_POPUP spanning the whole monitor (e.g. a PowerPoint slideshow) is a fullscreen window,
	 * not a menu. On Wayland an xdg_popup is parent-anchored and strut-constrained: it cannot reach
	 * 0,0 or cover the panel, so realize it as a fullscreen toplevel instead. X11 override-redirect
	 * popups already cover the screen, so this is Wayland-only. */
	SDL_Rect disp{};
	const bool fullscreen = _isPopup && !caps.positionsReadable &&
	                        SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &disp) &&
	                        (_windowRect.w >= disp.w) && (_windowRect.h >= disp.h);
	if (_isPopup && parent && !fullscreen)
	{
		/* SDL popups position parent-relative (works on Wayland too, via xdg_popup). */
		const SDL_Rect rel = { vis.x - parentRect.x, vis.y - parentRect.y, vis.w, vis.h };
		_win = std::make_unique<SdlWindow>(
		    SdlWindow::createPopup(parent, rel, caps.supportsTransparentWindows));
	}
	else if (_isPopup && !caps.positionsReadable && !fullscreen)
	{
		/* No owner yet: a Wayland popup needs a parent; retry once an app window exists. */
		WLog_VRB(TAG, "popup create deferred id=0x%08x: no parent yet", static_cast<unsigned>(_id));
		return false;
	}
	else
	{
		/* Transparent so the band ring outside the content stays invisible. */
		Uint32 flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
		if (caps.supportsTransparentWindows)
			flags |= SDL_WINDOW_TRANSPARENT;
		/* Use OpenGL for _NET_WM_SYNC_REQUEST X11 resize sync. */
		if (caps.positionsReadable)
			flags |= SDL_WINDOW_OPENGL;
		_win = std::make_unique<SdlWindow>(
		    SdlWindow::create(SDL_GetPrimaryDisplay(), _title, flags, vis));
	}
	if (!_win || !_win->window() || !_win->renderer())
	{
		_win.reset();
		WLog_WARN(TAG, "create failed id=0x%08x %s", static_cast<unsigned>(_id),
		          railRole(_isPopup, _layered));
		return false;
	}
	_win->resizeable(styleResizable());
	if (fullscreen)
	{
		/* Cover the whole output (panel included) - the Wayland-correct way to reach 0,0. */
		_fullscreen = true;
		SDL_SetWindowFullscreen(_win->window(), true);
		WLog_DBG(TAG, "fullscreen id=0x%08x %dx%d (WS_POPUP spans display)",
		         static_cast<unsigned>(_id), _windowRect.w, _windowRect.h);
	}

	/* Bind seat/pointer on Wayland. */
	if (!_isPopup && !caps.positionsReadable)
		sdl_wayland_move_prepare(_win->window());

	WLog_DBG(TAG,
	         "create id=0x%08x sdl=%u %s vis=%dx%d+%d+%d margins=L%d,T%d,R%d,B%d "
	         "transparent=%d",
	         static_cast<unsigned>(_id), static_cast<unsigned>(_win->id()),
	         railRole(_isPopup, _layered), vis.w, vis.h, vis.x, vis.y, _resizeMargins.x,
	         _resizeMargins.y, _resizeMargins.w, _resizeMargins.h,
	         caps.supportsTransparentWindows ? 1 : 0);
	return true;
}

bool SdlRailWindow::reconcile(SDL_Window* parent, const SDL_Rect& parentRect)
{
	std::unique_lock lock(_gfxLock);

	/* Hidden windows get no local SDL window. Layered decorations (drop shadows) are realized only
	 * in GFX mode, only while the server declares visible regions for them (an idle overlay with
	 * no vis rects would just cover the app window and ghost its stale alpha), and only anchored
	 * to a visible popup (menu/tooltip shadows; app-window edge shadows lag local moves and cover
	 * the resize margins). */
	const bool drawable = _visible && (_windowRect.w > 0) && (_windowRect.h > 0) &&
	                      (!_layered || (_hasGfx && !_visRects.empty() && _shadowAnchored));

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
		_geometryDirty = false;
		/* Shown + raised in paint() after the first frame (created hidden). */
	}

	/* create() returned false on failure, so _win is non-null from here down. */

	/* Resizability first: SDL refuses to maximize a non-resizable window. */
	if (_styleDirty)
	{
		_win->resizeable(styleResizable());
		_styleDirty = false;
		/* Deliberately NOT re-arming _geometryDirty here: apps toggle the resizable style on every
		 * activation change (click behind a modal Office dialog), and resizing the window to add or
		 * drop the band on each toggle makes it flicker. The band tracks _appliedInsets, which only
		 * refreshes on a real geometry order - so it stays put across the noise and self-corrects
		 * the next time the server actually moves/sizes the window. */
	}

	/* State first: maximized/minimized gates the geometry apply below. */
	applyServerState(_maxState, "maximize", SDL_MaximizeWindow);
	applyServerState(_minState, "minimize", SDL_MinimizeWindow);

	/* Maximizing (server- OR WM-driven) drops the band ring, but the geometry apply below - the
	 * only place that refreshes the baked insets - is gated off while maximized, so refresh them on
	 * the become-maximized edge or they stay stale, painting a phantom band and skewing the
	 * server-coord round-trip (outerRect/insets). Edge-triggered on effectivelyMaximized() (catches
	 * a WM maximize that never touches _maxState); restore, which the lagging SDL flag keeps
	 * "maximized" for a few frames, is left to the geometry block since it is not a
	 * become-maximized edge. */
	const bool maxed = effectivelyMaximized();
	if (maxed && !_wasMaximized)
		_appliedInsets = bandInsets();
	_wasMaximized = maxed;

	/* _GTK_FRAME_EXTENTS: tell the WM the band ring is frame, not content (snap/tile geometry). */
	const SDL_Rect ext = bandInsets();
	if (!SDL_RectsEqual(&ext, &_extentsApplied) &&
	    sdl_x11_set_frame_extents(_win->window(), ext.x, ext.w, ext.y, ext.h))
		_extentsApplied = ext;

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
	if (_minMaxDirty || _geometryDirty)
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
			const SDL_Rect bi = bandInsets();
			const int capW = usable.w + bi.x + bi.w;
			const int capH = usable.h + bi.y + bi.h;
			if ((maxW == 0) || (maxW > capW))
				maxW = capW;
			if ((maxH == 0) || (maxH > capH))
				maxH = capH;
		}
		SDL_SetWindowMaximumSize(_win->window(), maxW, maxH);
		_minMaxDirty = false;
	}

	/* Maximized (applied OR already declared by the server - the geometry order can precede the
	 * SHOW order): WM owns geometry; a server update stays pending until restored. Minimized is
	 * skipped too: the server sends a placeholder geometry on minimize; applying it would poison
	 * the WM's restore bounds (xf guards the same on WINDOW_SHOW_MINIMIZED). */
	if (_geometryDirty && !geometryFrozen() && !_fullscreen)
	{
		const SDL_Rect vis = targetOuterRect();
		/* The window is (or becomes) vis = _windowRect + fresh insets: record those as the insets
		 * now baked in, so the round-trip back to server coords strips exactly this. */
		_appliedInsets = bandInsets();
		int cw = 0;
		int ch = 0;
		SDL_GetWindowSize(_win->window(), &cw, &ch);
		bool applied = false;
		/* Only request WM geometry that differs from the live geometry (e.g. adopting a WM tile
		 * the server echoed leaves the window already at the target): redundant ConfigureRequests
		 * are churn the WM may treat as client intent, and skipping them keeps _geomApplyPending
		 * honest. Popup coords are parent-relative; Wayland positions are unreadable so only the
		 * size is compared. */
		if (_isPopup && parent)
		{
			SDL_SetWindowPosition(_win->window(), vis.x - parentRect.x, vis.y - parentRect.y);
			applied = true;
		}
		else if (railPlatformCaps().positionsReadable)
		{
			int cx = 0;
			int cy = 0;
			SDL_GetWindowPosition(_win->window(), &cx, &cy);
			if ((cx != vis.x) || (cy != vis.y))
			{
				SDL_SetWindowPosition(_win->window(), vis.x, vis.y);
				applied = true;
			}
		}
		if ((cw != vis.w) || (ch != vis.h))
		{
			SDL_SetWindowSize(_win->window(), vis.w, vis.h);
			applied = true;
			int aw = 0;
			int ah = 0;
			SDL_GetWindowSize(_win->window(), &aw, &ah);
			/* The WM ignored the resize and left the window at its own size (it restored an
			 * un-maximized window to its remembered floating size and won't shrink to the smaller
			 * server rect): the apply will never converge, so arming the bracket would deadlock the
			 * reporter and strand the window a band+ wider than the server rect. Adopt the WM
			 * geometry as authoritative - paint reports it and re-syncs the server. Only when the
			 * WM left the size untouched (a lagging apply still moves toward the target); X11 only;
			 * and only once the window is fully settled - never mid-drag or while a completion is
			 * unwinding, or the WM's live drag jitter would ping-pong the server every frame. */
			if (railPlatformCaps().positionsReadable && !_localMoveActive && !_loopEnd.pending &&
			    (aw == cw) && (ah == ch) && ((aw != vis.w) || (ah != vis.h)))
			{
				/* Keep the server-target origin (vis), adopt only the WM's size: the mismatch is
				 * the size, and the live window position is unreliable to read back here. */
				_wmRefusedOuter = { vis.x, vis.y, aw, ah };
				_wmRefused = true;
				applied = false; /* no convergence echo is coming; do not arm the bracket */
			}
		}
		_geometryDirty = false;
		/* The WM echoes a real apply back as MOVED/PIXEL events (possibly mangled during a
		 * maximize gap); the reporter drops those until convergence. applied == false means the
		 * live geometry already matches the server target - that IS convergence, so an earlier
		 * stuck bracket is released here too. */
		_geomApplyPending = applied;
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
		if (!_isPopup && !_icon.bgra.empty())
		{
			SDL_Surface* s = SDL_CreateSurfaceFrom(
			    static_cast<int>(_icon.w), static_cast<int>(_icon.h), SDL_PIXELFORMAT_BGRA32,
			    _icon.bgra.data(), static_cast<int>(_icon.w * 4));
			if (s)
			{
				if (!SDL_SetWindowIcon(_win->window(), s))
					SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
					            "SDL_SetWindowIcon failed for window 0x%08x: %s",
					            static_cast<unsigned>(_id), SDL_GetError());
				SDL_DestroySurface(s);
			}
		}
		_iconDirty = false;
	}

	/* Defer show until first frame; this runs every reconcile, so skip the call once visible. */
	if (!_minState.rail && _gfxPresented &&
	    (SDL_GetWindowFlags(_win->window()) & SDL_WINDOW_HIDDEN))
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
			/* Alpha may first appear in a later incremental frame (e.g. a menu fading in). */
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
	WLog_VRB(TAG, "gfx id=0x%08x %ux%u full=%d nDamage=%u", static_cast<unsigned>(_id), width,
	         height, full ? 1 : 0, nbDamage);
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
		/* No apply bracket here: s.rail is set before enter(), so the maximized/minimized guard
		 * already blocks every echo of this state change - a bracket would be a dead store. */
		s.rail = true;
		WLog_DBG(TAG, "%s id=0x%08x", what, static_cast<unsigned>(_id));
		enter(_win->window());
	}
	else if (!s.server && s.rail)
	{
		s.rail = false;
		/* Restore emits a burst of intermediate configures; drop them until the window converges.
		 */
		_geomApplyPending = true;
		WLog_DBG(TAG, "restore id=0x%08x (%s)", static_cast<unsigned>(_id), what);
		SDL_RestoreWindow(_win->window());
	}
	s.dirty = false;
}

bool SdlRailWindow::effectivelyMaximized() const
{
	return _maxState.rail ||
	       (_win && (SDL_GetWindowFlags(_win->window()) & SDL_WINDOW_MAXIMIZED) != 0);
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
		WLog_DBG(TAG, "map id=0x%08x %s", static_cast<unsigned>(_id), railRole(_isPopup, _layered));
		SDL_ShowWindow(_win->window());
		if ((!_isPopup && !_layered) || _fullscreen)
			_win->raise(); /* app + fullscreen windows come to the front (fullscreen needs the
			                * keyboard focus a menu popup would not); menu popups are above by
			                * design */
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
	const int gw =
	    static_cast<int>(_gfxW); /* GFX surface size as int (used across the blit paths) */
	const int gh = static_cast<int>(_gfxH);

	/* A size mismatch during a plain move = the WM snapped/tiled mid-drag; treat as a resize. */
	bool localResize = _localMoveActive && _localMoveIsResize;
	if (_localMoveActive && !localResize)
		localResize = (cw != gw) || (ch != gh);

	/* Window size changed outside a drag: repaint once even without damage. */
	const bool serverResize = !_localMoveActive && ((ww != _lastWinW) || (wh != _lastWinH));

	/* Skip undamaged frames. */
	if (!localResize && !serverResize && _gfxDamage.empty() &&
	    !((_layeredApp || _layered) && _visDirty))
	{
		WLog_VRB(TAG, "paintGfx skip id=0x%08x no-damage no-resize", static_cast<unsigned>(_id));
		return true;
	}

	/* Don't map a popup until it has real (non-black) content: avoids a black flash. */
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
		const bool blend =
		    honorsAlpha() && _gfxHasAlpha && railPlatformCaps().supportsTransparentWindows;
		if (!blend)
			contentFormat = SDL_PIXELFORMAT_BGRX32;
	}

	SDL_Surface* s = SDL_CreateSurfaceFrom(gw, gh, contentFormat, _gfxBuffer.data(),
	                                       static_cast<int>(_gfxStride));
	if (!s)
	{
		WLog_WARN(TAG, "paintGfx id=0x%08x SDL_CreateSurfaceFrom failed: %s",
		          static_cast<unsigned>(_id), SDL_GetError());
		return false;
	}

	/* Content blits at the inset offset; the ring outside it is the transparent resize band. */
	if (localResize)
	{
		/* Anchor the stale frame to the fixed corner. */
		const SDL_Point off = { _resizeAnchorRight ? (ww - bi.w - gw) : bi.x,
			                    _resizeAnchorBottom ? (wh - bi.h - gh) : bi.y };
		/* The "awaiting content" dashes only during a real edge/band resize; a MOVE whose size
		 * the WM changed (snap, untile restore) just shows the clipped stale frame - dashes there
		 * would read as a resize the user never started. */
		std::ignore = _win->paintResizeFrame(s, off, !_gfxDamage.empty(), bi, _localMoveIsResize);
	}
	else
	{
		/* Render accumulated damage or re-blit full surface on bare resize. */
		const SDL_Rect full = { 0, 0, gw, gh };
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
			/* A maximized custom-frame surface still contains the invisible border (gfx > rect;
			 * Windows parks the frame off-screen at rect - margin). The WM may clamp that overhang
			 * away (and Wayland can't overhang at all), so crop the border out of the blit instead
			 * of relying on the window position. */
			if (gw > _windowRect.w)
				dst.x -= _frameMargins.x;
			if (gh > _windowRect.h)
				dst.y -= _frameMargins.y;
		}
		if ((_layeredApp || _layered) && !_visRects.empty() && !maxed)
		{
			/* The layered surface is only defined inside the visibility rects (xf shapes the X
			 * window to them); outside is garbage that would paint a black ring, so clip the blit
			 * and leave the ring transparent. A maximized window has no shadow ring and its vis
			 * rect is inset by the dropped resize margin - clipping would cut the top-left edge,
			 * so draw the full surface instead. */
			const bool logClip = _visDirty;
			if (_visDirty)
			{
				/* Rects changed: wipe so newly-excluded regions don't keep stale pixels. */
				std::ignore = _win->fill(static_cast<Uint8>(0), 0, 0, 0);
				_gfxDamage.assign(1, full);
				_visDirty = false;
			}
			const SDL_Point off = { _visOffsetSet ? (_visOffset.x - _windowRect.x) : 0,
				                    _visOffsetSet ? (_visOffset.y - _windowRect.y) : 0 };
			if (logClip)
				WLog_VRB(TAG,
				         "clip id=0x%08x off=%d,%d nVis=%zu vis0=%dx%d+%d+%d win=%dx%d "
				         "gfx=%ux%u",
				         static_cast<unsigned>(_id), off.x, off.y, _visRects.size(), _visRects[0].w,
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
	WLog_VRB(TAG, "paintGfx id=0x%08x mode=%s win=%dx%d dmg=%zu", static_cast<unsigned>(_id),
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
			WLog_VRB(TAG, "paintLegacy skip id=0x%08x no-damage", static_cast<unsigned>(_id));
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
		WLog_VRB(TAG, "paintLegacy id=0x%08x full=%d visRects=%zu", static_cast<unsigned>(_id),
		         full ? 1 : 0, vis.size());
		/* Defer mapping until GFX frame arrives to avoid desktop flash. */
	}
	return true;
}
