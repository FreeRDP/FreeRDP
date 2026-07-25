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
#include <algorithm>
#include <tuple>

#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/freerdp.h>
#include <freerdp/log.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>
#include <freerdp/client/rail.h>

#include "sdl_rail.hpp"
#include "sdl_context.hpp"
#include "sdl_rail_platform.hpp"
#include "sdl_types.hpp"
#include "sdl_utils.hpp"
#include "sdl_wayland.hpp"
#include "sdl_x11.hpp"

#define TAG CLIENT_TAG("sdl.rail")

SdlRail::SdlRail(SdlContext* context) : _context(context)
{
}

SdlRail::~SdlRail() = default;

SdlRail* SdlRail::get(rdpContext* context)
{
	auto sdl = get_context(context);
	if (!sdl)
		return nullptr;
	return &sdl->getRailChannelContext();
}

SdlRailWindow* SdlRail::getWindow(uint64_t id)
{
	auto it = _windows.find(id);
	return (it == _windows.end()) ? nullptr : &it->second;
}

SdlRailWindow* SdlRail::getWindowBySdlId(SDL_WindowID id)
{
	for (auto& it : _windows)
	{
		if (it.second.sdlId() == id)
			return &it.second;
	}
	return nullptr;
}

SdlRailWindow* SdlRail::resolveParent(uint64_t ownerId)
{
	auto* owner = getWindow(ownerId);
	if (owner && !owner->isPopup() && owner->window())
		return owner;
	return nullptr;
}

bool SdlRail::ownsWindow(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	return getWindowBySdlId(id) != nullptr;
}

void SdlRail::invalidateWindow(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow)
		return;
	appWindow->invalidateAll();
	lock.unlock();
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
}

/* Caller holds _windowsLock. */
void SdlRail::sendSystemCommand(SdlRailWindow* appWindow, uint16_t command)
{
	if (!_rail || !_rail->ClientSystemCommand)
		return;
	RAIL_SYSCOMMAND_ORDER syscommand = {};
	syscommand.windowId = static_cast<UINT32>(appWindow->id());
	syscommand.command = command;
	std::ignore = _rail->ClientSystemCommand(_rail, &syscommand);
}

void SdlRail::sendWorkArea(const SDL_Rect& area)
{
	if (!_rail || !_rail->ClientSystemParam || (area.w <= 0) || (area.h <= 0))
		return;
	if (SDL_RectsEqual(&area, &_sentWorkArea))
		return;

	RAIL_SYSPARAM_ORDER param = {};
	/* ClientSystemParam dispatches on the params mask, not .param. */
	param.params = SPI_MASK_SET_WORK_AREA;
	param.workArea.left = WINPR_ASSERTING_INT_CAST(UINT16, area.x);
	param.workArea.top = WINPR_ASSERTING_INT_CAST(UINT16, area.y);
	param.workArea.right = WINPR_ASSERTING_INT_CAST(UINT16, area.x + area.w);
	param.workArea.bottom = WINPR_ASSERTING_INT_CAST(UINT16, area.y + area.h);
	if (_rail->ClientSystemParam(_rail, &param) == CHANNEL_RC_OK)
		_sentWorkArea = area;
}

void SdlRail::handleMaximized(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || appWindow->isPopup() || !appWindow->window())
		return;
	/* Skip the echo of reconcile's own SDL_MaximizeWindow (railMaximized already set). */
	if (appWindow->railMaximized())
	{
		WLog_DBG(TAG, "local maximize skipped id=0x%08" PRIx32 " (already rail-maximized)",
		         static_cast<UINT32>(appWindow->id()));
		return;
	}
	appWindow->setRailMaximized(true);
	/* Force full repaint (skips dirty-rect). */
	appWindow->invalidateAll();
	/* A maximize completing a local drag (WM snap-to-top): the server's modal move loop is still
	 * unwinding and would swallow or drag-restore an immediate SC_MAXIMIZE; defer it until the
	 * server's move/size END order confirms the loop closed (server_local_move_size). */
	if (appWindow->loopEndPending())
	{
		appWindow->deferMaximize();
		WLog_DBG(TAG, "local maximize deferred id=0x%08" PRIx32 " (modal loop open)",
		         static_cast<UINT32>(appWindow->id()));
	}
	else
	{
		WLog_DBG(TAG, "local maximize id=0x%08" PRIx32 " -> SC_MAXIMIZE",
		         static_cast<UINT32>(appWindow->id()));
		sendSystemCommand(appWindow, SC_MAXIMIZE);
	}
	lock.unlock();
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
}

void SdlRail::handleMinimized(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || appWindow->isPopup() || !appWindow->window())
		return;
	if (appWindow->railMinimized())
		return; /* echo of reconcile's own SDL_MinimizeWindow */
	appWindow->setRailMinimized(true);
	WLog_DBG(TAG, "local minimize id=0x%08" PRIx32 " -> SC_MINIMIZE",
	         static_cast<UINT32>(appWindow->id()));
	sendSystemCommand(appWindow, SC_MINIMIZE);
}

void SdlRail::handleClose(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || !appWindow->window())
		return;
	WLog_DBG(TAG, "local close id=0x%08" PRIx32 " -> SC_CLOSE",
	         static_cast<UINT32>(appWindow->id()));
	sendSystemCommand(appWindow, SC_CLOSE);
}

void SdlRail::handleRestored(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || appWindow->isPopup() || !appWindow->window())
		return;
	/* Resolve which state a RESTORED event ends; both clear = echo of our own restore, skip. */
	if (appWindow->railMinimized())
		appWindow->setRailMinimized(false);
	else if (appWindow->railMaximized())
		appWindow->setRailMaximized(false);
	else
		return;
	WLog_DBG(TAG, "local restore id=0x%08" PRIx32 " -> SC_RESTORE",
	         static_cast<UINT32>(appWindow->id()));
	appWindow->invalidateAll();
	sendSystemCommand(appWindow, SC_RESTORE);
	lock.unlock();
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
}

void SdlRail::handleFocus(SDL_WindowID id, bool gained)
{
	std::unique_lock lock(_windowsLock);
	auto appWindow = getWindowBySdlId(id);
	if (!appWindow || appWindow->isPopup() || !appWindow->window())
		return;

	/* Fallback parent for orphaned popups. */
	if (gained)
		_focusedAppId = appWindow->id();

	/* ClientActivate only. Do NOT SDL_RaiseWindow to avoid WM focus loops. */
	const uint32_t wid = static_cast<uint32_t>(appWindow->id());
	sendClientActivate(wid, gained);
}

/* Send RAIL_ACTIVATE_ORDER and track the active window id. Caller holds _windowsLock. */
void SdlRail::sendClientActivate(uint32_t wid, bool enabled)
{
	if (!_rail || !_rail->ClientActivate)
		return;
	if (enabled)
		_clientActiveId = wid;
	RAIL_ACTIVATE_ORDER activate = {};
	activate.windowId = wid;
	activate.enabled = enabled;
	std::ignore = _rail->ClientActivate(_rail, &activate);
}

/* Activate clicked window on server to route input correctly. */
void SdlRail::ensureActive(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || appWindow->isPopup() || !appWindow->window())
		return;
	const uint32_t wid = static_cast<uint32_t>(appWindow->id());
	if (wid == _clientActiveId)
		return;
	sendClientActivate(wid, true);
}

bool SdlRail::translateToServer(SDL_WindowID id, float& x, float& y)
{
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow)
		return false;
	SDL_FPoint rpos = { x, y };
	if (auto* renderer = appWindow->renderer())
		(void)SDL_RenderCoordinatesFromWindow(renderer, x, y, &rpos.x, &rpos.y);
	/* Convert window-local coords to server-absolute. */
	const SDL_Rect outer = appWindow->outerRect();
	x = rpos.x + static_cast<float>(outer.x);
	y = rpos.y + static_cast<float>(outer.y);
	return true;
}

SdlRailWindow* SdlRail::addWindow(uint64_t id, const SDL_Rect& rect)
{
	/* An id recreated before paint could erase its deleted entry would otherwise inherit that
	 * entry's _deleted flag (emplace is a no-op on a live key) and vanish on the next drain. Move
	 * the stale entry aside (node transfer keeps the non-movable object in place) so the fresh
	 * window owns the id; the stale SDL window/band are freed on the main thread in paint. */
	auto it = _windows.find(id);
	if ((it != _windows.end()) && it->second.isDeleted())
	{
		WLog_DBG(TAG, "window recreate id=0x%08" PRIx32 " reused a deleted entry",
		         static_cast<uint32_t>(id));
		_deadWindows.insert(_windows.extract(it));
	}
	/* emplace is a no-op if the id already exists; either way first->second is the window. */
	auto res = _windows.emplace(std::piecewise_construct, std::forward_as_tuple(id),
	                            std::forward_as_tuple(id, rect));
	return &res.first->second;
}

void SdlRail::enableRemoteAppMode(bool enable)
{
	_enabled = enable;
}

bool SdlRail::paint(SDL_Surface* primary, SDL_PixelFormat fallbackFormat,
                    const std::vector<SDL_Rect>& damage)
{
	if (!_enabled)
		return true;

	/* Report workarea once (avoids maximizing under local panels). */
	if (_sentWorkArea.w == 0)
	{
		SDL_Rect usable{};
		if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
			sendWorkArea(usable);
	}

	std::unique_lock lock(_windowsLock);

	/* Erase RDP-thread-deleted entries here so SDL windows die on the main thread. */
	for (auto it = _windows.begin(); it != _windows.end();)
	{
		if (it->second.isDeleted())
			it = _windows.erase(it);
		else
			++it;
	}
	/* Windows displaced by a same-id recreate (addWindow) also die here, on the main thread. */
	_deadWindows.clear();

	/* App windows first so popup parents exist before their popups. */
	for (auto& it : _windows)
	{
		auto& win = it.second;
		if (win.isPopup())
			continue;
		/* Parent owned dialogs to owner. */
		SDL_Window* parent = nullptr;
		SDL_Rect parentRect{};
		auto* owner = resolveParent(win.owner());
		if (owner && (owner != &win))
		{
			parent = owner->window();
			parentRect = owner->outerRect(); /* the SDL window's on-screen geometry */
		}
		win.paint(primary, fallbackFormat, damage, parent, parentRect);
	}

	/* Request full repaint after first window is realized (fixes reconnect blank windows). */
	if (!_refreshSent)
	{
		for (auto& it : _windows)
		{
			if (it.second.isPopup() || !it.second.window())
				continue;
			auto* ctx = &_context->common()->context;
			const auto dw = static_cast<UINT16>(
			    freerdp_settings_get_uint32(ctx->settings, FreeRDP_DesktopWidth));
			const auto dh = static_cast<UINT16>(
			    freerdp_settings_get_uint32(ctx->settings, FreeRDP_DesktopHeight));
			const RECTANGLE_16 all = { 0, 0, dw, dh };
			if (ctx->update && ctx->update->RefreshRect)
				(void)ctx->update->RefreshRect(ctx, 1, &all);
			_refreshSent = true;
			WLog_DBG(TAG, "refresh-rect sent (first app window realized)");
			break;
		}
	}

	for (auto& it : _windows)
	{
		auto& popup = it.second;
		if (!popup.isPopup())
			continue;

		/* A drop shadow (layered popup) is only worth drawing when it decorates a MENU/tooltip: it
		 * then adjoins that visible popup. A shadow that adjoins only a top-level app window is its
		 * frame shadow - it overlaps our resize band (hover shows a resize cursor, clicks resize
		 * the app), lags the window on every move, and fragments (one edge left behind) because the
		 * four edge bars are independent windows. Gate those out by anchoring the shadow to a real
		 * popup; server-coord rects are reliable on both backends. */
		if (popup.isLayered())
		{
			constexpr int reach = 48; /* shadow offset+blur spread from its popup, server px */
			const SDL_Rect sr = popup.windowRect();
			bool anchored = false;
			for (auto& other : _windows)
			{
				auto& cand = other.second;
				if ((&cand == &popup) || !cand.isPopup() || cand.isLayered() || !cand.window() ||
				    ((SDL_GetWindowFlags(cand.window()) & SDL_WINDOW_HIDDEN) != 0))
					continue;
				SDL_Rect zone = cand.windowRect();
				zone.x -= reach;
				zone.y -= reach;
				zone.w += 2 * reach;
				zone.h += 2 * reach;
				/* Containment, not intersection: a menu shadow hugs its popup and fits inside this
				 * zone. An app-window frame-edge shadow is a full-height/width bar that only
				 * crosses the popup's column/row; it extends far beyond the zone, so it is not a
				 * shadow of this popup and must not be adopted (else it paints a bar overflowing
				 * the menu). */
				const bool inside = (sr.x >= zone.x) && (sr.y >= zone.y) &&
				                    (sr.x + sr.w <= zone.x + zone.w) &&
				                    (sr.y + sr.h <= zone.y + zone.h);
				if (inside)
				{
					anchored = true;
					break;
				}
			}
			popup.setShadowAnchored(anchored);
		}

		/* Pick popup parent: ownerWindowId -> geometric match -> focused -> any app. */
		SdlRailWindow* chosen = resolveParent(popup.owner());
		if (!chosen && railPlatformCaps().positionsReadable)
		{
			const SDL_Rect pr = popup.windowRect();
			const SDL_Point origin = { pr.x, pr.y };
			for (auto& other : _windows)
			{
				auto& w = other.second;
				if (w.isPopup() || !w.window())
					continue;
				const SDL_Rect wr = w.windowRect();
				if (SDL_PointInRect(&origin, &wr))
				{
					chosen = &w;
					break;
				}
			}
		}
		if (!chosen)
			chosen = resolveParent(_focusedAppId);
		if (!chosen)
		{
			for (auto& other : _windows)
			{
				auto& w = other.second;
				if (!w.isPopup() && w.window())
				{
					chosen = &w;
					break;
				}
			}
		}

		SDL_Window* parent = nullptr;
		SDL_Rect parentRect{};
		if (chosen)
		{
			parent = chosen->window();
			parentRect = chosen->outerRect(); /* the SDL window's on-screen geometry */
		}
		else
			WLog_WARN(TAG, "popup id=0x%08" PRIx32 " has no parent app window",
			          static_cast<UINT32>(popup.id()));
		popup.paint(primary, fallbackFormat, damage, parent, parentRect);
	}

	/* All live windows now have real X11 handles: realize the server's z-order. */
	applyZOrder();
	return true;
}

/* Caller holds _windowsLock (main thread). */
void SdlRail::applyZOrder()
{
	if (!_zOrderDirty)
		return;
	/* X11 only: Wayland/Win/macOS have no reparenting-safe, focus-neutral restack path here. */
	if (!sdl::utils::isX11Driver())
	{
		_zOrderDirty = false;
		return;
	}
	/* Never restack the window the WM is actively dragging; keep dirty and retry after the move. */
	if (_localMove.id != 0)
	{
		WLog_VRB(TAG, "zorder apply deferred: local move 0x%08" PRIx32 " active", _localMove.id);
		return;
	}
	if (_zOrder == _appliedZOrder) /* drop identical server resends */
	{
		_zOrderDirty = false;
		return;
	}

	/* Restack top-level windows (skip popups and hidden windows). */
	std::vector<SDL_Window*> stack;
	stack.reserve(_zOrder.size());
	for (uint32_t id : _zOrder)
	{
		auto* w = getWindow(id);
		if (w && !w->isPopup() && w->window() &&
		    ((SDL_GetWindowFlags(w->window()) & SDL_WINDOW_HIDDEN) == 0))
			stack.push_back(w->window());
	}
	if (stack.size() >= 2)
	{
		std::ignore = sdl_x11_restack_windows(stack);
	}

	_appliedZOrder = _zOrder;
	_zOrderDirty = false;
}

UINT SdlRail::updateWindowFromSurface(gdiGfxSurface* surface)
{
	if (!surface)
		return CHANNEL_RC_OK;

	std::unique_lock lock(_windowsLock);
	auto appWindow = getWindow(surface->windowId);
	if (!appWindow)
	{
		/* Drop GFX surface for unknown windows (repaints later). */
		WLog_VRB(TAG, "gfx surface for untracked id=0x%08" PRIx32, surface->windowId);
		return CHANNEL_RC_OK;
	}

	const uint32_t w = surface->mappedWidth ? surface->mappedWidth : surface->width;
	const uint32_t h = surface->mappedHeight ? surface->mappedHeight : surface->height;

	/* Consume per-frame damage. */
	UINT32 nbRects = 0;
	const RECTANGLE_16* rects = region16_rects(&surface->invalidRegion, &nbRects);
	appWindow->updateGfxSurface(surface->data, surface->scanline, w, h, rects, nbRects);
	region16_clear(&surface->invalidRegion);

	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
	return CHANNEL_RC_OK;
}

UINT SdlRail::UpdateWindowFromSurface(RdpgfxClientContext* context, gdiGfxSurface* surface)
{
	WINPR_ASSERT(context);
	auto gdi = static_cast<rdpGdi*>(context->custom);
	if (!gdi || !gdi->context)
		return CHANNEL_RC_OK;
	auto sdl = get_context(gdi->context);
	if (!sdl)
		return CHANNEL_RC_OK;
	return sdl->getRailChannelContext().updateWindowFromSurface(surface);
}

bool SdlRail::init(RailClientContext* rail)
{
	_rail = rail;
	if (!rail)
		return false;

	registerUpdateCallbacks(_context->context()->update);

	{
		std::unique_lock lock(_windowsLock);
		/* A reconnect reuses this instance; uninit only marked the old windows (SDL_DestroyWindow
		 * is main-thread only). Move any leftovers to the graveyard so paint() destroys them on the
		 * main thread instead of clearing (and destroying) them here on the RDP thread. */
		for (auto it = _windows.begin(); it != _windows.end();)
		{
			auto cur = it++;
			_deadWindows.insert(_windows.extract(cur));
		}
		auto* settings = _context->context()->settings;
		const uint32_t caches =
		    freerdp_settings_get_uint32(settings, FreeRDP_RemoteAppNumIconCaches);
		_iconCacheEntries =
		    freerdp_settings_get_uint32(settings, FreeRDP_RemoteAppNumIconCacheEntries);
		_iconCache.assign(static_cast<size_t>(caches) * _iconCacheEntries, {});
	}

	rail->custom = this;
	rail->ServerExecuteResult = SdlRail::server_execute_result;
	/* ServerSystemParam not implemented: the server only sends screensaver state here. */
	rail->ServerLocalMoveSize = SdlRail::server_local_move_size;
	rail->ServerMinMaxInfo = SdlRail::server_min_max_info;
	/* Keep default ServerHandshake. */

	WLog_WARN(TAG, "RemoteApp/RAIL support in the SDL client is experimental");
	const RailPlatformCaps& caps = railPlatformCaps();
	WLog_DBG(TAG, "RAIL channel initialized: driver=%s positionsReadable=%d transparentWindows=%d",
	         sdl::utils::isWaylandDriver() ? "wayland"
	                                       : (sdl::utils::isX11Driver() ? "x11" : "other"),
	         caps.positionsReadable ? 1 : 0, caps.supportsTransparentWindows ? 1 : 0);
	/* Arm _NET_WM_SYNC_REQUEST on X11 for lockstep opaque resizes. */
	if (caps.positionsReadable)
		SDL_SetHint(SDL_HINT_VIDEO_X11_ENABLE_XSYNC_EXT, "1");
	return true;
}

bool SdlRail::uninit(RailClientContext* /*rail*/)
{
	_refreshSent = false;
	std::unique_lock lock(_windowsLock);
	/* Reset all move/completion state: a reconnect reuses this instance, and Windows reuses
	 * window ids, so leftovers would gate a next-session window. */
	_localMove = {};
	/* uninit runs on the RDP/channel disconnect thread; SDL_DestroyWindow must run on the main
	 * thread. Do NOT clear the maps here - mark every window dead so paint() reaps them on the main
	 * thread (reconnect reuses this instance), and let ~SdlRail drain them there on final teardown.
	 * _deadWindows is likewise left for the main thread. */
	WLog_DBG(TAG, "RAIL channel uninit, marking %zu windows for main-thread teardown",
	         _windows.size());
	for (auto& kv : _windows)
		kv.second.markDeleted();
	_rail = nullptr;
	return true;
}

void SdlRail::registerUpdateCallbacks(rdpUpdate* update)
{
	WINPR_ASSERT(update);
	auto window = update->window;
	WINPR_ASSERT(window);

	window->WindowCreate = SdlRail::window_common;
	window->WindowUpdate = SdlRail::window_common;
	window->WindowIcon = SdlRail::window_icon;
	window->WindowCachedIcon = SdlRail::window_cached_icon;
	window->WindowDelete = SdlRail::window_delete;
	window->MonitoredDesktop = SdlRail::monitored_desktop;
	window->NonMonitoredDesktop = SdlRail::non_monitored_desktop;
}

/* --- server callbacks --- */

UINT SdlRail::server_execute_result(RailClientContext* context,
                                    const RAIL_EXEC_RESULT_ORDER* execResult)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(execResult);
	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);

	if (execResult->execResult != RAIL_EXEC_S_OK)
	{
		WLog_ERR(TAG, "RAIL exec error: execResult=0x%04" PRIx16 " rawResult=0x%08" PRIx32,
		         execResult->execResult, execResult->rawResult);
		freerdp_abort_connect_context(rail->_context->context());
	}
	else
	{
		WLog_DBG(TAG, "RemoteApp exec OK, enabling RAIL mode");
		rail->enableRemoteAppMode(true);
	}
	return CHANNEL_RC_OK;
}

UINT SdlRail::server_local_move_size(RailClientContext* context,
                                     const RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(localMoveSize);

	/* Start native move/resize to avoid dual-authority geometry races. */
	if (localMoveSize->isMoveSizeStart && (localMoveSize->moveSizeType >= RAIL_WMSZ_LEFT) &&
	    (localMoveSize->moveSizeType <= RAIL_WMSZ_MOVE))
	{
		WLog_DBG(TAG, "server move/size start id=0x%08" PRIx32 " type=%" PRIu16 " pos=%d,%d",
		         localMoveSize->windowId, localMoveSize->moveSizeType, localMoveSize->posX,
		         localMoveSize->posY);
		(void)sdl_push_user_event(SDL_EVENT_USER_RAIL_MOVE, localMoveSize->windowId, 0,
		                          static_cast<int>(localMoveSize->moveSizeType));
	}
	else if (!localMoveSize->isMoveSizeStart)
	{
		WLog_DBG(TAG, "server move/size end id=0x%08" PRIx32, localMoveSize->windowId);
		/* Server ended the move/size: resume applying geometry + input (covers Wayland move). */
		auto rail = static_cast<SdlRail*>(context->custom);
		if (rail)
		{
			std::unique_lock lock(rail->_windowsLock);
			/* Ignore server end order during Wayland resize - unless no compositor resize ever
			 * started (begin_resize silently refused): then the loop closed on the forwarded
			 * release and the server just resized by press->release delta. Drop the latch so that
			 * geometry (arriving right after this END) applies instead of being filtered as a
			 * drag echo - the window would otherwise freeze at the stale size. */
			const bool ownedByWaylandResize = rail->_localMove.wayland &&
			                                  (rail->_localMove.id == localMoveSize->windowId) &&
			                                  rail->_localMove.sawResize;
			if (!ownedByWaylandResize)
			{
				if (auto* appWindow = rail->getWindow(localMoveSize->windowId))
					appWindow->setLocalMoveActive(false);
				if (rail->_localMove.id == localMoveSize->windowId)
				{
					if (rail->_localMove.wayland)
					{
						/* Grab-less Wayland drag: latch is dead now; repaint clears the dashed
						 * placeholder. */
						rail->_localMove = {};
						if (auto* appWindow = rail->getWindow(localMoveSize->windowId))
							appWindow->invalidateAll();
						(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
					}
					else
					{
						/* END while our drag is still pending = the app closed its own loop early
						 * (window destroyed, app-side cancel): the eventual release must not send
						 * the synthetic button-up (a phantom click) nor arm this window's loop-end
						 * (never cleared - this END already passed). */
						rail->_localMove.serverEnded = true;
					}
				}
			}
			/* The modal loop is now confirmed closed - the END order cannot pre-date our release
			 * (a geometry echo can): a deferred SC_MAXIMIZE is safe to send. */
			auto* appWindow = rail->getWindow(localMoveSize->windowId);
			if (appWindow && appWindow->loopEndPending())
			{
				const auto actions = appWindow->takeLoopEnd();
				if (actions.maximize && appWindow->railMaximized())
				{
					WLog_DBG(TAG, "deferred SC_MAXIMIZE id=0x%08" PRIx32, localMoveSize->windowId);
					rail->sendSystemCommand(appWindow, SC_MAXIMIZE);
				}
				if (actions.snap)
				{
					/* WM snap/tile sized the window during the move: the loop is closed now, so a
					 * ClientWindowMove carrying the WM rect is no longer swallowed by it. */
					WLog_DBG(TAG, "deferred snap resize id=0x%08" PRIx32 " rect=%d,%d %dx%d",
					         localMoveSize->windowId, actions.snapRect.x, actions.snapRect.y,
					         actions.snapRect.w, actions.snapRect.h);
					rail->sendClientWindowMove(appWindow, actions.snapRect);
				}
			}
		}
	}
	return CHANNEL_RC_OK;
}

/* X11 only: suppress button-up during modal loop. */
bool SdlRail::suppressServerInput(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	if (_localMove.id == 0)
		return false;
	auto* appWindow = getWindowBySdlId(id);
	return appWindow && appWindow->localMoveActive() && !_localMove.wayland;
}

bool SdlRail::suppressServerMotion(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	if (_localMove.id == 0)
		return false;
	auto* appWindow = getWindowBySdlId(id);
	/* Both backends: with a compositor grab no motion arrives anyway; without one (a Wayland
	 * begin_resize the compositor silently refused) forwarded motion would keep feeding the
	 * server's own modal resize while its geometry updates are ignored as drag echoes - the
	 * local window freezes at the stale size. Button events still pass: the real release is
	 * what closes the server loop on Wayland. */
	return appWindow && appWindow->localMoveActive();
}

/* RAIL_WMSZ_* -> _NET_WM_MOVERESIZE direction (0..7 = resize edges, 8 = move). */
static int railToNetDirection(uint16_t moveType)
{
	switch (moveType)
	{
		case RAIL_WMSZ_TOPLEFT:
			return 0;
		case RAIL_WMSZ_TOP:
			return 1;
		case RAIL_WMSZ_TOPRIGHT:
			return 2;
		case RAIL_WMSZ_RIGHT:
			return 3;
		case RAIL_WMSZ_BOTTOMRIGHT:
			return 4;
		case RAIL_WMSZ_BOTTOM:
			return 5;
		case RAIL_WMSZ_BOTTOMLEFT:
			return 6;
		case RAIL_WMSZ_LEFT:
			return 7;
		case RAIL_WMSZ_MOVE:
		default:
			return 8;
	}
}

/* Single source of truth for the RAIL_WMSZ_* <-> XDG_TOPLEVEL_RESIZE_EDGE_* edge pairing. Numeric
 * xdg literals (not the xdg-shell enum) keep this free of the Wayland protocol header. */
static constexpr struct
{
	uint16_t rail;
	uint32_t xdg;
} kRailXdgEdges[] = {
	{ RAIL_WMSZ_TOP, 1 },      { RAIL_WMSZ_BOTTOM, 2 },       { RAIL_WMSZ_LEFT, 4 },
	{ RAIL_WMSZ_TOPLEFT, 5 },  { RAIL_WMSZ_BOTTOMLEFT, 6 },   { RAIL_WMSZ_RIGHT, 8 },
	{ RAIL_WMSZ_TOPRIGHT, 9 }, { RAIL_WMSZ_BOTTOMRIGHT, 10 },
};

/* RAIL_WMSZ_* -> XDG_TOPLEVEL_RESIZE_EDGE_* (0 = none, e.g. RAIL_WMSZ_MOVE). */
static uint32_t railToXdgEdge(uint16_t moveType)
{
	for (const auto& e : kRailXdgEdges)
		if (e.rail == moveType)
			return e.xdg;
	return 0;
}

/* Which window edges a RAIL_WMSZ_* drag moves; the opposite edges stay anchored. */
struct RailEdges
{
	bool left = false;
	bool right = false;
	bool top = false;
	bool bottom = false;
};
static RailEdges railEdges(uint16_t t)
{
	return { (t == RAIL_WMSZ_TOPLEFT) || (t == RAIL_WMSZ_LEFT) || (t == RAIL_WMSZ_BOTTOMLEFT),
		     (t == RAIL_WMSZ_TOPRIGHT) || (t == RAIL_WMSZ_RIGHT) || (t == RAIL_WMSZ_BOTTOMRIGHT),
		     (t == RAIL_WMSZ_TOPLEFT) || (t == RAIL_WMSZ_TOP) || (t == RAIL_WMSZ_TOPRIGHT),
		     (t == RAIL_WMSZ_BOTTOMLEFT) || (t == RAIL_WMSZ_BOTTOM) ||
		         (t == RAIL_WMSZ_BOTTOMRIGHT) };
}

void SdlRail::handleLocalMoveRequested(uint32_t windowId, uint16_t moveType)
{
	const bool wayland = sdl::utils::isWaylandDriver();
	const bool x11 = sdl::utils::isX11Driver();
	if (!wayland && !x11)
		return;

	std::unique_lock lock(_windowsLock);
	auto appWindow = getWindow(windowId);
	if (!appWindow || !appWindow->window() || appWindow->isPopup())
		return;

	const bool isMove = (moveType == RAIL_WMSZ_MOVE);
	WLog_DBG(TAG, "local move start id=0x%08" PRIx32 " driver=%s type=%" PRIu16 " %s", windowId,
	         wayland ? "wayland" : "x11", moveType, isMove ? "move" : "resize");
	bool started = false;
	if (wayland)
	{
		/* Wayland: positions unreadable, sizes readable. */
		if (isMove)
			started = sdl_wayland_begin_move(appWindow->window());
		else
			started = sdl_wayland_begin_resize(appWindow->window(), railToXdgEdge(moveType));
		if (started && !isMove)
		{
			_localMove.id = windowId;
			_localMove.wayland = true;
			_localMove.type = moveType;
			/* Reset resize guard for new drag. */
			_localMove.sawResize = false;
		}
	}
	else
	{
		/* X11: hand resize to WM, suppress input. */
		started = sdl_x11_begin_move_size(appWindow->window(), railToNetDirection(moveType));
		if (started)
		{
			_localMove.id = windowId;
			_localMove.wayland = false;
			_localMove.type = moveType;
			/* Latch the anchor of the server's modal loop (the forwarded press that started it)
			 * and the server's frozen cursor as per-drag state; the completion releases
			 * relative to them. */
			_localMove.anchor = _lastPressServer;
			_localMove.pointer = _lastPointerServer;
			_localMove.wmSized = false;
			_localMove.serverEnded = false;
			/* A fresh drag supersedes this window's own unsent snap rect / un-sticks a completion
			 * gate whose END order never arrived. Per-window, so another window's pending close
			 * survives automatically. */
			appWindow->clearLoopEnd();
			/* Pin the stale contents to the fixed corner while the WM drags: X anchors them
			 * top-left by default on every resize step, which flicker-fights the re-anchored
			 * repaints on left/top drags. */
			const RailEdges e = railEdges(moveType);
			const int row = e.top ? 2 : (e.bottom ? 0 : 1);
			const int col = e.left ? 2 : (e.right ? 0 : 1);
			(void)sdl_x11_set_bit_gravity(appWindow->window(), row * 3 + col + 1);
		}
	}
	/* Drive local frame during X11/Wayland drag. */
	if (started && (!wayland || !isMove))
	{
		appWindow->setLocalMoveActive(true);
		if (!isMove)
		{
			/* Anchor the stale frame to the fixed edge (opposite the dragged one). */
			const RailEdges e = railEdges(moveType);
			appWindow->setResizeAnchor(e.left, e.top);
		}
	}
	else if (!started)
		WLog_WARN(TAG, "WM move failed for RAIL window 0x%08" PRIx32, windowId);
}

/* Report a server-coords rect via ClientWindowMove. The order carries the frame INCLUDING the
 * server's invisible borders (raw resize margins, xf parity); reporting the visible rect instead
 * shrinks the window by the margin box on every honored report. Caller holds _windowsLock. */
void SdlRail::sendClientWindowMove(SdlRailWindow* appWindow, const SDL_Rect& serverRect)
{
	const SDL_Rect fm = appWindow->frameMargins();
	RAIL_WINDOW_MOVE_ORDER move = {};
	move.windowId = static_cast<UINT32>(appWindow->id());
	move.left = WINPR_ASSERTING_INT_CAST(INT16, serverRect.x - fm.x);
	move.top = WINPR_ASSERTING_INT_CAST(INT16, serverRect.y - fm.y);
	move.right = WINPR_ASSERTING_INT_CAST(INT16, serverRect.x + serverRect.w + fm.w);
	move.bottom = WINPR_ASSERTING_INT_CAST(INT16, serverRect.y + serverRect.h + fm.h);
	if (_rail && _rail->ClientWindowMove &&
	    (_rail->ClientWindowMove(_rail, &move) != CHANNEL_RC_OK))
		WLog_WARN(TAG, "ClientWindowMove failed for RAIL window 0x%08" PRIx32, move.windowId);
}

/* Report final geometry and adopt locally. */
void SdlRail::reportAndAdopt(SdlRailWindow* appWindow, int x, int y, int w, int h)
{
	/* Local geometry -> server rect: strip the client-side band insets. */
	const SDL_Rect rect = appWindow->serverRect({ x, y, w, h });
	/* Report outer frame to server (inflate by margins). */
	const bool maximized = appWindow->effectivelyMaximized();
	/* Only report when the rect actually changed (xf parity: "if current window position
	 * disagrees with RDP window position, send update"). */
	const SDL_Rect cur = appWindow->windowRect();
	const bool unchanged = SDL_RectsEqual(&rect, &cur);
	const SDL_Rect fm = appWindow->frameMargins();
	if (!maximized && !unchanged)
		sendClientWindowMove(appWindow, rect);

	/* Adopt WM final geometry and resume input routing. */
	if (maximized)
		appWindow->setLocalMoveActive(false); /* geometry is WM/server-owned while maximized */
	else
		appWindow->adoptLocalGeometry(rect);
	/* Repaint now: the last presented frame may still be the resize placeholder. */
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
}

void SdlRail::clampIntoDesktop(int& x, int& y, int w, int h) const
{
	auto* settings = _context->context()->settings;
	const int dw = static_cast<int>(freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth));
	const int dh = static_cast<int>(freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight));
	x = std::clamp(x, 0, std::max(0, dw - w));
	y = std::clamp(y, 0, std::max(0, dh - h));
}

void SdlRail::noteDragResize(SDL_WindowID id, int w, int h)
{
	std::unique_lock lock(_windowsLock);
	if ((_localMove.id == 0) || _localMove.wayland || (_localMove.type != RAIL_WMSZ_MOVE))
		return;
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || (static_cast<uint32_t>(appWindow->id()) != _localMove.id))
		return;
	/* Our own mid-move reconcile only resizes after a server size-adopt (drag-restore of a
	 * maximized window), and only ever to the adopted size: filter exactly that echo. Anything
	 * else is WM intent - including a re-tile back to the frozen _windowRect size, or a
	 * DIFFERENT size after the restore (restore, then tile at the edge). */
	if (appWindow->localMoveSizeChanged())
	{
		const SDL_Rect outer = appWindow->outerRect();
		if ((w == outer.w) && (h == outer.h))
			return;
	}
	_localMove.wmSized = true;
	_localMove.wmSize = { w, h };
}

void SdlRail::syncGeometry(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	auto* w = getWindowBySdlId(id);
	if (!w || !w->window() || w->isDeleted() || w->isPopup())
		return;

	/* Geometry is not client-owned in these states. Do NOT clear the apply bracket here: the
	 * restore's SDL_RestoreWindow re-arms it, and clearing while the SDL maximized flag lags the
	 * actual restore would let the restore's intermediate configures through as reports. */
	if (w->effectivelyMaximized() || w->railMinimized())
		return;

	const auto wid = static_cast<uint32_t>(w->id());
	if (w->localMoveActive() || (_localMove.id == wid) || w->loopEndPending() ||
	    w->stateTransitionPending())
		return;

	int lw = 0;
	int lh = 0;
	SDL_GetWindowSize(w->window(), &lw, &lh);
	if ((lw <= 0) || (lh <= 0))
		return;

	/* A 0-content size (window shrunk to just the band ring) is never a geometry the server should
	 * adopt; reporting it craters the window server-side. Healthy WMs never produce one - mutter
	 * does on X11 tile-toggle (untile restores a never-written 0x0 saved_rect; mutter 50.3,
	 * https://gitlab.gnome.org/GNOME/mutter/-/work_items/4918). Drop and stay converged. */
	const SDL_Rect ins = w->insets();
	if (((lw - ins.x - ins.w) <= 0) || ((lh - ins.y - ins.h) <= 0))
	{
		WLog_DBG(TAG, "geometry sync id=0x%08" PRIx32 " %dx%d dropped (content collapsed)", wid, lw,
		         lh);
		return;
	}

	const SDL_Rect vis = w->outerRect();
	const bool posKnown = railPlatformCaps().positionsReadable;
	int lx = vis.x;
	int ly = vis.y;
	if (posKnown)
		SDL_GetWindowPosition(w->window(), &lx, &ly);
	else
		clampIntoDesktop(lx, ly, lw, lh);

	/* Convergence = the window settled at what we last applied. Positions only count where they
	 * are readable: on Wayland lx/ly are our own clamped fallback, and letting the clamp defeat
	 * the equality would leave the apply bracket set forever (window goes report-mute). */
	if ((lw == vis.w) && (lh == vis.h) && (!posKnown || ((lx == vis.x) && (ly == vis.y))))
	{
		w->clearGeomApplyPending();
		return; /* converged / self-echo - nothing to say */
	}

	/* Causal echo bracket: a client-issued apply is still settling, so this divergent event is our
	 * own (possibly WM-mangled) echo, not WM intent - reporting it would poison the server state
	 * (e.g. the restore rect during a maximize gap). Re-asserting the server rect is no better: a
	 * systematic WM mutation would spin a fast event loop. So drop and tolerate the transient
	 * divergence. The bracket is released by convergence (above), by reconcile finding the target
	 * already applied, or by a completed local move. A WM that permanently defies our applies
	 * leaves the window report-mute by design: every escape heuristic tried (time windows, event
	 * counts, order sequencing) re-opened a poison path, and a mouse drag always recovers. */
	if (w->geomApplyPending())
	{
		/* A Wayland compositor resize (content-edge drag the compositor granted late) can run AHEAD
		 * of the server: the window is already LARGER than the size we just applied. That can never
		 * be an echo of our apply - an echo settles AT the applied size, it does not overshoot it -
		 * so it is the live resize outrunning the server. Release the bracket and report the live
		 * size; otherwise every following event is dropped and the window stays stuck oversized
		 * (its content sized to the server rect, a transparent gap out to the compositor size).
		 * Wayland-only: there insets are zero so lw/lh is the bare window, and there is no
		 * completion event after a content-edge resize to release the bracket any other way. */
		const bool overtaken = !posKnown && ((lw > vis.w) || (lh > vis.h));
		if (!overtaken)
			return;
		w->clearGeomApplyPending();
	}

	WLog_DBG(TAG, "geometry sync id=0x%08" PRIx32 " local=%d,%d %dx%d (sync)", wid, lx, ly, lw, lh);
	reportAndAdopt(w, lx, ly, lw, lh);
}

void SdlRail::noteResizeGrab(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	if (_localMove.id == 0)
		return;
	auto* appWindow = getWindowBySdlId(id);
	/* A MOUSE_LEAVE while our resize latch is active means the compositor took the pointer for the
	 * grab (the user is still holding the button - a real leave cannot happen mid-drag). Mark the
	 * grab confirmed now so the server's END order keeps the latch (and the dashed frame): the
	 * first PIXEL_SIZE configure that would otherwise set sawResize can land ~120ms later, losing
	 * the race with the END order and dropping the frame on ~half the drags. */
	if (appWindow && (static_cast<uint32_t>(appWindow->id()) == _localMove.id) &&
	    _localMove.wayland && !_localMove.sawResize)
		_localMove.sawResize = true;
}

void SdlRail::handleWaylandResize(SDL_WindowID id)
{
	if (!sdl::utils::isWaylandDriver())
		return;
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || appWindow->isPopup() || !appWindow->window())
		return;
	/* Only the DRAGGED window's resize counts; an unrelated window's compositor tile must not arm
	 * (or feed) someone else's grab. RAIL ids are non-zero, so a zero _localMove.id (no active
	 * drag) matches nothing. */
	if (static_cast<uint32_t>(appWindow->id()) == _localMove.id)
	{
		if (_localMove.wayland)
			_localMove.sawResize = true;
		return;
	}

	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);
	if ((w <= 0) || (h <= 0))
		return;

	/* A maximize reveals the Wayland work area (unknown up front); report it to the server. */
	if (appWindow->effectivelyMaximized())
	{
		sendWorkArea({ 0, 0, w, h });
		return;
	}

	/* Local window size is the OUTER frame (rect + insets; identical off the inset paths). */
	const SDL_Rect vis = appWindow->outerRect();
	if ((w == vis.w) && (h == vis.h))
		return; /* echo of a size we already reported/applied - nothing new */

	/* Compositor snap/tile: report via the debounced sync (never from transitional state). */
	lock.unlock();
	syncGeometry(id);
}

void SdlRail::completeWaylandResize(bool definitive)
{
	std::unique_lock lock(_windowsLock);
	if ((_localMove.id == 0) || !_localMove.wayland)
		return;
	/* Only a drag that actually resized ends here; a bare re-enter is the spurious/idle enter.
	 * A definitive end (a band enter or a fresh button-down: the old grab is over) cancels a
	 * no-op click grab instead - left latched, its dashed placeholder would stay on screen, the
	 * window would ignore server geometry, and the next unrelated resize would be misattributed
	 * to it. */
	if (!_localMove.sawResize)
	{
		if (definitive)
		{
			if (auto* appWindow = getWindow(_localMove.id))
			{
				appWindow->setLocalMoveActive(false);
				/* Repaint now: the dashed placeholder is on screen and an undamaged window would
				 * otherwise keep it until the next server frame. */
				appWindow->invalidateAll();
				(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
			}
			_localMove.id = 0;
		}
		return;
	}

	auto appWindow = getWindow(_localMove.id);
	_localMove.id = 0;
	if (!appWindow || !appWindow->window())
		return;

	(void)SDL_SyncWindow(appWindow->window());
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);

	/* Derive Wayland origin from anchor edge. */
	const SDL_Rect start = appWindow->windowRect();
	const RailEdges e = railEdges(_localMove.type);
	int x = e.left ? (start.x + start.w - w) : start.x;
	int y = e.top ? (start.y + start.h - h) : start.y;
	clampIntoDesktop(x, y, w, h);
	reportAndAdopt(appWindow, x, y, w, h);
}

void SdlRail::completeLocalMoveIfPending()
{
	std::unique_lock lock(_windowsLock);
	if (_localMove.id == 0)
		return;
	/* Skip Wayland button-up finalize. */
	if (_localMove.wayland)
		return;

	auto appWindow = getWindow(_localMove.id);
	if (!appWindow || !appWindow->window() || appWindow->isDeleted())
	{
		/* Window gone mid-drag: nothing to release into or report for. */
		_localMove.id = 0;
		return;
	}

	/* Finalize WM resize before reading final size. */
	(void)SDL_SyncWindow(appWindow->window());

	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);
	_localMove.id = 0;

	SDL_GetWindowPosition(appWindow->window(), &x, &y);
	const bool isMove = (_localMove.type == RAIL_WMSZ_MOVE);
	const SDL_Rect start = appWindow->outerRect(); /* position frozen at drag start */
	/* A plain move never changes size: report the server-owned size. The local readback is stale
	 * when the server drag-restored a maximized window mid-move (the WM keeps the grabbed frame at
	 * the maximized size); reporting that back as normal geometry poisons the server's
	 * maximize/restore state. */
	if (isMove)
	{
		/* Exception: a WM-imposed size during a plain move is a local snap/tile (or un-tile). No
		 * RDP command exists for it and the move loop only carries position, so adopt the WM size
		 * (the live readback lies - mutter re-asserts tile geometry when its grab ends) and resend
		 * the rect once the loop's END order confirms closure; sent earlier, the open loop
		 * swallows it, and a snapped server window also drag-restores itself at release. */
		if (!appWindow->effectivelyMaximized() && _localMove.wmSized)
		{
			/* WM size wins even after a mid-move server drag-restore (noteDragResize already
			 * filtered the restore's own reconcile echo): restore-then-tile keeps the tile. */
			w = _localMove.wmSize.x;
			h = _localMove.wmSize.y;
			appWindow->deferSnap(appWindow->serverRect({ x, y, w, h }));
		}
		else
		{
			w = start.w;
			h = start.h;
		}
	}
	_localMove.wmSized = false;
	/* Synthetic button-up closes the server loop and translates the final window delta into a
	 * pointer delta: no post-drop shift, honors WM snaps, avoids phantom resizes on click. A drag
	 * that ended maximized releases at the anchor (zero delta) so the deferred SC_MAXIMIZE stands
	 * and a moved release does not drag-restore the fresh maximize. */
	if (_localMove.serverEnded)
	{
		/* Loop closed early: button-up would be a phantom click. ClientWindowMove reports geometry.
		 */
		_localMove.serverEnded = false;
	}
	else
	{
		int px = _localMove.anchor.x;
		int py = _localMove.anchor.y;
		if (isMove && appWindow->localMoveSizeChanged())
		{
			/* Server drag-restored mid-move: measure remaining delta from the server's new anchor
			 * and frozen cursor, not the local drag-start. */
			const SDL_Point sp = appWindow->localMoveServerPos();
			const SDL_Rect ii = appWindow->insets();
			px = _localMove.pointer.x + (x + ii.x) - sp.x;
			py = _localMove.pointer.y + (y + ii.y) - sp.y;
		}
		else if (!appWindow->effectivelyMaximized())
		{
			const RailEdges e = railEdges(_localMove.type);
			if (isMove || e.left)
				px += x - start.x;
			else if (e.right)
				px += (x + w) - (start.x + start.w);
			if (isMove || e.top)
				py += y - start.y;
			else if (e.bottom)
				py += (y + h) - (start.y + start.h);
		}
		(void)freerdp_client_send_button_event(_context->common(), FALSE, PTR_FLAGS_BUTTON1, px,
		                                       py);
		/* Server loop unwinding: wait for explicit END order to gate deferred SC_MAXIMIZE. */
		appWindow->armLoopEnd();
	}

	(void)sdl_x11_set_bit_gravity(appWindow->window(), 0 /* forget: back to the default */);
	reportAndAdopt(appWindow, x, y, w, h);
}

UINT SdlRail::server_min_max_info(RailClientContext* context,
                                  const RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(minMaxInfo);
	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);

	std::unique_lock lock(rail->_windowsLock);
	auto appWindow = rail->getWindow(minMaxInfo->windowId);
	if (appWindow)
	{
		WLog_VRB(TAG, "server minmax id=0x%08" PRIx32 " min=%dx%d max=%dx%d", minMaxInfo->windowId,
		         minMaxInfo->minTrackWidth, minMaxInfo->minTrackHeight, minMaxInfo->maxTrackWidth,
		         minMaxInfo->maxTrackHeight);
		appWindow->setMinMaxSize({ minMaxInfo->minTrackWidth, minMaxInfo->minTrackHeight },
		                         { minMaxInfo->maxTrackWidth, minMaxInfo->maxTrackHeight });
		lock.unlock();
		(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
	}
	return CHANNEL_RC_OK;
}

/* --- window order callbacks --- */

/* Non-zero edge means margins exist; all-zero means "no band". */
static bool marginsSet(const SDL_Rect& m)
{
	return m.x || m.y || m.w || m.h;
}

BOOL SdlRail::window_common(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                            const WINDOW_STATE_ORDER* windowState)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(windowState);
	auto rail = SdlRail::get(context);
	if (!rail)
		return FALSE;

	const UINT32 fieldFlags = orderInfo->fieldFlags;
	std::unique_lock lock(rail->_windowsLock);
	auto appWindow = rail->getWindow(orderInfo->windowId);

	if (fieldFlags & WINDOW_ORDER_STATE_NEW)
	{
		const SDL_Rect rect = { static_cast<int>(windowState->windowOffsetX),
			                    static_cast<int>(windowState->windowOffsetY),
			                    static_cast<int>(windowState->windowWidth),
			                    static_cast<int>(windowState->windowHeight) };
		appWindow = rail->addWindow(orderInfo->windowId, rect);
		if (!appWindow)
			return FALSE;

		WLog_DBG(TAG,
		         "window create id=0x%08" PRIx32 " %dx%d+%d+%d style=0x%08" PRIx32
		         " ex=0x%08" PRIx32 " owner=0x%08" PRIx32,
		         orderInfo->windowId, rect.w, rect.h, rect.x, rect.y, windowState->style,
		         windowState->extendedStyle, windowState->ownerWindowId);
		/* Seed BAND from session margins (initial sync lacks them). CWM frame margins NOT seeded:
		 * trusting session margins would inflate custom-frame windows on every move. */
		const SDL_Rect& sm = rail->_sessionMargins;
		if (marginsSet(sm))
			appWindow->setResizeMargins(sm.x, sm.y, sm.w, sm.h);
		/* The SDL window is created lazily on the main thread (reconcile), as SDL requires. */
	}

	if (!appWindow)
		return FALSE;

	if (fieldFlags & (WINDOW_ORDER_FIELD_WND_OFFSET | WINDOW_ORDER_FIELD_WND_SIZE))
	{
		SDL_Rect r = appWindow->windowRect();
		if (fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
		{
			r.x = static_cast<int>(windowState->windowOffsetX);
			r.y = static_cast<int>(windowState->windowOffsetY);
		}
		if (fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
		{
			r.w = static_cast<int>(windowState->windowWidth);
			r.h = static_cast<int>(windowState->windowHeight);
		}
		WLog_VRB(TAG, "server geom id=0x%08" PRIx32 " rect=%d,%d %dx%d", orderInfo->windowId, r.x,
		         r.y, r.w, r.h);
		appWindow->updateWindowRect(r);
	}
	if (fieldFlags & WINDOW_ORDER_FIELD_OWNER)
		appWindow->setOwner(windowState->ownerWindowId);
	if (fieldFlags & (WINDOW_ORDER_FIELD_RESIZE_MARGIN_X | WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y))
	{
		SDL_Rect m = appWindow->frameMargins(); /* raw server margins as the base */
		if (fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_X)
		{
			m.x = static_cast<int>(windowState->resizeMarginLeft);
			m.w = static_cast<int>(windowState->resizeMarginRight);
		}
		if (fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y)
		{
			m.y = static_cast<int>(windowState->resizeMarginTop);
			m.h = static_cast<int>(windowState->resizeMarginBottom);
		}
		/* Trust window's OWN frame margins verbatim. Session margin is only a BAND fallback:
		 * inflating a zero-announcing window by borrowed margins would grow it on every move. */
		appWindow->setFrameMargins(m);
		SDL_Rect& sm = rail->_sessionMargins;
		if (marginsSet(m) && !marginsSet(sm))
		{
			sm = m;
			/* Backfill the BAND of earlier zero-announce windows (frame margins stay their own).
			 * bandMargins() floors + gates on resizability at read time. */
			for (auto& [id, w] : rail->_windows)
				if (!marginsSet(w.resizeMargins()))
					w.setResizeMargins(m.x, m.y, m.w, m.h);
		}
		const SDL_Rect bandSrc = marginsSet(m) ? m : sm;
		appWindow->setResizeMargins(bandSrc.x, bandSrc.y, bandSrc.w, bandSrc.h);
		WLog_INFO(TAG, "margins id=0x%08" PRIx32 " raw L%d T%d R%d B%d", orderInfo->windowId, m.x,
		          m.y, m.w, m.h);
	}
	if (fieldFlags & WINDOW_ORDER_FIELD_STYLE)
		appWindow->setStyle(windowState->style, windowState->extendedStyle);
	if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
		appWindow->setTitle(reinterpret_cast<const char16_t*>(windowState->titleInfo.string),
		                    windowState->titleInfo.length);

	if (fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		WLog_DBG(TAG, "server showState id=0x%08" PRIx32 " state=0x%02" PRIx32, orderInfo->windowId,
		         windowState->showState);
		appWindow->setVisible(windowState->showState != WINDOW_HIDE);
		/* Mirror server show-state locally. */
		switch (windowState->showState)
		{
			case WINDOW_SHOW_MAXIMIZED:
				appWindow->setServerMaximized(true);
				appWindow->setServerMinimized(false);
				break;
			case WINDOW_SHOW_MINIMIZED:
				appWindow->setServerMinimized(true);
				break;
			case WINDOW_SHOW:
				appWindow->setServerMaximized(false);
				appWindow->setServerMinimized(false);
				break;
			default:
				break;
		}
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		WLog_DBG(TAG, "visoff id=0x%08" PRIx32 " %d,%d", orderInfo->windowId,
		         windowState->visibleOffsetX, windowState->visibleOffsetY);
		appWindow->setVisibleOffset({ static_cast<int>(windowState->visibleOffsetX),
		                              static_cast<int>(windowState->visibleOffsetY) });
	}
	if (fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		std::vector<SDL_Rect> rects;
		rects.reserve(windowState->numVisibilityRects);
		for (UINT32 i = 0; i < windowState->numVisibilityRects; i++)
		{
			const RECTANGLE_16& r = windowState->visibilityRects[i];
			rects.push_back({ r.left, r.top, r.right - r.left, r.bottom - r.top });
		}
		if (rects.empty())
			WLog_INFO(TAG, "visrects id=0x%08" PRIx32 " n=0", orderInfo->windowId);
		else
			WLog_INFO(TAG, "visrects id=0x%08" PRIx32 " n=%zu first=%dx%d+%d+%d",
			          orderInfo->windowId, rects.size(), rects[0].w, rects[0].h, rects[0].x,
			          rects[0].y);
		appWindow->setVisibilityRects(std::move(rects));
	}

	/* Wake the main thread to create/move/show/paint the SDL window(s). */
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
	return TRUE;
}

SdlRailIcon* SdlRail::iconCacheLookup(uint32_t cacheId, uint32_t cacheEntry)
{
	if (cacheId == 0xFF)
		return &_iconScratch;
	const size_t idx = static_cast<size_t>(cacheId) * _iconCacheEntries + cacheEntry;
	if ((_iconCacheEntries == 0) || (cacheEntry >= _iconCacheEntries) || (idx >= _iconCache.size()))
		return nullptr;
	return &_iconCache[idx];
}

/* ICON_INFO (1/4/8/16/24/32 bpp + AND mask) -> BGRA32, like xf convert_rail_icon. */
static bool convertRailIcon(const ICON_INFO* info, SdlRailIcon& icon)
{
	icon.w = info->width;
	icon.h = info->height;
	icon.bgra.assign(4ULL * info->width * info->height, 0);
	return freerdp_image_copy_from_icon_data(
	           icon.bgra.data(), PIXEL_FORMAT_BGRA32, 0, 0, 0,
	           WINPR_ASSERTING_INT_CAST(UINT16, info->width),
	           WINPR_ASSERTING_INT_CAST(UINT16, info->height), info->bitsColor,
	           WINPR_ASSERTING_INT_CAST(UINT16, info->cbBitsColor), info->bitsMask,
	           WINPR_ASSERTING_INT_CAST(UINT16, info->cbBitsMask), info->colorTable,
	           WINPR_ASSERTING_INT_CAST(UINT16, info->cbColorTable), info->bpp) == TRUE;
}

BOOL SdlRail::window_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                          const WINDOW_ICON_ORDER* windowIcon)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(windowIcon);
	WINPR_ASSERT(windowIcon->iconInfo);
	auto rail = SdlRail::get(context);
	if (!rail)
		return FALSE;

	std::unique_lock lock(rail->_windowsLock);
	auto appWindow = rail->getWindow(orderInfo->windowId);
	if (!appWindow)
		return TRUE;

	/* Decode into the cache slot so a later WindowCachedIcon can reference it. */
	const ICON_INFO* info = windowIcon->iconInfo;
	auto* icon = rail->iconCacheLookup(info->cacheId, info->cacheEntry);
	if (!icon || !convertRailIcon(info, *icon))
	{
		WLog_WARN(TAG, "failed to decode icon %02" PRIX32 ":%04" PRIX32 " for window 0x%08" PRIx32,
		          info->cacheId, info->cacheEntry, orderInfo->windowId);
		return TRUE;
	}
	appWindow->setIcon(*icon);
	lock.unlock();
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
	return TRUE;
}

BOOL SdlRail::window_cached_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                 const WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(windowCachedIcon);
	auto rail = SdlRail::get(context);
	if (!rail)
		return FALSE;

	std::unique_lock lock(rail->_windowsLock);
	auto appWindow = rail->getWindow(orderInfo->windowId);
	if (!appWindow)
		return TRUE;

	const CACHED_ICON_INFO& cached = windowCachedIcon->cachedIcon;
	auto* icon = rail->iconCacheLookup(cached.cacheId, cached.cacheEntry);
	if (!icon || icon->bgra.empty())
	{
		WLog_WARN(TAG,
		          "cached icon %02" PRIX32 ":%04" PRIX32 " not in cache (window 0x%08" PRIx32 ")",
		          cached.cacheId, cached.cacheEntry, orderInfo->windowId);
		return TRUE;
	}
	appWindow->setIcon(*icon);
	lock.unlock();
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
	return TRUE;
}

BOOL SdlRail::window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	WINPR_ASSERT(orderInfo);
	auto rail = SdlRail::get(context);
	if (!rail)
		return FALSE;

	/* Mark only; erased on the main thread (paint) so the SDL window dies there. */
	std::unique_lock lock(rail->_windowsLock);
	auto appWindow = rail->getWindow(orderInfo->windowId);
	if (appWindow)
	{
		WLog_DBG(TAG, "window delete id=0x%08" PRIx32, orderInfo->windowId);
		appWindow->markDeleted();
	}

	/* Windows reuses window ids, so any state keyed by this id would be inherited by a future
	 * window: a stale _clientActiveId makes ensureActive skip its ClientActivate (new window
	 * unresponsive); a stale _localMove.id runs completeLocalMoveIfPending on the wrong window; a
	 * stale gate never sees its END order. Clear all of them for the deleted id. (The per-window
	 * loop-end state lives on the window object, so it dies with it - no stale-id inheritance.) */
	if (rail->_localMove.id == orderInfo->windowId)
		rail->_localMove = {};
	if (rail->_clientActiveId == orderInfo->windowId)
		rail->_clientActiveId = 0;
	if (rail->_focusedAppId == orderInfo->windowId)
		rail->_focusedAppId = 0;
	lock.unlock();

	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
	return TRUE;
}

BOOL SdlRail::monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(monitoredDesktop);
	auto rail = SdlRail::get(context);
	if (!rail)
		return FALSE;

	/* Launch the RemoteApp on DESKTOP_ARC_COMPLETED, like xf_rail_monitored_desktop. */
	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ARC_COMPLETED)
	{
		rail->enableRemoteAppMode(true);
		const char* app =
		    freerdp_settings_get_string(context->settings, FreeRDP_RemoteApplicationProgram);
		if (app && (strnlen(app, 1) > 0))
		{
			WLog_DBG(TAG, "RAIL mode enabled (monitored desktop); launching '%s'", app);
			if (client_rail_server_start_cmd(rail->_rail) != CHANNEL_RC_OK)
			{
				WLog_ERR(TAG, "client_rail_server_start_cmd failed for '%s'", app);
				return FALSE;
			}
		}
	}

	/* Authoritative top-level z-order (windowIds[0] topmost); capture it, paint() realizes it. */
	if (orderInfo->fieldFlags &
	    (WINDOW_ORDER_FIELD_DESKTOP_ZORDER | WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND))
	{
		std::unique_lock lock(rail->_windowsLock);
		if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND)
		{
			/* Server changed active window: sync our ClientActivate dedup to avoid skipping
			 * re-clicks. */
			rail->_clientActiveId = monitoredDesktop->activeWindowId;
		}
		if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
		{
			if (monitoredDesktop->windowIds && (monitoredDesktop->numWindowIds > 0))
				rail->_zOrder.assign(monitoredDesktop->windowIds,
				                     monitoredDesktop->windowIds + monitoredDesktop->numWindowIds);
			else
				rail->_zOrder.clear();
			rail->_zOrderDirty = true;
			lock.unlock();
			(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE); /* wake the main thread to restack */
		}
	}
	return TRUE;
}

BOOL SdlRail::non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	WINPR_ASSERT(orderInfo);
	auto rail = SdlRail::get(context);
	if (rail)
	{
		WLog_DBG(TAG, "RAIL mode disabled (non-monitored desktop)");
		rail->enableRemoteAppMode(false);
	}
	return TRUE;
}
