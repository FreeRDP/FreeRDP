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

#include <atomic>
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
	/* Server-reported window rect. */
	[[nodiscard]] SDL_Rect windowRect() const;

	/* Local move/resize state. */
	void setLocalMoveActive(bool active);
	[[nodiscard]] bool localMoveActive() const;
	/* True if the server resized the window during the current/last local move (drag-restore). */
	[[nodiscard]] bool localMoveSizeChanged() const;
	/* Where the server re-anchored its window mid-move (valid when localMoveSizeChanged()). */
	[[nodiscard]] SDL_Point localMoveServerPos() const;
	void adoptLocalGeometry(const SDL_Rect& rect);
	/* Resize anchor corner during local resize. */
	void setResizeAnchor(bool right, bool bottom);

	/* Deferred destroy: mark here (RDP thread), erased on the main thread in SdlRail::paint. */
	void markDeleted();
	[[nodiscard]] bool isDeleted() const;

	/* In-flight local state application flag. */
	[[nodiscard]] bool geomApplyPending() const;
	void clearGeomApplyPending();

	/* Deferred completion of server modal move/size loop. */
	void armLoopEnd(); /* completion: now awaiting the END order */
	[[nodiscard]] bool loopEndPending() const;
	void deferMaximize();                       /* WM snap-to-top mid-drag: SC_MAXIMIZE on close */
	void deferSnap(const SDL_Rect& serverRect); /* WM snap/tile: resend rect on close */
	void clearLoopEnd(); /* a fresh drag of this window supersedes the pending close */
	struct LoopEndActions
	{
		bool maximize = false;
		bool snap = false;
		SDL_Rect snapRect = { 0, 0, 0, 0 };
	};
	[[nodiscard]] LoopEndActions takeLoopEnd(); /* END order: drain queued actions and reset */

	/* Adopt WM-refused geometry. */
	[[nodiscard]] bool takeWmOverride(SDL_Rect& outer);

	/* Visible sub-rects (window-relative); only these are painted so windows on top don't bleed. */
	void setVisibilityRects(std::vector<SDL_Rect> rects);
	/* Server visible-region origin offset. */
	void setVisibleOffset(SDL_Point offset);

	/* Server min/max tracking size (RAIL MinMaxInfo); applied in reconcile. */
	void setMinMaxSize(SDL_Point minSize, SDL_Point maxSize);

	/* Server outside resize margins. */
	void setResizeMargins(int left, int top, int right, int bottom);
	[[nodiscard]] SDL_Rect resizeMargins() const; /* x=left y=top w=right h=bottom */
	/* Raw server frame margins. */
	void setFrameMargins(const SDL_Rect& m);
	[[nodiscard]] SDL_Rect frameMargins() const;

	/* Owning window id (WINDOW_ORDER_FIELD_OWNER); popups position relative to it. */
	void setOwner(uint64_t ownerId);
	[[nodiscard]] uint64_t owner() const;
	/* Popup = caption-less transient (menu/dropdown/tooltip); created as an SDL popup. */
	[[nodiscard]] bool isPopup() const;
	/* Full-screen popup treated as a top-level window. */
	[[nodiscard]] bool isFullscreen() const;
	/* Layered decoration (drop shadow); realized only while anchored to a visible popup. */
	[[nodiscard]] bool isLayered() const;
	/* Mark shadow anchored to an active popup. */
	void setShadowAnchored(bool anchored);
	/* Local window insets for resize bands. */
	[[nodiscard]] SDL_Rect insets() const;
	/* Server rect inflated by insets() = the local SDL window's on-screen geometry. */
	[[nodiscard]] SDL_Rect outerRect() const;
	/* Server coordinate for window-local (0,0), aligned with blit anchor. */
	[[nodiscard]] SDL_Point serverOrigin() const;
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
	                      const RECTANGLE_16* damage, uint32_t nbDamage, uint32_t format);
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
	/* Effective maximized state (local or server-declared). */
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
		/* Read lock-free by railMaximized()/geometryFrozen() on either thread, so they cannot be
		 * plain bools. `dirty` stays one: every read and write of it is under _gfxLock. */
		std::atomic<bool> rail{ false };
		std::atomic<bool> server{ false };
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
	/* Band eligibility margins. */
	[[nodiscard]] SDL_Rect bandMargins() const;
	[[nodiscard]] SDL_Rect bandInsets() const;      /* insets(); caller holds _gfxLock */
	[[nodiscard]] SDL_Point blitOffset() const;     /* caller holds _gfxLock */
	[[nodiscard]] SDL_Rect targetOuterRect() const; /* outerRect(); caller holds _gfxLock */
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
	/* Insets applied to active SDL window; stripped to recover server coordinates. */
	SDL_Rect _appliedInsets = { 0, 0, 0, 0 };
	bool _wasMaximized = false; /* refresh insets on the maximize rising edge */
	SDL_Point _minSize = { 0, 0 };
	SDL_Point _maxSize = { 0, 0 };
	bool _minMaxDirty = false;
	bool _isPopup = false;
	bool _fullscreen = false;      /* full-display popup as fullscreen toplevel (Wayland) */
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
	bool _topmost = false;
	bool _topmostDirty = false;
	bool _styleDirty = false;   /* resizability needs re-applying (style change) */
	bool _painted = false;      /* full copy done; afterwards only damage regions are re-copied */
	bool _gfxPresented = false; /* presented own GFX content once: gate the first map on it */
	bool _mapped = false;       /* shown at least once (one-shot show+raise) */
	bool _localMoveActive = false; /* WM move/resize in progress: ignore server geometry + input */
	bool _localMoveIsResize = false;          /* local move is a resize vs a move */
	bool _localMoveSizeChanged = false;       /* server resized mid-move (drag-restore) */
	SDL_Point _localMoveServerPos = { 0, 0 }; /* the re-anchored server origin (see above) */
	/* Deferred server-modal-loop close for THIS window (see armLoopEnd()). */
	struct
	{
		bool pending = false;
		bool maximize = false;
		bool snap = false;
		SDL_Rect snapRect = { 0, 0, 0, 0 };
	} _loopEnd;
	bool _geomApplyPending = false; /* a client-issued geometry/state apply is still settling */
	bool _wmRefused = false;        /* WM refused resize; adopt its geometry (takeWmOverride) */
	SDL_Rect _wmRefusedOuter = { 0, 0, 0, 0 };
	bool _resizeAnchorRight = false;  /* anchor stale frame to the right edge (left-side resize) */
	bool _resizeAnchorBottom = false; /* anchor stale frame to the bottom edge (top-side resize) */
	/* Timeout deadline for anchored frames during resize completion. */
	uint64_t _awaitingFrameUntil = 0;
	SDL_Rect _extentsApplied = { 0, 0, 0, 0 }; /* band insets last mirrored to the WM */

	/* Guard for state shared between RDP and main threads. */
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
	/* Regions changed since last paint. Guarded by _gfxLock. */
	std::vector<SDL_Rect> _gfxDamage;
	/* Pixel dimensions at last presentation. */
	int _lastWinW = 0;
	int _lastWinH = 0;
	int _lastGfxW = 0;
	int _lastGfxH = 0;
	bool _needsFullBlit = true;
};
