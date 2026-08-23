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
	/* Local maximize/minimize/restore state handlers. */
	void handleMaximized(SDL_WindowID id);
	void handleMinimized(SDL_WindowID id);
	void handleRestored(SDL_WindowID id);
	void handleClose(SDL_WindowID id);
	/* Local focus change: send ClientActivate, like xf FocusIn/FocusOut. */
	void handleFocus(SDL_WindowID id, bool gained);
	/* Rewrite window-local (x,y) to server-absolute; false if id is not a RAIL window. */
	bool translateToServer(SDL_WindowID id, float& x, float& y);
	/* Suppress button-up to server during active local move/resize. */
	bool suppressServerInput(SDL_WindowID id);

	/* Start native interactive window move/resize. */
	void handleLocalMoveRequested(uint32_t windowId, SDL_Point pos, uint16_t moveType);
	/* Finalize pending local move/resize. */
	void completeLocalMoveIfPending();

	/* Finalize Wayland resize. */
	void completeWaylandResize();
	/* Handle Wayland resize event. */
	void handleWaylandResize(SDL_WindowID id);

  private:
	/* _windows helpers: callers must hold _windowsLock. */
	[[nodiscard]] SdlRailWindow* getWindow(uint64_t id);
	[[nodiscard]] SdlRailWindow* getWindowBySdlId(SDL_WindowID id);
	SdlRailWindow* addWindow(uint64_t id, const SDL_Rect& rect);
	/* The window `ownerId` names, if it is a live non-popup app window (a valid popup/dialog
	 * parent); else nullptr. Caller holds _windowsLock. */
	[[nodiscard]] SdlRailWindow* resolveParent(uint64_t ownerId);

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

	/* MS-RDPERP icon cache slot for cacheId:cacheEntry; nullptr if out of range. cacheId 0xFF =
	 * "do not cache" scratch slot (the spec says 0xFFFF but the field is one byte). */
	[[nodiscard]] SdlRailIcon* iconCacheLookup(uint32_t cacheId, uint32_t cacheEntry);

  private:
	SdlContext* _context;
	RailClientContext* _rail = nullptr;
	bool _enabled = false;
	bool _refreshSent = false; /* one full RefreshRect per connection, at first window realize */
	/* Last work area reported to the server (server coordinates). */
	SDL_Rect _sentWorkArea = { 0, 0, 0, 0 };
	/* Guards _windows (RDP thread mutates, main thread paints/iterates, GFX thread looks up).
	 * Erased on the main thread only, so SDL windows are destroyed there. */
	mutable std::mutex _windowsLock;
	std::map<uint64_t, SdlRailWindow> _windows;
	/* Windows awaiting destruction on the main thread. */
	std::multimap<uint64_t, SdlRailWindow> _deadWindows;
	/* WM move/resize in progress; report the final rect when it ends. Wayland tracks size only. */
	uint32_t _localMoveId = 0;
	bool _localMoveWayland = false;
	SDL_Point _localMoveGrabPos = { 0, 0 }; /* server-absolute grab point; close the loop here */
	uint16_t _localMoveType = 0;            /* RAIL_WMSZ_* of the active local move */
	/* Wayland: a WINDOW_RESIZED has arrived since the grab started, so the pending op really
	 * resized (guards against a bare pointer re-enter finalizing a no-op click). Main thread. */
	bool _localMoveSawResize = false;
	/* Last focused app window ID. */
	uint64_t _focusedAppId = 0;
	/* Server-driven z-order list. */
	std::vector<uint32_t> _zOrder;
	std::vector<uint32_t> _appliedZOrder;
	uint32_t _activeWindowId = 0;
	bool _zOrderDirty = false;
	/* Icon cache (RDP thread): index = cacheId * entries + cacheEntry, sized from settings. */
	std::vector<SdlRailIcon> _iconCache;
	SdlRailIcon _iconScratch;
	uint32_t _iconCacheEntries = 0;
};
