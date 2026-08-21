/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client RAIL (RemoteApp Integrated Locally)
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
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <freerdp/client/rail.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/update.h>

#include "sdl_rail_window.hpp"

class SdlContext;

/**
 * RAIL channel handler for the SDL3 client.
 *
 * Server-authoritative: the server drives window geometry via WINDOW_ORDER and the local
 * borderless windows follow (the RemoteApp frame is server-drawn content). Interactive
 * move/resize is handed to the local WM only on a server request (ServerLocalMoveSize).
 */
class SdlRail
{
  public:
	explicit SdlRail(SdlContext* context);
	~SdlRail();

	SdlRail(const SdlRail&) = delete;
	SdlRail(SdlRail&&) = delete;
	SdlRail& operator=(const SdlRail&) = delete;
	SdlRail& operator=(SdlRail&&) = delete;

	bool init(RailClientContext* rail);
	bool uninit(RailClientContext* rail);

	[[nodiscard]] bool enabled() const
	{
		return _enabled;
	}

	/* Paint all RAIL windows (window-mapped GFX surface, or legacy desktop region).
	 * `damage` = server-updated desktop rects for this cycle (empty = nothing new). */
	bool paint(SDL_Surface* primary, SDL_PixelFormat fallbackFormat,
	           const std::vector<SDL_Rect>& damage);

	/* Called from the GFX pipeline when a window-mapped surface updated (RDP thread). */
	UINT updateWindowFromSurface(gdiGfxSurface* surface);

	/* GFX callback: install as gfx->UpdateWindowFromSurface after gdi_graphics_pipeline_init. */
	static UINT UpdateWindowFromSurface(RdpgfxClientContext* context, gdiGfxSurface* surface);

	/* Input routing (main thread): true if the SDL window id is one of our RAIL windows. */
	[[nodiscard]] bool ownsWindow(SDL_WindowID id);
	/* Mark a RAIL window fully dirty and request a repaint (e.g. on expose). */
	void invalidateWindow(SDL_WindowID id);
	/* Local maximize/minimize/restore (WM snap, taskbar): tell the server via ClientSystemCommand
	 * so the window takes the real state. One RESTORED event ends either state. */
	void handleMaximized(SDL_WindowID id);
	void handleMinimized(SDL_WindowID id);
	void handleRestored(SDL_WindowID id);
	void handleClose(SDL_WindowID id);
	/* Local focus change: send ClientActivate, like xf FocusIn/FocusOut. */
	void handleFocus(SDL_WindowID id, bool gained);
	/* activate the window on the server if it isn't already the active one (dedup via
	 * _clientActiveId). Called on click so input routes to the clicked window, not a stale one. */
	void ensureActive(SDL_WindowID id);
	/* Rewrite window-local (x,y) to server-absolute; false if id is not a RAIL window. */
	bool translateToServer(SDL_WindowID id, float& x, float& y);
	/* Record the last forwarded left button-down (server coords): the anchor of any server modal
	 * move/size loop that press starts. Main thread. */
	void noteLeftPress(float x, float y)
	{
		_lastPressServer = { static_cast<int>(x), static_cast<int>(y) };
		_lastPointerServer = _lastPressServer;
	}
	/* Record every forwarded motion: once a WM drag suppresses input, the server's cursor stays
	 * frozen at the LAST forwarded position (a few motions leak in before the LocalMoveSize
	 * round-trip), which is what a mid-drag re-anchor is measured against. Main thread. */
	void noteServerPointer(float x, float y)
	{
		_lastPointerServer = { static_cast<int>(x), static_cast<int>(y) };
	}
	/* True while a WM move/resize is in progress for this window: the caller must NOT forward the
	 * event to the server, or the server runs its own move/size loop and grows the window. */
	bool suppressServerInput(SDL_WindowID id);
	/* Motion-only variant covering BOTH backends (suppressServerInput is X11-only): keeps a
	 * grab-less Wayland drag from feeding the server's own modal resize. Buttons still pass. */
	bool suppressServerMotion(SDL_WindowID id);

	/* Main thread: start the WM-native interactive move/resize the server requested
	 * (payload-carried so back-to-back requests can't clobber each other). */
	void handleLocalMoveRequested(uint32_t windowId, uint16_t moveType);
	/* Main thread: the WM resized the window mid-drag during a plain X11 move (snap/tile or
	 * un-tile) - record the WM's size; the completion adopts it (the live size at release is
	 * unreliable: mutter re-asserts the tile geometry when its grab ends). */
	void noteDragResize(SDL_WindowID id, int w, int h);
	/* Main thread: a WM event changed this window's geometry outside a drag (WM move settle,
	 * keyboard tile). Reports are synchronous but filter out echoes of recent server orders
	 * to prevent feedback loops and restore-rect poisoning. */
	void syncGeometry(SDL_WindowID id);
	/* X11 only: finalize the pending WM move/resize, reporting final geometry via ClientWindowMove.
	 * Wayland uses completeWaylandResize instead (compositor grab hides the real button-up). */
	void completeLocalMoveIfPending();

	/* Wayland (main thread): finalize the resize when the compositor grab ends (pointer re-enter)
	 * or as a backstop on the next button-down. No-op unless a real Wayland resize is pending.
	 * `definitive` = the grab is provably over (fresh button-down); cancels a no-op click grab. */
	void completeWaylandResize(bool definitive = false);
	/* Wayland (main thread): the compositor took the pointer for the pending resize (a MOUSE_LEAVE
	 * during the latch = grab granted). Confirms the grab ~2ms in, far ahead of the first configure
	 * (~120ms), so the server's END order cannot clear the latch out from under a granted grab. */
	void noteResizeGrab(SDL_WindowID id);
	/* Wayland (main thread): a compositor-driven resize (snap/tile) with no server move request.
	 * Report the new size (keep server's last origin, clamped). No-op if move/maximized/unchanged.
	 */
	void handleWaylandResize(SDL_WindowID id);

  private:
	/* _windows helpers: callers must hold _windowsLock. */
	[[nodiscard]] SdlRailWindow* getWindow(uint64_t id);
	[[nodiscard]] SdlRailWindow* getWindowBySdlId(SDL_WindowID id);
	/* Send RAIL_ACTIVATE_ORDER and track _clientActiveId; caller holds _windowsLock. */
	void sendClientActivate(uint32_t wid, bool enabled);
	SdlRailWindow* addWindow(uint64_t id, const SDL_Rect& rect);
	/* The window `ownerId` names, if it is a live non-popup app window (a valid popup/dialog
	 * parent); else nullptr. Caller holds _windowsLock. */
	[[nodiscard]] SdlRailWindow* resolveParent(uint64_t ownerId);
	[[nodiscard]] SdlRailWindow* resolvePopupParent(const SdlRailWindow& popup);

	void enableRemoteAppMode(bool enable);
	/* Report a work area to the server (SPI_SET_WORK_AREA, server coords). Main thread. No-op if it
	 * matches the last one sent. */
	void sendWorkArea(const SDL_Rect& area);
	/* Clamp a window origin (x,y) so a w x h window stays inside the server desktop. */
	void clampIntoDesktop(int& x, int& y, int w, int h) const;
	/* Send a RAIL_SYSCOMMAND_ORDER for the window. Caller holds _windowsLock. */
	void sendSystemCommand(SdlRailWindow* appWindow, uint16_t command);
	/* Realize the server's top-level z-order (X11 only, focus-neutral). Caller holds _windowsLock.
	 */
	void applyZOrder();
	/* Session-wide resize margins (Windows' invisible border is per-DPI, not per-window): seeds
	 * windows announced without margin fields (initial sync of pre-existing windows only sends
	 * them on activation). Guarded by _windowsLock. */
	SDL_Rect _sessionMargins = { 0, 0, 0, 0 };
	SDL_Point _lastPressServer = { 0, 0 };   /* last forwarded left press; main thread */
	SDL_Point _lastPointerServer = { 0, 0 }; /* last forwarded pointer position; main thread */

	/* --- RAIL server callbacks (static, dispatched to the instance) --- */
	static UINT server_execute_result(RailClientContext* context,
	                                  const RAIL_EXEC_RESULT_ORDER* execResult);
	static UINT server_local_move_size(RailClientContext* context,
	                                   const RAIL_LOCALMOVESIZE_ORDER* localMoveSize);
	static UINT server_min_max_info(RailClientContext* context,
	                                const RAIL_MINMAXINFO_ORDER* minMaxInfo);

	/* --- window order callbacks (registered on rdpUpdate::window) --- */
	void registerUpdateCallbacks(rdpUpdate* update);
	static BOOL window_common(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                          const WINDOW_STATE_ORDER* windowState);
	/* window_common field handlers (split out for readability). */
	void updateMargins(SdlRailWindow* appWindow, const WINDOW_ORDER_INFO* orderInfo,
	                   const WINDOW_STATE_ORDER* state);
	static void updateShowState(SdlRailWindow* appWindow, const WINDOW_ORDER_INFO* orderInfo,
	                            const WINDOW_STATE_ORDER* state);
	static void updateVisRects(SdlRailWindow* appWindow, const WINDOW_ORDER_INFO* orderInfo,
	                           const WINDOW_STATE_ORDER* state);
	static BOOL window_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                        const WINDOW_ICON_ORDER* windowIcon);
	static BOOL window_cached_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                               const WINDOW_CACHED_ICON_ORDER* windowCachedIcon);
	static BOOL window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo);
	static BOOL monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                              const MONITORED_DESKTOP_ORDER* monitoredDesktop);
	static BOOL non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo);

	static SdlRail* get(rdpContext* context);

	/* Shared finalize tail (caller holds _windowsLock): report the final rect to the server and
	 * adopt it locally. Both the X11 and Wayland completion paths funnel here. */
	void reportAndAdopt(SdlRailWindow* appWindow, int x, int y, int w, int h);
	/* Report a server-coords rect via ClientWindowMove: the order carries the OUTER frame (rect
	 * inflated by the raw frame margins; the server deflates by exactly those). Caller holds
	 * _windowsLock. */
	void sendClientWindowMove(SdlRailWindow* appWindow, const SDL_Rect& serverRect);

	/* MS-RDPERP icon cache slot for cacheId:cacheEntry; nullptr if out of range. cacheId 0xFF =
	 * "do not cache" scratch slot (the spec says 0xFFFF but the field is one byte). */
	[[nodiscard]] SdlRailIcon* iconCacheLookup(uint32_t cacheId, uint32_t cacheEntry);

  private:
	SdlContext* _context;
	RailClientContext* _rail = nullptr;
	/* Written on the RDP thread (RemoteApp mode toggles), read lock-free on the main thread. */
	std::atomic<bool> _enabled = false;
	bool _refreshSent = false; /* one full RefreshRect per connection, at first window realize */
	/* Last work area reported to the server (server coords). On Wayland it's unknown up front, so a
	 * maximize corrects it from the actual maximized window size. */
	SDL_Rect _sentWorkArea = { 0, 0, 0, 0 };
	/* Guards _windows (RDP thread mutates, main thread paints/iterates, GFX thread looks up).
	 * Erased on the main thread only, so SDL windows are destroyed there. */
	mutable std::mutex _windowsLock;
	std::map<uint64_t, SdlRailWindow> _windows;
	/* Windows deleted on the RDP thread but recreated under the same id before the next paint could
	 * erase them: the stale entry is moved here (node transfer, no move-construction -
	 * SdlRailWindow is non-movable) so the fresh window takes the id, and its SDL window/band die
	 * on the main thread when paint drains this. Multimap: an id can collide more than once before
	 * a drain. */
	std::multimap<uint64_t, SdlRailWindow> _deadWindows;
	/* State of the one interactive WM move/resize active at a time (a user drags one window);
	 * report the final rect when it ends. Reset as a unit with `= {}` so teardown can't miss a
	 * field. The per-window loop-end (SdlRailWindow::armLoopEnd) is separate: it deliberately
	 * outlives the drag until the server's move/size END. */
	struct LocalMove
	{
		uint32_t id = 0;      /* windowId of the active move, 0 = none (RAIL ids are non-zero) */
		bool wayland = false; /* Wayland tracks size only */
		uint16_t type = 0;    /* RAIL_WMSZ_* of the active local move */
		SDL_Point anchor = { 0, 0 };  /* modal-loop anchor (forwarded press) of this drag */
		SDL_Point pointer = { 0, 0 }; /* server's frozen cursor (last forward) of this drag */
		/* Wayland: a WINDOW_RESIZED arrived since the grab started, so the op really resized
		 * (guards against a bare pointer re-enter finalizing a no-op click). Main thread. */
		bool sawResize = false;
		/* Server END order arrived while the drag was still pending (app closed its loop early):
		 * the completion must not send the synthetic button-up. Guarded by _windowsLock. */
		bool serverEnded = false;
		/* Last WM-imposed size while a plain X11 move was active (noteDragResize; self-echoes of
		 * our own reconcile filtered out). Consumed by the completion. */
		bool wmSized = false;
		SDL_Point wmSize = { 0, 0 };
	};
	LocalMove _localMove;
	/* Last focused app window; parent for a right-click popup when ownerWindowId doesn't resolve to
	 * a non-popup window (avoids mis-parenting with several apps open). */
	uint64_t _focusedAppId = 0;
	/* Server-driven top-level z-order (MS-RDPERP), windowIds[0] = topmost. RDP thread writes under
	 * _windowsLock; main thread realizes it, records _appliedZOrder to skip identical resends. */
	std::vector<uint32_t> _zOrder;
	std::vector<uint32_t> _appliedZOrder;
	uint32_t _clientActiveId = 0; /* last window we told the server to activate (ClientActivate) */
	bool _zOrderDirty = false;
	/* Icon cache (RDP thread): index = cacheId * entries + cacheEntry, sized from settings. */
	std::vector<SdlRailIcon> _iconCache;
	SdlRailIcon _iconScratch;
	uint32_t _iconCacheEntries = 0;
};
