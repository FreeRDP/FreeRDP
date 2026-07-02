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
	/* Local focus change: send ClientActivate, like xf FocusIn/FocusOut. */
	void handleFocus(SDL_WindowID id, bool gained);
	/* Rewrite window-local (x,y) to server-absolute; false if id is not a RAIL window. */
	bool translateToServer(SDL_WindowID id, float& x, float& y);

	/* Main thread: start the WM-native interactive move/resize the server requested
	 * (payload-carried so back-to-back requests can't clobber each other). */
	void handleLocalMoveRequested(uint32_t windowId, SDL_Point pos, uint16_t moveType);
	/* Main thread: after a WM move ends, report the final position via ClientWindowMove (X11 only).
	 */
	void completeLocalMoveIfPending();

  private:
	/* _windows helpers: callers must hold _windowsLock. */
	[[nodiscard]] SdlRailWindow* getWindow(uint64_t id);
	[[nodiscard]] SdlRailWindow* getWindowBySdlId(SDL_WindowID id);
	SdlRailWindow* addWindow(uint64_t id, const SDL_Rect& rect);

	void enableRemoteAppMode(bool enable);

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
	static BOOL window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo);
	static BOOL monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                              const MONITORED_DESKTOP_ORDER* monitoredDesktop);
	static BOOL non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo);

	static SdlRail* get(rdpContext* context);

  private:
	SdlContext* _context;
	RailClientContext* _rail = nullptr;
	bool _enabled = false;
	bool _refreshSent = false; /* one full RefreshRect per connection, at first window realize */
	/* Guards _windows (RDP thread mutates, main thread paints/iterates, GFX thread looks up).
	 * Erased on the main thread only, so SDL windows are destroyed there. */
	mutable std::mutex _windowsLock;
	std::map<uint64_t, SdlRailWindow> _windows;
	/* WM move/resize in progress; report the final rect when it ends. Wayland tracks size only. */
	uint32_t _localMoveId = 0;
	bool _localMoveWayland = false;
	SDL_Point _localMoveStart = { 0, 0 };
	SDL_Point _localMoveStartSize = { 0, 0 };
};
