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
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <freerdp/types.h>

class SdlWindow;

/* A decoded RAIL window icon (BGRA32 pixels). */
struct SdlRailIcon
{
	uint32_t w = 0;
	uint32_t h = 0;
	std::vector<uint8_t> bgra;
};

/**
 * A single RemoteApp window, backed by a borderless SDL window.
 *
 * Geometry is server-authoritative: `_windowRect` mirrors the server's WINDOW_ORDER
 * offset/size and the local SDL window is moved/resized to follow it.
 */
class SdlRailWindow
{
  public:
	SdlRailWindow(uint64_t id, const SDL_Rect& rect);
	SdlRailWindow(SdlRailWindow&&) = delete;
	SdlRailWindow(const SdlRailWindow&) = delete;
	SdlRailWindow& operator=(const SdlRailWindow&) = delete;
	SdlRailWindow& operator=(SdlRailWindow&&) = delete;
	~SdlRailWindow();

	[[nodiscard]] uint64_t id() const;
	[[nodiscard]] SDL_WindowID sdlId() const;
	[[nodiscard]] SDL_Window* window() const;
	[[nodiscard]] SDL_Renderer* renderer() const;

	/* --- state setters, safe to call from the RDP thread (no SDL window calls) --- */

	/* Server-driven geometry (from WINDOW_ORDER). */
	void updateWindowRect(const SDL_Rect& rect);
	/* The window's server rect: what the GFX surface spans. The local SDL window is this rect
	 * inflated by insets() so the outside resize band is part of it (see outerRect()). */
	[[nodiscard]] SDL_Rect windowRect() const;

	/* Local (WM) move/resize in progress: server geometry and input are ignored while set.
	 * adoptLocalGeometry() takes the WM's final geometry and clears the flag. */
	void setLocalMoveActive(bool active);
	[[nodiscard]] bool localMoveActive() const;
	/* True if the server resized the window during the current/last local move (drag-restore). */
	[[nodiscard]] bool localMoveSizeChanged() const;
	/* Where the server re-anchored its window mid-move (valid when localMoveSizeChanged()). */
	[[nodiscard]] SDL_Point localMoveServerPos() const;
	void adoptLocalGeometry(const SDL_Rect& rect);
	/* Which corner the stale frame anchors to during a local resize, so old content stays
	 * pinned to the fixed (opposite) edge instead of sliding with the moving one. */
	void setResizeAnchor(bool right, bool bottom);

	/* Deferred destroy: mark here (RDP thread), erased on the main thread in SdlRail::paint. */
	void markDeleted();
	[[nodiscard]] bool isDeleted() const;

	/* Local state apply in flight: ignores window move/resize echoes (possibly mangled by WM)
	 * until convergence (clearGeomApplyPending) so they aren't wrongly reported as WM intent. */
	[[nodiscard]] bool geomApplyPending() const;
	void clearGeomApplyPending();

	/* Visible sub-rects (window-relative); only these are painted so windows on top don't bleed. */
	void setVisibilityRects(std::vector<SDL_Rect> rects);
	/* Server-absolute visible-region origin (WINDOW_ORDER_FIELD_VIS_OFFSET); the visibility rects
	 * are relative to it. */
	void setVisibleOffset(SDL_Point offset);

	/* Server min/max tracking size (RAIL MinMaxInfo); applied in reconcile. */
	void setMinMaxSize(SDL_Point minSize, SDL_Point maxSize);

	/* Server's outside resize band: the local window inflates over them (insets()) and
	 * ClientWindowMove reports the outer rect to prevent shrinking. */
	void setResizeMargins(int left, int top, int right, int bottom);
	[[nodiscard]] SDL_Rect resizeMargins() const; /* x=left y=top w=right h=bottom */
	/* RAW server margins: ClientWindowMove uses the outer rect including these, since the server
	 * expects the full frame and deflates it (reporting the visible rect shrinks the window). */
	void setFrameMargins(const SDL_Rect& m);
	[[nodiscard]] SDL_Rect frameMargins() const;

	/* Owning window id (WINDOW_ORDER_FIELD_OWNER); popups position relative to it. */
	void setOwner(uint64_t ownerId);
	[[nodiscard]] uint64_t owner() const;
	/* Popup = caption-less transient (menu/dropdown/tooltip); created as an SDL popup. */
	[[nodiscard]] bool isPopup() const;
	/* Layered decoration (drop shadow); realized only while anchored to a visible popup. */
	[[nodiscard]] bool isLayered() const;
	/* Set per paint pass: a visible popup adjoins this shadow (its anchor). Without one the shadow
	 * stays unrealized - app-window edge shadows would lag local moves and cover the resize
	 * margins (misrouted clicks); menu/tooltip shadows are the ones worth drawing. */
	void setShadowAnchored(bool anchored);
	/* SDL window insets: local window is inflated by these so the outside resize band takes input.
	 * Zero for popups / maximized / non-composited X11. */
	[[nodiscard]] SDL_Rect insets() const;
	/* Server rect inflated by insets() = the local SDL window's on-screen geometry. */
	[[nodiscard]] SDL_Rect outerRect() const;
	/* Inverse of outerRect(): strip insets() off a local outer rect to recover the server rect. */
	[[nodiscard]] SDL_Rect serverRect(const SDL_Rect& outer) const;

	void setStyle(uint32_t style, uint32_t exStyle);
	void setTitle(const std::string& title);
	void setTitle(const char16_t* str, size_t lenBytes);
	void setVisible(bool visible);
	/* Window icon (WindowIcon/WindowCachedIcon orders); applied in reconcile. */
	void setIcon(const SdlRailIcon& icon);

	/* --- main thread only (SDL window ops) --- */

	/* Window-mapped GFX surface (RDP thread): deep-copies the damaged gdi pixels for paint(). */
	void updateGfxSurface(const void* data, uint32_t stride, uint32_t width, uint32_t height,
	                      const RECTANGLE_16* damage, uint32_t nbDamage);
	/* Mark the whole window dirty for re-blit (e.g. on expose). */
	void invalidateAll();

	/* Maximize/minimize sync: local state (rail) vs server-reported state (RDP thread). */
	[[nodiscard]] bool railMaximized() const
	{
		return _maxState.rail;
	}
	void setRailMaximized(bool m)
	{
		std::unique_lock lock(_gfxLock);
		_maxState.rail = m;
	}
	void setServerMaximized(bool m);
	[[nodiscard]] bool railMinimized() const
	{
		return _minState.rail;
	}
	void setRailMinimized(bool m)
	{
		std::unique_lock lock(_gfxLock);
		_minState.rail = m;
	}
	void setServerMinimized(bool m);
	/* Max state applied locally OR declared by the server: geometry stays server/WM-owned either
	 * way (the geometry order can precede the SHOW order). Lock-free like railMaximized(). */
	[[nodiscard]] bool maxDeclared() const
	{
		return _maxState.rail || _maxState.server;
	}
	/* Max or min pins the geometry (server/WM owns it): the client geometry apply is skipped. */
	[[nodiscard]] bool geometryFrozen() const
	{
		return maxDeclared() || _minState.rail || _minState.server;
	}
	/* Local or live-WM maximized (railMaximized may lag the SDL flag during a snap). */
	[[nodiscard]] bool effectivelyMaximized() const;
	/* Transition in flight (local intent vs server state). WM resizes only trusted outside. */
	[[nodiscard]] bool stateTransitionPending() const
	{
		/* _maxState/_minState are written under _gfxLock on the RDP thread. */
		std::unique_lock lock(_gfxLock);
		return _maxState.dirty || _minState.dirty || (_maxState.rail != _maxState.server) ||
		       (_minState.rail != _minState.server);
	}

	/* Create/move/show the local SDL window to match pending state; popups use parent+rect. */
	bool reconcile(SDL_Window* parent, const SDL_Rect& parentRect);
	/* Render: GFX surface if mapped, else the shared desktop region. `damage` = updated rects. */
	bool paint(SDL_Surface* primary, SDL_PixelFormat fallbackFormat,
	           const std::vector<SDL_Rect>& damage, SDL_Window* parent, const SDL_Rect& parentRect);

  private:
	/* One maximize/minimize state pair: local (sent to server), server-reported, pending apply. */
	struct StateSync
	{
		bool rail = false;
		bool server = false;
		bool dirty = false;
	};
	void setServerState(StateSync& s, bool m);
	/* Caller holds _gfxLock and checked _win. */
	void applyServerState(StateSync& s, const char* what, bool (*enter)(SDL_Window*));
	[[nodiscard]] bool styleResizable() const; /* caller holds _gfxLock */
	/* Window classes whose GFX surface carries meaningful per-pixel alpha. */
	[[nodiscard]] bool honorsAlpha() const
	{
		return _isPopup || _layeredApp || _layered;
	}
	/* Band eligibility rule: margins for a resizable app window, else zero; caller holds _gfxLock.
	 */
	[[nodiscard]] SDL_Rect bandMargins() const;
	[[nodiscard]] SDL_Rect bandInsets() const;      /* insets(); caller holds _gfxLock */
	[[nodiscard]] SDL_Rect targetOuterRect() const; /* outerRect(); caller holds _gfxLock */
	/* Wayland: keep the outside resize band (sdl_wayland_band) at the current band margins. Called
	 * from reconcile WITHOUT _gfxLock held (it self-locks). No-op on X11. */
	void syncWaylandBand();
	bool create(SDL_Window* parent, const SDL_Rect& parentRect);
	bool paintGfx(SDL_PixelFormat format);
	bool paintLegacy(SDL_Surface* primary, const std::vector<SDL_Rect>& damage);

	uint64_t _id;
	std::unique_ptr<SdlWindow> _win; /* local window+renderer (lazy, main thread) */
	SDL_Rect _windowRect;            /* server offset/size */
	uint32_t _style = 0;
	bool _everResizable = false; /* latched: ever seen resizable (styleResizable) */
	uint64_t _ownerId = 0;
	SDL_Rect _resizeMargins = { 0, 0, 0, 0 }; /* x=left y=top w=right h=bottom */
	SDL_Rect _frameMargins = { 0, 0, 0, 0 };  /* raw server margins, for ClientWindowMove */
	/* Band insets actually baked into the current SDL window size (set when reconcile/create sizes
	 * it). serverRect/outerRect strip THIS, not a freshly recomputed bandInsets(): the two diverge
	 * for a beat when the server toggles the resizable style, and stripping the live value then
	 * leaks the band box into the reported server geometry (window grows one band per toggle). */
	SDL_Rect _appliedInsets = { 0, 0, 0, 0 };
	bool _wasMaximized = false; /* refresh insets on the maximize rising edge */
	SDL_Point _minSize = { 0, 0 };
	SDL_Point _maxSize = { 0, 0 };
	bool _minMaxDirty = false;
	bool _isPopup = false;
	bool _layered = false;         /* caption-less WS_EX_LAYERED decoration (drop shadow) */
	bool _layeredApp = false;      /* captioned WS_EX_LAYERED app window: honor per-pixel alpha */
	bool _popupClassified = false; /* isPopup/_layered frozen after the first (creation) style */
	bool _shadowAnchored = false;  /* a visible popup adjoins this shadow (see setShadowAnchored) */
	bool _parentApplied = false;   /* transient-for owner set once (owned non-popup dialogs) */
	StateSync _maxState;           /* maximize sync */
	StateSync _minState;           /* minimize sync */
	std::string _title = "RdpRailWindow";
	SdlRailIcon _icon;
	bool _iconDirty = false;
	bool _visible = false;
	bool _geometryDirty = true;
	bool _titleDirty = false;
	bool _styleDirty = false;   /* resizability needs re-applying (style change) */
	bool _painted = false;      /* full copy done; afterwards only damage regions are re-copied */
	bool _gfxPresented = false; /* presented own GFX content once: gate the first map on it */
	bool _mapped = false;       /* shown at least once (one-shot show+raise) */
	bool _localMoveActive = false; /* WM move/resize in progress: ignore server geometry + input */
	bool _localMoveIsResize = false;          /* local move is a resize vs a move */
	bool _localMoveSizeChanged = false;       /* server resized mid-move (drag-restore) */
	SDL_Point _localMoveServerPos = { 0, 0 }; /* the re-anchored server origin (see above) */
	bool _geomApplyPending = false;  /* a client-issued geometry/state apply is still settling */
	bool _resizeAnchorRight = false;  /* anchor stale frame to the right edge (left-side resize) */
	bool _resizeAnchorBottom = false; /* anchor stale frame to the bottom edge (top-side resize) */
	SDL_Rect _extentsApplied = { 0, 0, 0, 0 }; /* band insets last mirrored to the WM */

	/* Guards all state shared between the RDP thread (setters) and the main thread
	 * (reconcile/paint): geometry, title, visibility, flags, vis rects and the gfx snapshot. */
	mutable std::mutex _gfxLock;
	std::vector<SDL_Rect> _visRects;
	SDL_Point _visOffset = { 0, 0 }; /* server-absolute origin of _visRects */
	bool _visOffsetSet = false;      /* until VIS_OFFSET arrives, rects are window-relative */
	bool _visDirty = false;          /* rects/offset changed: wipe + full reclip (layered app) */
	bool _deleted = false;
	std::vector<uint8_t> _gfxBuffer; /* owned deep-copy of the gdi surface (avoids UAF) */
	uint32_t _gfxStride = 0;
	uint32_t _gfxW = 0;
	uint32_t _gfxH = 0;
	bool _hasGfx = false;
	/* Popup surface has real per-pixel alpha (blend); all-zero undefined alpha stays opaque. */
	bool _gfxHasAlpha = false;
	/* Regions changed since the last paint (server damage, surface coords); only these are copied
	 * and uploaded. Empty = nothing to repaint (skip the window). Guarded by _gfxLock. */
	std::vector<SDL_Rect> _gfxDamage;
	/* Window pixel size at the last GFX present. A server-driven resize changes it before the
	 * next frame arrives; paintGfx repaints once so the revealed area isn't a white flash. */
	int _lastWinW = 0;
	int _lastWinH = 0;
};
