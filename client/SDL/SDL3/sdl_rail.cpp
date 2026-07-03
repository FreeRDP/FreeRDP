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
	if (!_rail || !_rail->ClientActivate)
		return;
	WLog_VRB(TAG, "activate id=0x%08" PRIx32 " gained=%d", static_cast<UINT32>(appWindow->id()),
	         gained ? 1 : 0);
	RAIL_ACTIVATE_ORDER activate = {};
	activate.windowId = static_cast<UINT32>(appWindow->id());
	activate.enabled = gained;
	std::ignore = _rail->ClientActivate(_rail, &activate);
}

bool SdlRail::translateToServer(SDL_WindowID id, float& x, float& y)
{
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow)
		return false;

	/* Window-local -> server-absolute (add the window's visible on-screen origin). */
	SDL_FPoint rpos = { x, y };
	if (auto* renderer = appWindow->renderer())
		(void)SDL_RenderCoordinatesFromWindow(renderer, x, y, &rpos.x, &rpos.y);

	const auto& rect = appWindow->windowRect();
	x = rpos.x + static_cast<float>(rect.x);
	y = rpos.y + static_cast<float>(rect.y);
	return true;
}

SdlRailWindow* SdlRail::addWindow(uint64_t id, const SDL_Rect& rect)
{
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

	std::unique_lock lock(_windowsLock);

	/* Erase RDP-thread-deleted entries here so SDL windows die on the main thread. */
	for (auto it = _windows.begin(); it != _windows.end();)
	{
		if (it->second.isDeleted())
			it = _windows.erase(it);
		else
			++it;
	}

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
			parentRect = owner->windowRect();
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
			parentRect = chosen->windowRect();
		}
		else
			WLog_WARN(TAG, "popup id=0x%08" PRIx32 " has no parent app window",
			          static_cast<UINT32>(popup.id()));
		popup.paint(primary, fallbackFormat, damage, parent, parentRect);
	}
	return true;
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
		_windows.clear();
	}

	rail->custom = this;
	rail->ServerExecuteResult = SdlRail::server_execute_result;
	/* ServerSystemParam: TODO apply the workarea (SPI_SET_WORK_AREA) to maximize bounds. */
	rail->ServerLocalMoveSize = SdlRail::server_local_move_size;
	rail->ServerMinMaxInfo = SdlRail::server_min_max_info;
	/* Keep default ServerHandshake. */

	const RailPlatformCaps& caps = railPlatformCaps();
	WLog_INFO(TAG, "RAIL channel initialized: driver=%s positionsReadable=%d transparentWindows=%d",
	          sdl::utils::isWaylandDriver() ? "wayland"
	                                        : (sdl::utils::isX11Driver() ? "x11" : "other"),
	          caps.positionsReadable ? 1 : 0, caps.supportsTransparentWindows ? 1 : 0);
	return true;
}

bool SdlRail::uninit(RailClientContext* /*rail*/)
{
	_refreshSent = false;
	_localMoveId = 0;
	std::unique_lock lock(_windowsLock);
	WLog_DBG(TAG, "RAIL channel uninit, destroying %zu windows", _windows.size());
	_windows.clear();
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
		/* Carry the target in the payload so back-to-back requests can't clobber it. */
		const UINT32 packedPos =
		    (static_cast<UINT32>(static_cast<UINT16>(localMoveSize->posX)) << 16) |
		    static_cast<UINT16>(localMoveSize->posY);
		WLog_DBG(TAG, "server move/size start id=0x%08" PRIx32 " type=%" PRIu16 " pos=%d,%d",
		         localMoveSize->windowId, localMoveSize->moveSizeType, localMoveSize->posX,
		         localMoveSize->posY);
		(void)sdl_push_user_event(SDL_EVENT_USER_RAIL_MOVE, localMoveSize->windowId, packedPos,
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
			/* Ignore server end order during Wayland resize. */
			const bool ownedByWaylandResize =
			    rail->_localMoveWayland && (rail->_localMoveId == localMoveSize->windowId);
			if (!ownedByWaylandResize)
			{
				if (auto* appWindow = rail->getWindow(localMoveSize->windowId))
					appWindow->setLocalMoveActive(false);
			}
		}
	}
	return CHANNEL_RC_OK;
}

/* X11 only: suppress button-up during modal loop. */
bool SdlRail::suppressServerInput(SDL_WindowID id)
{
	if (_localMoveId == 0)
		return false; /* fast path: no move pending, skip the lock on every motion event */
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	return appWindow && appWindow->localMoveActive() && !_localMoveWayland;
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

/* RAIL_WMSZ_* -> XDG_TOPLEVEL_RESIZE_EDGE_*. */
static uint32_t railToXdgEdge(uint16_t moveType)
{
	switch (moveType)
	{
		case RAIL_WMSZ_TOP:
			return 1;
		case RAIL_WMSZ_BOTTOM:
			return 2;
		case RAIL_WMSZ_LEFT:
			return 4;
		case RAIL_WMSZ_TOPLEFT:
			return 5;
		case RAIL_WMSZ_BOTTOMLEFT:
			return 6;
		case RAIL_WMSZ_RIGHT:
			return 8;
		case RAIL_WMSZ_TOPRIGHT:
			return 9;
		case RAIL_WMSZ_BOTTOMRIGHT:
			return 10;
		default:
			return 0; /* none */
	}
}

/* Which window edges a RAIL_WMSZ_* drag moves; the opposite edges stay anchored. */
struct RailEdges
{
	bool left, right, top, bottom;
};
static RailEdges railEdges(uint16_t t)
{
	return { (t == RAIL_WMSZ_TOPLEFT) || (t == RAIL_WMSZ_LEFT) || (t == RAIL_WMSZ_BOTTOMLEFT),
		     (t == RAIL_WMSZ_TOPRIGHT) || (t == RAIL_WMSZ_RIGHT) || (t == RAIL_WMSZ_BOTTOMRIGHT),
		     (t == RAIL_WMSZ_TOPLEFT) || (t == RAIL_WMSZ_TOP) || (t == RAIL_WMSZ_TOPRIGHT),
		     (t == RAIL_WMSZ_BOTTOMLEFT) || (t == RAIL_WMSZ_BOTTOM) ||
		         (t == RAIL_WMSZ_BOTTOMRIGHT) };
}

void SdlRail::handleLocalMoveRequested(uint32_t windowId, SDL_Point pos, uint16_t moveType)
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
			_localMoveId = windowId;
			_localMoveWayland = true;
			_localMoveType = moveType;
			/* Reset resize guard for new drag. */
			_localMoveSawResize = false;
		}
	}
	else
	{
		/* X11: hand resize to WM, suppress input. */
		started = sdl_x11_begin_move_size(appWindow->window(), railToNetDirection(moveType));
		if (started)
		{
			_localMoveId = windowId;
			_localMoveWayland = false;
			_localMoveType = moveType;
			const SDL_Rect rect = appWindow->windowRect();
			_localMoveGrabPos = { rect.x + pos.x, rect.y + pos.y };
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

/* Caller holds _windowsLock. Report the final rect to the server and adopt it locally. */
void SdlRail::reportAndAdopt(SdlRailWindow* appWindow, int x, int y, int w, int h)
{
	/* Always report, even if barely changed: the server may have diverged before input suppression
	 * engaged, and a no-op report is harmless. Add back the invisible resize margins (matches
	 * xf_rail_end_local_move). */
	const SDL_Rect m = appWindow->resizeMargins();
	RAIL_WINDOW_MOVE_ORDER move = {};
	move.windowId = static_cast<UINT32>(appWindow->id());
	move.left = WINPR_ASSERTING_INT_CAST(INT16, x - m.x);
	move.top = WINPR_ASSERTING_INT_CAST(INT16, y - m.y);
	move.right = WINPR_ASSERTING_INT_CAST(INT16, x + w + m.w);
	move.bottom = WINPR_ASSERTING_INT_CAST(INT16, y + h + m.h);
	if (_rail && _rail->ClientWindowMove &&
	    (_rail->ClientWindowMove(_rail, &move) != CHANNEL_RC_OK))
		WLog_WARN(TAG, "ClientWindowMove failed for RAIL window 0x%08" PRIx32, move.windowId);

	/* Adopt WM final geometry and resume input routing. */
	appWindow->adoptLocalGeometry({ x, y, w, h });
	/* Repaint now: the last presented frame may still be the resize placeholder. */
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
}

void SdlRail::noteWaylandResize()
{
	std::unique_lock lock(_windowsLock);
	if (_localMoveId && _localMoveWayland)
		_localMoveSawResize = true;
}

void SdlRail::completeWaylandResize()
{
	std::unique_lock lock(_windowsLock);
	if ((_localMoveId == 0) || !_localMoveWayland)
		return;
	/* Only a drag that actually resized ends here; a bare re-enter is the spurious/idle enter. */
	if (!_localMoveSawResize)
		return;

	auto appWindow = getWindow(_localMoveId);
	_localMoveId = 0;
	if (!appWindow || !appWindow->window())
		return;

	(void)SDL_SyncWindow(appWindow->window());
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);

	/* Derive Wayland origin from anchor edge. */
	const SDL_Rect start = appWindow->windowRect();
	const RailEdges e = railEdges(_localMoveType);
	auto* settings = _context->context()->settings;
	const int dw = static_cast<int>(freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth));
	const int dh = static_cast<int>(freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight));
	int x = e.left ? (start.x + start.w - w) : start.x;
	int y = e.top ? (start.y + start.h - h) : start.y;
	x = std::clamp(x, 0, std::max(0, dw - w));
	y = std::clamp(y, 0, std::max(0, dh - h));
	reportAndAdopt(appWindow, x, y, w, h);
}

void SdlRail::completeLocalMoveIfPending()
{
	if (_localMoveId == 0)
		return;

	std::unique_lock lock(_windowsLock);
	/* Skip Wayland button-up finalize. */
	if (_localMoveWayland)
		return;

	auto appWindow = getWindow(_localMoveId);
	if (!appWindow || !appWindow->window())
	{
		_localMoveId = 0;
		return;
	}

	/* Finalize WM resize before reading final size. */
	(void)SDL_SyncWindow(appWindow->window());

	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);
	_localMoveId = 0;

	SDL_GetWindowPosition(appWindow->window(), &x, &y);
	/* Synthetic button-up at final dragged corner. */
	const RailEdges e = railEdges(_localMoveType);
	const int px = e.left ? x : (e.right ? (x + w) : _localMoveGrabPos.x);
	const int py = e.top ? y : (e.bottom ? (y + h) : _localMoveGrabPos.y);
	(void)freerdp_client_send_button_event(_context->common(), FALSE, PTR_FLAGS_BUTTON1, px, py);

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
		SDL_Rect m = appWindow->resizeMargins();
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
		appWindow->setResizeMargins(m.x, m.y, m.w, m.h);
	}
	if (fieldFlags & WINDOW_ORDER_FIELD_STYLE)
		appWindow->setStyle(windowState->style, windowState->extendedStyle);
	if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
		appWindow->setTitle(reinterpret_cast<const char16_t*>(windowState->titleInfo.string),
		                    windowState->titleInfo.length);

	if (fieldFlags & WINDOW_ORDER_FIELD_SHOW)
		appWindow->setVisible(windowState->showState != WINDOW_HIDE);

	if (fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		std::vector<SDL_Rect> rects;
		rects.reserve(windowState->numVisibilityRects);
		for (UINT32 i = 0; i < windowState->numVisibilityRects; i++)
		{
			const RECTANGLE_16& r = windowState->visibilityRects[i];
			rects.push_back({ r.left, r.top, r.right - r.left, r.bottom - r.top });
		}
		appWindow->setVisibilityRects(std::move(rects));
	}

	/* Wake the main thread to create/move/show/paint the SDL window(s). */
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
