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
	[[nodiscard]] SDL_Rect windowRect() const;

	/* The window's server rect: what the GFX surface spans and the local window shows. The
	 * invisible resize margins live outside it. */
	/* Local (WM) move/resize in progress: server geometry and input are ignored while set.
	 * adoptLocalGeometry() takes the WM's final geometry and clears the flag. */
	void setLocalMoveActive(bool active);
	[[nodiscard]] bool localMoveActive() const;
	void adoptLocalGeometry(const SDL_Rect& rect);
	/* Which corner the stale frame anchors to during a local resize, so old content stays
	 * pinned to the fixed (opposite) edge instead of sliding with the moving one. */
	void setResizeAnchor(bool right, bool bottom);

	/* Deferred destroy: mark here (RDP thread), erased on the main thread in SdlRail::paint. */
	void markDeleted();
	[[nodiscard]] bool isDeleted() const;

	/* Visible sub-rects (window-relative); only these are painted so windows on top don't bleed. */
	void setVisibilityRects(std::vector<SDL_Rect> rects);

	/* Server min/max tracking size (RAIL MinMaxInfo); applied in reconcile. */
	void setMinMaxSize(SDL_Point minSize, SDL_Point maxSize);

	/* Server's outside resize band: ClientWindowMove reports the outer rect to prevent shrinking. */
	void setResizeMargins(int left, int top, int right, int bottom);
	[[nodiscard]] SDL_Rect resizeMargins() const; /* x=left y=top w=right h=bottom */

	/* Owning window id (WINDOW_ORDER_FIELD_OWNER); popups position relative to it. */
	void setOwner(uint64_t ownerId);
	[[nodiscard]] uint64_t owner() const;
	/* Popup = caption-less transient (menu/dropdown/tooltip); created as an SDL popup. */
	[[nodiscard]] bool isPopup() const;

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
	/* Local or live-WM maximized (railMaximized may lag the SDL flag during a snap). */
	[[nodiscard]] bool effectivelyMaximized() const;

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
	bool create(SDL_Window* parent, const SDL_Rect& parentRect);
	bool paintGfx(SDL_PixelFormat format);
	bool paintLegacy(SDL_Surface* primary, const std::vector<SDL_Rect>& damage);

	uint64_t _id;
	std::unique_ptr<SdlWindow> _win; /* local window+renderer (lazy, main thread) */
	SDL_Rect _windowRect;            /* server offset/size */
	uint32_t _style = 0;
	uint64_t _ownerId = 0;
	SDL_Rect _resizeMargins = { 0, 0, 0, 0 }; /* x=left y=top w=right h=bottom */
	SDL_Point _minSize = { 0, 0 };
	SDL_Point _maxSize = { 0, 0 };
	bool _minMaxDirty = false;
	bool _isPopup = false;
	bool _layered = false;         /* WS_EX_LAYERED shadow/glass decoration: never rendered */
	bool _popupClassified = false; /* isPopup/_layered frozen after the first (creation) style */
	bool _parentApplied = false;   /* transient-for owner set once (owned non-popup dialogs) */
	StateSync _maxState;           /* maximize sync */
	StateSync _minState;           /* minimize sync */
	std::string _title = "RdpRailWindow";
	SdlRailIcon _icon;
	bool _iconDirty = false;
	bool _visible = false;
	bool _geometryDirty = true;
	bool _titleDirty = false;
	bool _painted = false;    /* full copy done; afterwards only damage regions are re-copied */
	bool _localMoveActive = false; /* WM move/resize in progress: ignore server geometry + input */
	bool _localMoveIsResize = false;          /* local move is a resize vs a move */
	bool _resizeAnchorRight = false;  /* anchor stale frame to the right edge (left-side resize) */
	bool _resizeAnchorBottom = false; /* anchor stale frame to the bottom edge (top-side resize) */

	/* Guards all state shared between the RDP thread (setters) and the main thread
	 * (reconcile/paint): geometry, title, visibility, flags, vis rects and the gfx snapshot. */
	mutable std::mutex _gfxLock;
	std::vector<SDL_Rect> _visRects;
	bool _deleted = false;
	std::vector<uint8_t> _gfxBuffer; /* owned deep-copy of the gdi surface (avoids UAF) */
	uint32_t _gfxStride = 0;
	uint32_t _gfxW = 0;
	uint32_t _gfxH = 0;
	bool _hasGfx = false;
	/* Regions changed since the last paint (server damage, surface coords); only these are copied
	 * and uploaded. Empty = nothing to repaint (skip the window). Guarded by _gfxLock. */
	std::vector<SDL_Rect> _gfxDamage;
};
