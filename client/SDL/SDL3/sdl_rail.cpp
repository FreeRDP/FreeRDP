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
#include <utility>
#include <vector>

#include <winpr/assert.h>
#include <winpr/platform.h>
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

#define TAG CLIENT_TAG("sdl.rail")

/* The compositor runs the drag (Wayland); the client only tracks the size. */
[[nodiscard]] static bool compositorDrag()
{
	return railPlatformCaps().drag == RailDragModel::CompositorResize;
}

SdlRail::SdlRail(SdlContext* context) : _context(context)
{
}

SdlRail::~SdlRail()
{
	railPlatformSetNativeLoopHandler({});
}

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
	if (id == 0)
		return nullptr;
	for (auto& it : _windows)
	{
		if (it.second.sdlId() == id)
			return &it.second;
	}
	return nullptr;
}

/* Test if popup adjoins window edge from outside. */
static bool adjoinsOutside(const SDL_Rect& popup, const SDL_Rect& win)
{
	SDL_Rect hit{};
	if (SDL_GetRectIntersection(&popup, &win, &hit))
		return false;
	const SDL_Rect grown = { win.x - 1, win.y - 1, win.w + 2, win.h + 2 };
	return SDL_GetRectIntersection(&popup, &grown, &hit);
}

/* Fullscreen popups (e.g. slide shows) act as toplevels and can parent popups. */
static bool canParentPopup(const SdlRailWindow& w)
{
	return (!w.isPopup() || w.isFullscreen()) && (w.window() != nullptr);
}

SdlRailWindow* SdlRail::resolveParent(uint64_t ownerId)
{
	auto* owner = getWindow(ownerId);
	if (owner && canParentPopup(*owner))
		return owner;
	return nullptr;
}

/* Pick a popup's parent app window: ownerWindowId -> geometric containment -> focused -> any app.
 */
SdlRailWindow* SdlRail::resolvePopupParent(const SdlRailWindow& popup)
{
	SdlRailWindow* chosen = resolveParent(popup.owner());
	if (!chosen)
	{
		const SDL_Rect pr = popup.windowRect();
		const SDL_Point origin = { pr.x, pr.y };
		for (uint32_t id : _zOrder)
		{
			auto* w = getWindow(id);
			if (!w || !canParentPopup(*w))
				continue;
			const SDL_Rect wr = w->windowRect();
			if (SDL_PointInRect(&origin, &wr))
			{
				chosen = w;
				break;
			}
		}
		if (!chosen)
		{
			for (auto& other : _windows)
			{
				auto& w = other.second;
				if (!canParentPopup(w))
					continue;
				const SDL_Rect wr = w.windowRect();
				if (SDL_PointInRect(&origin, &wr))
				{
					chosen = &w;
					break;
				}
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
			if (canParentPopup(w))
			{
				chosen = &w;
				break;
			}
		}
	}
	return chosen;
}

/* Identify companion shadow frames adjoining owner window. */
bool SdlRail::isShadowFrame(const SdlRailWindow& popup)
{
	if (!popup.isLayered())
		return false;
	const SDL_Rect pr = popup.windowRect();
	for (auto& other : _windows)
	{
		auto& w = other.second;
		if (canParentPopup(w) && adjoinsOutside(pr, w.windowRect()))
			return true;
	}
	return false;
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

void SdlRail::sendWorkArea(const SDL_Rect& local)
{
	/* Convert local display coordinates to server coordinates (relative to primary monitor). */
	const SDL_Rect area = { local.x - _displayShift.x, local.y - _displayShift.y, local.w,
		                    local.h };
	if (!_rail || !_rail->ClientSystemParam || (area.w <= 0) || (area.h <= 0))
		return;
	if ((area.x < 0) || (area.y < 0))
	{
		WLog_WARN(TAG, "work area %d,%d is outside the primary monitor, not sent", area.x, area.y);
		return;
	}
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
	/* Skip echo of programmatic maximize, but trigger reconcile to update insets. */
	if (appWindow->railMaximized())
	{
		WLog_DBG(TAG, "local maximize skipped id=0x%08" PRIx32 " (already rail-maximized)",
		         static_cast<UINT32>(appWindow->id()));
		lock.unlock();
		if (!sdl_push_user_event(SDL_EVENT_USER_UPDATE))
			WLog_WARN(TAG, "sdl_push_user_event failed");
		return;
	}
	appWindow->setRailMaximized(true);
	/* Force full repaint (skips dirty-rect). */
	appWindow->invalidateAll();
	/* Defer SC_MAXIMIZE if modal move loop is still open. */
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
	/* Resolve restored state; trigger reconcile in case WM cleared flags asynchronously. */
	if (appWindow->railMinimized())
	{
		appWindow->setRailMinimized(false);
		appWindow->noteLocalRestore();
	}
	else if (appWindow->railMaximized())
		appWindow->setRailMaximized(false);
	else
	{
		lock.unlock();
		if (!sdl_push_user_event(SDL_EVENT_USER_UPDATE))
			WLog_WARN(TAG, "sdl_push_user_event failed");
		return;
	}
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
	if (!appWindow || (appWindow->isPopup() && !appWindow->isFullscreen()) || !appWindow->window())
		return;

	/* Ignore focus loss while dragging: mapping restore shadow frames can steal focus
	 * and cancel the server modal loop. */
	if (!gained && (_localMove.id == static_cast<uint32_t>(appWindow->id())))
		return;

	/* Fallback parent for orphaned popups. */
	if (gained)
		_focusedAppId = appWindow->id();

	/* Only report activation, never deactivation (see the X11 client, #13422).
	 * The server treats a deactivation of the owner as "no active window in
	 * this application" and closes popups/menus that have just been opened
	 * when the compositor moves focus to them. The activation that follows
	 * (or an explicit click, see ensureActive()) is enough to route input. */
	if (!gained)
		return;

	/* A window owned by this one is currently active (e.g. a profile card or
	 * a transient dialog). On a real desktop the owner is only activated by a
	 * click; focus-follows-mouse compositors hand focus back on hover, which
	 * would make the server close the owned popup. Leave that to
	 * ensureActive() (explicit click). */
	const auto* active = getWindow(_clientActiveId);
	if (active && (active != appWindow) &&
	    (active->owner() == static_cast<uint32_t>(appWindow->id())))
		return;

	/* ClientActivate only. Do NOT SDL_RaiseWindow to avoid WM focus loops. */
	const auto wid = static_cast<uint32_t>(appWindow->id());
	sendClientActivate(wid, true);
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
	if (!appWindow || (appWindow->isPopup() && !appWindow->isFullscreen()) || !appWindow->window())
		return;
	const auto wid = static_cast<uint32_t>(appWindow->id());
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
	/* Window-local to server-absolute, aligned with surface blit anchor. */
	SDL_Point origin = appWindow->serverOrigin();
	SDL_Point rel{};
	if (appWindow->isPopup() && !railPlatformCaps().supportsWindowPositions &&
	    appWindow->placedRel(rel))
	{
		auto* sdlParent = SDL_GetWindowParent(appWindow->window());
		if (auto* parent = sdlParent ? getWindowBySdlId(SDL_GetWindowID(sdlParent)) : nullptr)
		{
			const SDL_Rect pr = parent->outerRect();
			origin = { pr.x + rel.x, pr.y + rel.y };
		}
	}
	/* Input is relative to the top left of the remote desktop, not to the primary monitor. */
	x = rpos.x + static_cast<float>(origin.x - _inputOrigin.x);
	y = rpos.y + static_cast<float>(origin.y - _inputOrigin.y);
	return true;
}

void SdlRail::notePopupMoved(SDL_WindowID id)
{
	/* X11 input already matches the server rect; only Wayland lags behind it. */
	if (railPlatformCaps().supportsWindowPositions)
		return;
	std::unique_lock lock(_windowsLock);
	auto* w = getWindowBySdlId(id);
	if (!w || !w->isPopup())
		return;
	w->notePlaced();
	/* Show the held frame now: that commit applies the move. */
	if (!sdl_push_user_event(SDL_EVENT_USER_UPDATE))
		WLog_WARN(TAG, "could not wake the paint loop for a moved popup");
}

SdlRailWindow* SdlRail::addWindow(uint64_t id, const SDL_Rect& rect)
{
	/* Move deleted entry aside so new window can reuse the ID. */
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
	res.first->second.setLocalShift(_localShift);
	return &res.first->second;
}

/* Caller holds _windowsLock or runs before any window exists. */
void SdlRail::updateCoordinates()
{
	const auto* settings = _context->common()->context.settings;
	_displayShift = { freerdp_settings_get_int32(settings, FreeRDP_MonitorLocalShiftX),
		              freerdp_settings_get_int32(settings, FreeRDP_MonitorLocalShiftY) };
	_localShift = railPlatformCaps().supportsWindowPositions ? _displayShift : SDL_Point{ 0, 0 };
	_inputOrigin = { 0, 0 };
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	for (UINT32 x = 0; x < count; x++)
	{
		const auto* mon = static_cast<const rdpMonitor*>(
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x));
		if (!mon)
			continue;
		_inputOrigin.x = std::min(_inputOrigin.x, mon->x);
		_inputOrigin.y = std::min(_inputOrigin.y, mon->y);
	}
	WLog_DBG(TAG, "coordinates: display shift %d,%d window shift %d,%d input origin %d,%d",
	         _displayShift.x, _displayShift.y, _localShift.x, _localShift.y, _inputOrigin.x,
	         _inputOrigin.y);
}

void SdlRail::enableRemoteAppMode(bool enable)
{
	_enabled = enable;
	if (enable)
		_desktopReleased = true;
}

/* Show the desktop anyway after this: not every logon prompt is announced. */
static constexpr Uint32 kDesktopHoldMs = 10000;

bool SdlRail::desktopMayShow()
{
	if (_desktopReleased)
		return true;
	const uint64_t now = SDL_GetTicks();
	if (_desktopHoldUntil == 0)
	{
		_desktopHoldUntil = now + kDesktopHoldMs;
		/* Frames may stop coming while the logon waits; wake up to re-check. */
		if (!sdl_push_update_after(kDesktopHoldMs + 1))
			WLog_WARN(TAG, "SDL_AddTimer failed: %s", SDL_GetError());
		WLog_DBG(TAG, "RemoteApp starting: holding the logon desktop back");
		return false;
	}
	if (now < _desktopHoldUntil)
		return false;
	WLog_DBG(TAG, "RemoteApp not started after %" PRIu32 " ms, showing the desktop",
	         kDesktopHoldMs);
	_desktopReleased = true;
	return true;
}

void SdlRail::noteLogonPrompt()
{
	if (_desktopReleased.exchange(true))
		return;
	WLog_DBG(TAG, "logon dialog on the server, showing the desktop");
	if (!sdl_push_user_event(SDL_EVENT_USER_UPDATE))
		WLog_WARN(TAG, "sdl_push_user_event failed");
}

bool SdlRail::paint(SDL_Surface* primary, SDL_PixelFormat fallbackFormat,
                    const std::vector<SDL_Rect>& damage)
{
	if (!_enabled)
		return true;

	std::unique_lock lock(_windowsLock);

	/* Report workarea once (avoids maximizing under local panels). */
	if (_sentWorkArea.w == 0)
	{
		SDL_Rect usable{};
		if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
			sendWorkArea(usable);
	}

	/* Windows whose SDL parent is going away (deleted by the server, or displaced by a same-id
	 * recreate, see addWindow). Popups owned by that window die with it (the server deletes
	 * them too), and so do non-popup windows, as before. Any other popup keeps its entry: the
	 * server still owns it. A popup without an owner (e.g. a tray notification) is parented to
	 * the focused app window as a fallback; deleting it locally when that window closes
	 * desynchronizes us from the server, which keeps sending orders for it. It only drops its
	 * local SDL window here and the next pass re-creates it under a new parent.
	 * SDL destroys child windows together with their parent, so every window that is about to
	 * lose its SDL window (deleted, displaced, or under one of those) releases it here, deepest
	 * child first, before any entry is destroyed below. */
	std::vector<std::pair<size_t, SdlRailWindow*>> release;
	for (auto& [wid, win] : _windows)
	{
		if (!win.window())
			continue;
		size_t depth = 0;
		bool orphan = false;
		bool ownerDeleted = false;
		for (SDL_Window* p = SDL_GetWindowParent(win.window()); p; p = SDL_GetWindowParent(p))
		{
			depth++;
			auto* owner = getWindowBySdlId(SDL_GetWindowID(p));
			if (!owner)
				orphan = true;
			else if (owner->isDeleted())
			{
				orphan = true;
				if (owner->id() == win.owner())
					ownerDeleted = true;
			}
		}
		if (orphan && !win.isDeleted() && (ownerDeleted || !win.isPopup()))
			win.markDeleted();
		if (orphan || win.isDeleted())
			release.emplace_back(depth, &win);
	}
	for (auto& [wid, win] : _deadWindows)
	{
		if (!win.window())
			continue;
		size_t depth = 0;
		for (SDL_Window* p = SDL_GetWindowParent(win.window()); p; p = SDL_GetWindowParent(p))
			depth++;
		release.emplace_back(depth, &win);
	}
	std::sort(release.begin(), release.end(),
	          [](const auto& a, const auto& b) { return a.first > b.first; });
	for (auto& [depth, win] : release)
		win->releaseWindow();

	/* Erase deleted windows; their SDL windows are gone already. */
	for (auto it = _windows.begin(); it != _windows.end();)
		it = it->second.isDeleted() ? _windows.erase(it) : std::next(it);
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
		/* Adopt WM-refused geometry and update server. */
		SDL_Rect refusedOuter{};
		if (win.takeWmOverride(refusedOuter))
		{
			const SDL_Rect rect = win.serverRect(refusedOuter);
			const SDL_Rect cur = win.windowRect();
			WLog_DBG(TAG,
			         "wm-refused adopt id=0x%08" PRIx32
			         " outer=%dx%d -> srv=%d,%d %dx%d (was %dx%d)",
			         static_cast<uint32_t>(win.id()), refusedOuter.w, refusedOuter.h, rect.x,
			         rect.y, rect.w, rect.h, cur.w, cur.h);
			if (!SDL_RectsEqual(&rect, &cur))
				sendClientWindowMove(&win, rect);
			win.adoptLocalGeometry(rect);
		}
		/* Resend a size the server left unanswered. */
		SDL_Rect unconfirmed{};
		if (win.takeUnconfirmedReport(unconfirmed) && !win.railMinimized() &&
		    !win.effectivelyMaximized())
		{
			WLog_DBG(TAG, "size resent id=0x%08" PRIx32 " %dx%d (not confirmed)",
			         static_cast<uint32_t>(win.id()), unconfirmed.w, unconfirmed.h);
			sendClientWindowMove(&win, unconfirmed);
		}
		if (win.takeRestoreRetry())
		{
			WLog_DBG(TAG, "restore resent id=0x%08" PRIx32, static_cast<uint32_t>(win.id()));
			sendSystemCommand(&win, SC_RESTORE);
		}
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

		/* Latch shadow frame classification once identified. */
		if (!popup.isFrame() && isShadowFrame(popup))
			popup.setFrame(true);

		/* Anchor drop shadow to adjoining visible popup. */
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
				/* Shadow must be contained within reach zone to distinguish popup shadows from
				 * app frame edge shadows. */
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

		SdlRailWindow* chosen = nullptr;
		/* Existing popups have an immutable SDL parent; position against it. */
		if (auto* pw = popup.window())
		{
			SDL_Window* p = SDL_GetWindowParent(pw);
			chosen = p ? getWindowBySdlId(SDL_GetWindowID(p)) : nullptr;
		}
		else
		{
			chosen = resolvePopupParent(popup);
			if (!chosen)
				WLog_DBG(TAG, "popup id=0x%08" PRIx32 " has no parent app window",
				         static_cast<UINT32>(popup.id()));
		}

		SDL_Window* parent = nullptr;
		SDL_Rect parentRect{};
		if (chosen)
		{
			parent = chosen->window();
			parentRect = chosen->outerRect(); /* the SDL window's on-screen geometry */
		}
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
	/* Only backends with a focus-neutral restack (supportsRestack). */
	if (!railPlatformCaps().supportsRestack)
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
	/* Nor while Windows resizes one on its native frame. */
	if (_nativeResizeId != 0)
	{
		const auto* resizing = getWindowBySdlId(_nativeResizeId);
		if (resizing && !resizing->isDeleted())
			return;
		_nativeResizeId = 0; /* destroyed mid-resize: no end callback comes */
	}
	/* Restack top-level windows (skip popups and hidden windows). */
	std::vector<SDL_Window*> stack;
	std::vector<uint32_t> applied;
	stack.reserve(_zOrder.size());
	applied.reserve(_zOrder.size());
	for (uint32_t id : _zOrder)
	{
		auto* w = getWindow(id);
		if (w && !w->isPopup() && w->window() &&
		    ((SDL_GetWindowFlags(w->window()) & SDL_WINDOW_HIDDEN) == 0))
		{
			stack.push_back(w->window());
			applied.push_back(id);
		}
	}
	/* Dedup against the restacked set; menus and unrealized windows are filtered out. */
	if (applied == _appliedZOrder)
	{
		_zOrderDirty = false;
		return;
	}
	if (stack.size() >= 2)
	{
		std::ignore = railPlatformRestack(stack);
	}

	_appliedZOrder = std::move(applied);
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
		WLog_VRB(TAG, "gfx surface for untracked id=0x%08" PRIx64, surface->windowId);
		return CHANNEL_RC_OK;
	}

	/* Clamp mapped dimensions to allocated surface bounds. */
	const uint32_t w =
	    std::min(surface->mappedWidth ? surface->mappedWidth : surface->width, surface->width);
	const uint32_t h =
	    std::min(surface->mappedHeight ? surface->mappedHeight : surface->height, surface->height);

	/* Consume per-frame damage. */
	UINT32 nbRects = 0;
	const RECTANGLE_16* rects = region16_rects(&surface->invalidRegion, &nbRects);
	/* Skip undamaged surfaces during EndFrame. */
	const bool remapped = appWindow->takeSurfaceChange(surface->surfaceId);
	if ((nbRects == 0) && !remapped)
		return CHANNEL_RC_OK;
	appWindow->updateGfxSurface(surface->data, surface->scanline, w, h, rects, nbRects,
	                            surface->format);
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
		/* Move previous windows to deadWindows for main-thread teardown. */
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
	{
		std::unique_lock lock(_windowsLock);
		updateCoordinates();
	}
	const RailPlatformCaps& caps = railPlatformCaps();
	railPlatformSetNativeLoopHandler({ this, redrawDuringSizeMove, nativeResize });
	WLog_DBG(TAG, "RAIL channel initialized: driver=%s windowPositions=%d transparentWindows=%d",
	         SDL_GetCurrentVideoDriver(), caps.supportsWindowPositions ? 1 : 0,
	         caps.supportsTransparentWindows ? 1 : 0);
#if SDL_VERSION_ATLEAST(3, 4, 10)
	/* Synchronize WM-driven X11 resizes via _NET_WM_SYNC_REQUEST to prevent torn frames. */
	if (sdl::utils::isX11Driver())
		SDL_SetHint(SDL_HINT_VIDEO_X11_ENABLE_XSYNC_EXT, "1");
#endif
	/* Like Windows, the click that activates a window also reaches the app. */
	if (!SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1"))
		WLog_WARN(TAG, "SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH) failed: %s", SDL_GetError());
	return true;
}

bool SdlRail::uninit(WINPR_ATTR_UNUSED RailClientContext* rail)
{
	WINPR_ASSERT(rail);
	/* The caller clears rail->custom next; server callbacks return early on nullptr. */
	std::unique_lock lock(_windowsLock);
	_refreshSent = false;
	_appStarted = false;
	/* Reset session and move state on reconnect. */
	_localMove = {};
	_sessionMargins = {};
	_sentWorkArea = {};
	_clientActiveId = 0;
	_focusedAppId = 0;
	_zOrder.clear();
	_appliedZOrder.clear();
	_zOrderDirty = false;
	_iconCache.clear();
	_iconScratch = {};
	/* Mark windows for main-thread teardown. */
	WLog_DBG(TAG, "RAIL channel uninit, marking %zu windows for main-thread teardown",
	         _windows.size());
	for (auto& kv : _windows)
		kv.second.markDeleted();
	_rail = nullptr;
	_enabled = false;
	/* The next connection logs on again: hold its logon desktop back too. */
	_desktopReleased = false;
	_desktopHoldUntil = 0;
	lock.unlock();
	(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
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
	if (!rail)
		return CHANNEL_RC_OK; /* late order after disconnect */

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
		if (auto rail = static_cast<SdlRail*>(context->custom))
		{
			std::unique_lock lock(rail->_windowsLock);
			rail->_moveLoop = { localMoveSize->windowId, 0 };
		}
		(void)sdl_push_user_event(SDL_EVENT_USER_RAIL_MOVE, localMoveSize->windowId,
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
			auto* appWindow = rail->getWindow(localMoveSize->windowId);
			/* Ignore server END order during Wayland resize unless unstarted. */
			const bool ownedByWaylandResize = compositorDrag() &&
			                                  (rail->_localMove.id == localMoveSize->windowId) &&
			                                  rail->_localMove.sawResize;
			if (!ownedByWaylandResize)
			{
				if (appWindow)
					appWindow->setLocalMoveActive(false);
				if (rail->_localMove.id == localMoveSize->windowId)
				{
					if (compositorDrag())
					{
						/* Wayland drag completed. */
						rail->_localMove = {};
						if (appWindow)
							appWindow->invalidateAll();
						(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
					}
					else
					{
						/* Server ended drag loop early. */
						rail->_localMove.serverEnded = true;
					}
				}
			}
			if (rail->_moveLoop.id == localMoveSize->windowId)
			{
				const SDL_DisplayID display = rail->_moveLoop.display;
				rail->_moveLoop = {};
				auto* moved = rail->getWindow(localMoveSize->windowId);
				if (moved && (display != 0) && !moved->effectivelyMaximized())
					rail->moveToDisplay(moved, display);
			}
			/* Execute deferred actions now that modal loop has closed. */
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
					/* Send deferred snap resize. */
					WLog_DBG(TAG, "deferred snap resize id=0x%08" PRIx32 " rect=%d,%d %dx%d",
					         localMoveSize->windowId, actions.snapRect.x, actions.snapRect.y,
					         actions.snapRect.w, actions.snapRect.h);
					rail->sendClientWindowMove(appWindow, actions.snapRect);
				}
				/* WM resize can land after button-up (e.g. KDE tiling) while syncGeometry was
				 * latched out. Sync post-loop geometry now; also needed for deferred snaps where
				 * snapRect preceded the WM's final tile size. */
				if (appWindow->window() && !appWindow->effectivelyMaximized() &&
				    !appWindow->localMoveSizeChanged())
				{
					const SDL_WindowID sdlId = appWindow->sdlId();
					lock.unlock();
					rail->syncGeometry(sdlId);
				}
			}
		}
	}
	return CHANNEL_RC_OK;
}

/* A local move/resize runs under the WM grab (not Wayland, the compositor owns that drag). */
bool SdlRail::dragActive(SDL_WindowID id)
{
	return !compositorDrag() && suppressServerMotion(id);
}

bool SdlRail::suppressServerMotion(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	if (_localMove.id == 0)
		return false;
	auto* appWindow = getWindowBySdlId(id);
	/* Suppress motion events during active local move. */
	return appWindow && appWindow->localMoveActive();
}

void SdlRail::redrawDuringSizeMove(void* userdata)
{
	auto* rail = static_cast<SdlRail*>(userdata);
	WINPR_ASSERT(rail);
	rail->_sizeMoveRedraws++;
	if (!rail->_context->drawToWindows())
		WLog_WARN(TAG, "redraw during the native size/move loop failed");
}

void SdlRail::nativeResize(void* userdata, SDL_WindowID id, int moveType, bool end)
{
	auto* rail = static_cast<SdlRail*>(userdata);
	WINPR_ASSERT(rail);
	std::unique_lock lock(rail->_windowsLock);
	if (!end)
	{
		/* A server-started drag runs the same loop and already drives the frame. */
		if (rail->_localMove.id != 0)
			return;
		auto* appWindow = rail->getWindowBySdlId(id);
		if (!appWindow || appWindow->isPopup() || appWindow->effectivelyMaximized())
			return;
		WLog_DBG(TAG, "native resize start id=0x%08" PRIx64 " type=%d", appWindow->id(), moveType);
		appWindow->setLocalMoveActive(true);
		const RailEdges e = railEdges(static_cast<uint16_t>(moveType));
		appWindow->setResizeAnchor(e.left, e.top);
		rail->_nativeResizeId = id;
		return;
	}
	if (rail->_nativeResizeId != id)
		return;
	rail->_nativeResizeId = 0;
	auto* appWindow = rail->getWindowBySdlId(id);
	if (!appWindow || !appWindow->window() || appWindow->isDeleted())
		return;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);
	SDL_GetWindowPosition(appWindow->window(), &x, &y);
	WLog_DBG(TAG, "native resize end id=0x%08" PRIx64 " %d,%d %dx%d", appWindow->id(), x, y, w, h);
	rail->reportAndAdopt(appWindow, x, y, w, h);
}

void SdlRail::handleLocalMoveRequested(uint32_t windowId, uint16_t moveType)
{
	const RailDragModel model = railPlatformCaps().drag;
	if (model == RailDragModel::None)
		return;
	const bool compositor = (model == RailDragModel::CompositorResize);

	std::unique_lock lock(_windowsLock);
	auto appWindow = getWindow(windowId);
	if (!appWindow || !appWindow->window() || appWindow->isPopup())
		return;

	const bool isMove = (moveType == RAIL_WMSZ_MOVE);
	WLog_DBG(TAG, "local move start id=0x%08" PRIx32 " driver=%s type=%" PRIu16 " %s", windowId,
	         SDL_GetCurrentVideoDriver(), moveType, isMove ? "move" : "resize");
	const bool started = railPlatformBeginDrag(appWindow->window(), moveType);
	/* Latch and drive the local frame; a Wayland move needs neither (the compositor owns it). */
	if (started && (!compositor || !isMove))
	{
		_localMove = {}; /* clear all per-drag state as a unit before latching this drag */
		_localMove.id = windowId;
		_localMove.type = moveType;
		if (!compositor)
		{
			/* Latch drag anchors for release calculation. */
			_localMove.anchor = _lastPressServer;
			_localMove.pointer = _lastPointerServer;
			/* Clear prior loop-end state on new drag. */
			appWindow->clearLoopEnd();
		}
		appWindow->setLocalMoveActive(true);
		if (!isMove)
		{
			/* Anchor the stale frame to the fixed edge (opposite the dragged one). */
			const RailEdges e = railEdges(moveType);
			appWindow->setResizeAnchor(e.left, e.top);
		}
	}
	else if (!started)
	{
		WLog_WARN(TAG, "WM move failed for RAIL window 0x%08" PRIx32, windowId);
	}

	/* Native modal loop: runs unlocked until the drag ends. */
	if (started && (model == RailDragModel::ModalLoop))
	{
		SDL_Window* sdlWindow = appWindow->window();
		lock.unlock();
		_sizeMoveRedraws = 0;
		railPlatformRunModalDrag(sdlWindow, moveType);
		WLog_DBG(TAG, "modal drag end id=0x%08" PRIx32 " redraws=%" PRIu32, windowId,
		         _sizeMoveRedraws);
		completeLocalMoveIfPending();
	}
}

/* Wayland: keep server window position in sync with the active monitor across displays. */
void SdlRail::handleDisplayChanged(SDL_WindowID id, SDL_DisplayID display)
{
	if (railPlatformCaps().supportsWindowPositions)
		return;
	std::unique_lock lock(_windowsLock);
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || appWindow->isPopup() || appWindow->effectivelyMaximized())
		return;
	/* The end of the server move loop would undo it. */
	if (_moveLoop.id == appWindow->id())
	{
		_moveLoop.display = display;
		return;
	}
	moveToDisplay(appWindow, display);
}

void SdlRail::moveToDisplay(SdlRailWindow* appWindow, SDL_DisplayID display)
{
	const auto* settings = _context->common()->context.settings;
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	for (UINT32 x = 0; x < count; x++)
	{
		const auto* mon = static_cast<const rdpMonitor*>(
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x));
		if (!mon || (mon->orig_screen != display))
			continue;
		const SDL_Rect area = { mon->x, mon->y, mon->width, mon->height };
		SDL_Rect rect = appWindow->windowRect();
		const SDL_Point center = { rect.x + rect.w / 2, rect.y + rect.h / 2 };
		if (SDL_PointInRect(&center, &area))
			return;
		rect.x = area.x + std::max(0, (area.w - rect.w) / 2);
		rect.y = area.y + std::max(0, (area.h - rect.h) / 2);
		WLog_DBG(TAG, "window 0x%08" PRIx32 " now on display %" PRIu32 ", server move to %d,%d",
		         static_cast<uint32_t>(appWindow->id()), display, rect.x, rect.y);
		sendClientWindowMove(appWindow, rect);
		appWindow->adoptLocalGeometry(rect);
		return;
	}
}

/* Send ClientWindowMove with full frame geometry including server margins (caller holds
 * _windowsLock). */
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
	/* Report geometry if changed. */
	const SDL_Rect cur = appWindow->windowRect();
	const bool unchanged = SDL_RectsEqual(&rect, &cur);
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

void SdlRail::clampIntoDesktop(const SdlRailWindow& appWindow, int& x, int& y, int w, int h) const
{
	auto* settings = _context->context()->settings;
	const int dw = static_cast<int>(freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth));
	const int dh = static_cast<int>(freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight));
	/* Clamp the content rect to the remote desktop; the resize band may extend outside. */
	const SDL_Rect s = appWindow.serverRect({ x, y, w, h });
	x += std::clamp(s.x, _inputOrigin.x, _inputOrigin.x + std::max(0, dw - s.w)) - s.x;
	y += std::clamp(s.y, _inputOrigin.y, _inputOrigin.y + std::max(0, dh - s.h)) - s.y;
}

void SdlRail::noteDragResize(SDL_WindowID id, int w, int h)
{
	std::unique_lock lock(_windowsLock);
	if ((_localMove.id == 0) || compositorDrag() || (_localMove.type != RAIL_WMSZ_MOVE))
		return;
	auto* appWindow = getWindowBySdlId(id);
	if (!appWindow || (static_cast<uint32_t>(appWindow->id()) != _localMove.id))
		return;
	/* Filter out reconcile echo from server drag-restore. */
	if (appWindow->localMoveSizeChanged())
	{
		const SDL_Rect outer = appWindow->outerRect();
		if ((w == outer.w) && (h == outer.h))
			return;
	}
	_localMove.wmSized = true;
	_localMove.wmSize = { w, h };
}

/* Update server work area from maximized window bounds (caller holds _windowsLock). */
void SdlRail::reportMaximizedWorkArea(SdlRailWindow* appWindow)
{
	/* Ignore unconfirmed maximize states that still include resize insets. */
	if ((SDL_GetWindowFlags(appWindow->window()) & SDL_WINDOW_MAXIMIZED) == 0)
		return;
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);
	if ((w <= 0) || (h <= 0))
		return;
	/* Clip work area within primary display bounds. */
	SDL_Rect primary{};
	if (!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &primary))
		return;
	/* Without readable positions a maximized window is taken to be on the primary monitor. */
	int x = primary.x;
	int y = primary.y;
	if (railPlatformCaps().supportsWindowPositions)
		SDL_GetWindowPosition(appWindow->window(), &x, &y);
	const SDL_Rect area = { x, y, w, h };
	SDL_Rect overlap{};
	if (!SDL_GetRectIntersection(&area, &primary, &overlap) || !SDL_RectsEqual(&overlap, &area))
		return;
	sendWorkArea(area);
}

void SdlRail::syncGeometry(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	auto* w = getWindowBySdlId(id);
	if (!w || !w->window() || w->isDeleted() || w->isPopup())
		return;

	/* Geometry is server/WM-owned when maximized or minimized. */
	if (w->effectivelyMaximized())
	{
		reportMaximizedWorkArea(w);
		return;
	}
	if (w->railMinimized())
		return;

	const auto wid = static_cast<uint32_t>(w->id());
	/* Skip geometry sync while awaiting server restored rect. */
	if (w->localMoveActive() || (_localMove.id == wid) || w->loopEndPending() ||
	    w->stateTransitionPending() || w->awaitingRestoreRect() || w->insetsStale())
		return;

	int lw = 0;
	int lh = 0;
	SDL_GetWindowSize(w->window(), &lw, &lh);
	if ((lw <= 0) || (lh <= 0))
		return;

	/* Ignore invalid zero-content window size updates. */
	const SDL_Rect ins = w->insets();
	if (((lw - ins.x - ins.w) <= 0) || ((lh - ins.y - ins.h) <= 0))
	{
		WLog_DBG(TAG, "geometry sync id=0x%08" PRIx32 " %dx%d dropped (content collapsed)", wid, lw,
		         lh);
		return;
	}

	const SDL_Rect vis = w->outerRect();
	const bool posKnown = railPlatformCaps().supportsWindowPositions;
	int lx = vis.x;
	int ly = vis.y;
	if (posKnown)
		SDL_GetWindowPosition(w->window(), &lx, &ly);
	else
		clampIntoDesktop(*w, lx, ly, lw, lh);

	/* Check if window settled at applied target. */
	if ((lw == vis.w) && (lh == vis.h) && (!posKnown || ((lx == vis.x) && (ly == vis.y))))
	{
		w->clearGeomApplyPending();
		return; /* Ignore self-echo when geometry converged. */
	}

	/* Drop transient echo events while client geometry application is in flight. */
	if (w->geomApplyPending())
	{
		/* Allow live Wayland compositor resize to advance beyond in-flight apply. */
		const bool overtaken = !posKnown && ((lw > vis.w) || (lh > vis.h));
		if (!overtaken)
			return;
		w->clearGeomApplyPending();
	}

	WLog_DBG(TAG, "geometry sync id=0x%08" PRIx32 " local=%d,%d %dx%d (sync)", wid, lx, ly, lw, lh);
	reportAndAdopt(w, lx, ly, lw, lh);
}

/* A release outside our windows brings no enter: finish once the size settles. */
static constexpr Uint32 kResizeSettleMs = 500;

static Uint32 resizeSettleTimer(void* userdata, [[maybe_unused]] SDL_TimerID timerID,
                                [[maybe_unused]] Uint32 interval)
{
	if (!SDL_RunOnMainThread([](void* rail) { static_cast<SdlRail*>(rail)->settleWaylandResize(); },
	                         userdata, false))
		WLog_WARN(TAG, "could not queue the resize settle check: %s", SDL_GetError());
	return 0;
}

void SdlRail::settleWaylandResize()
{
	{
		std::unique_lock lock(_windowsLock);
		if ((_localMove.id == 0) || !compositorDrag() || !_localMove.sawResize ||
		    (SDL_GetTicks() - _localMove.lastResize < kResizeSettleMs))
			return;
	}
	completeWaylandResize();
}

void SdlRail::noteResizeGrab(SDL_WindowID id)
{
	std::unique_lock lock(_windowsLock);
	if (_localMove.id == 0)
		return;
	auto* appWindow = getWindowBySdlId(id);
	/* Mouse leave or a configure during the drag: the compositor resize is under way. */
	if (!appWindow || (static_cast<uint32_t>(appWindow->id()) != _localMove.id) ||
	    !compositorDrag())
		return;
	_localMove.sawResize = true;
	_localMove.lastResize = SDL_GetTicks();
	if (SDL_AddTimer(kResizeSettleMs, resizeSettleTimer, this) == 0)
		WLog_WARN(TAG, "resize settle timer failed: %s", SDL_GetError());
}

void SdlRail::completeWaylandResize(bool definitive)
{
	std::unique_lock lock(_windowsLock);
	if ((_localMove.id == 0) || !compositorDrag())
		return;
	/* Finalize drag resize on window enter if resize occurred. */
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
			_localMove = {}; /* Snapshot and clear drag state under lock. */
		}
		return;
	}

	/* Snapshot and clear drag state atomically under lock. */
	const LocalMove drag = _localMove;
	_localMove = {};

	auto appWindow = getWindow(drag.id);
	if (!appWindow || !appWindow->window())
		return;

	(void)SDL_SyncWindow(appWindow->window());
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);

	/* Derive Wayland origin from anchor edge. */
	const SDL_Rect start = appWindow->outerRect();
	const RailEdges e = railEdges(drag.type);
	int x = e.left ? (start.x + start.w - w) : start.x;
	int y = e.top ? (start.y + start.h - h) : start.y;
	clampIntoDesktop(*appWindow, x, y, w, h);
	reportAndAdopt(appWindow, x, y, w, h);
}

void SdlRail::completeLocalMoveIfPending()
{
	std::unique_lock lock(_windowsLock);
	if (_localMove.id == 0)
		return;
	/* Skip Wayland button-up finalize. */
	if (compositorDrag())
		return;

	/* Snapshot and clear drag state atomically under lock. */
	const LocalMove drag = _localMove;
	_localMove = {};

	auto appWindow = getWindow(drag.id);
	if (!appWindow || !appWindow->window() || appWindow->isDeleted())
		return; /* window gone mid-drag: nothing to release into or report for */

	/* Send synthetic button release if WM grab consumed physical release. */
	if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) &&
	    !(SDL_GetGlobalMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK))
		if (!railPlatformReleaseButton(appWindow->window()))
			WLog_WARN(TAG, "synthetic button release failed");

	/* Finalize WM resize before reading final size. */
	(void)SDL_SyncWindow(appWindow->window());

	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	SDL_GetWindowSize(appWindow->window(), &w, &h);
	SDL_GetWindowPosition(appWindow->window(), &x, &y);
	const bool isMove = (drag.type == RAIL_WMSZ_MOVE);
	const SDL_Rect start = appWindow->outerRect(); /* position frozen at drag start */
	/* Preserve server size during move; defer WM snap/tile size sync until loop END to avoid
	 * server drag-restore conflict. */
	if (isMove)
	{
		if (!appWindow->effectivelyMaximized() && drag.wmSized &&
		    !appWindow->localMoveSizeChanged())
		{
			/* Adopt WM size if window was resized during move. */
			w = drag.wmSize.x;
			h = drag.wmSize.y;
			appWindow->deferSnap(appWindow->serverRect({ x, y, w, h }));
		}
		else
		{
			w = start.w;
			h = start.h;
		}
	}
	/* Finalize modal loop unless server closed it early. */
	if (!drag.serverEnded)
	{
		int px = drag.anchor.x;
		int py = drag.anchor.y;
		if (isMove && appWindow->localMoveSizeChanged())
		{
			/* Measure delta from server anchor on drag-restore. */
			const SDL_Point sp = appWindow->localMoveServerPos();
			const SDL_Rect now = appWindow->serverRect({ x, y, w, h });
			px = drag.pointer.x + now.x - sp.x;
			py = drag.pointer.y + now.y - sp.y;
		}
		else if (!appWindow->effectivelyMaximized())
		{
			const RailEdges e = railEdges(drag.type);
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

		/* Adopt local geometry without sending ClientWindowMove during mouse drags. */
		const SDL_Rect rect = appWindow->serverRect({ x, y, w, h });
		const bool restoredMidDrag = appWindow->localMoveSizeChanged();
		appWindow->adoptLocalGeometry(rect);
		/* Drag complete: apply pending server resize delayed during WM grab. */
		if (restoredMidDrag)
			appWindow->markGeometryDirty();
		(void)sdl_push_user_event(SDL_EVENT_USER_UPDATE);
	}
	else if (!isMove)
	{
		/* Sync final resize geometry if server closed loop early. */
		clampIntoDesktop(*appWindow, x, y, w, h);
		reportAndAdopt(appWindow, x, y, w, h);
	}

	railPlatformEndDrag(appWindow->window());
}

UINT SdlRail::server_min_max_info(RailClientContext* context,
                                  const RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(minMaxInfo);
	auto rail = static_cast<SdlRail*>(context->custom);
	if (!rail)
		return CHANNEL_RC_OK; /* late order after disconnect */

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

/* RESIZE_MARGIN fields: set this window's frame margins and seed/backfill the session BAND. */
void SdlRail::updateMargins(SdlRailWindow* appWindow, const WINDOW_ORDER_INFO* orderInfo,
                            const WINDOW_STATE_ORDER* state)
{
	const UINT32 fieldFlags = orderInfo->fieldFlags;
	SDL_Rect m = appWindow->frameMargins(); /* raw server margins as the base */
	if (fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_X)
	{
		m.x = static_cast<int>(state->resizeMarginLeft);
		m.w = static_cast<int>(state->resizeMarginRight);
	}
	if (fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y)
	{
		m.y = static_cast<int>(state->resizeMarginTop);
		m.h = static_cast<int>(state->resizeMarginBottom);
	}
	/* Use window's own frame margins; session margin serves as fallback for band sizing. */
	appWindow->setFrameMargins(m);
	SDL_Rect& sm = _sessionMargins;
	if (marginsSet(m) && !marginsSet(sm))
	{
		sm = m;
		/* Backfill band of earlier zero-announce windows. */
		for (auto& [id, w] : _windows)
			if (!marginsSet(w.resizeMargins()))
				w.setResizeMargins(m.x, m.y, m.w, m.h);
	}
	const SDL_Rect bandSrc = marginsSet(m) ? m : sm;
	appWindow->setResizeMargins(bandSrc.x, bandSrc.y, bandSrc.w, bandSrc.h);
	WLog_DBG(TAG, "margins id=0x%08" PRIx32 " raw L%d T%d R%d B%d", orderInfo->windowId, m.x, m.y,
	         m.w, m.h);
}

/* SHOW field: mirror the server's show-state (visibility + maximize/minimize) locally. */
void SdlRail::updateShowState(SdlRailWindow* appWindow, const WINDOW_ORDER_INFO* orderInfo,
                              const WINDOW_STATE_ORDER* state)
{
	WLog_DBG(TAG, "server showState id=0x%08" PRIx32 " state=0x%02" PRIx32, orderInfo->windowId,
	         state->showState);
	appWindow->setVisible(state->showState != WINDOW_HIDE);
	switch (state->showState)
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

/* VISIBILITY field: convert the server's visibility rects (window-relative) and store them. */
void SdlRail::updateVisRects(SdlRailWindow* appWindow, const WINDOW_ORDER_INFO* orderInfo,
                             const WINDOW_STATE_ORDER* state)
{
	std::vector<SDL_Rect> rects;
	rects.reserve(state->numVisibilityRects);
	for (UINT32 i = 0; i < state->numVisibilityRects; i++)
	{
		const RECTANGLE_16& r = state->visibilityRects[i];
		rects.push_back({ r.left, r.top, r.right - r.left, r.bottom - r.top });
	}
	if (rects.empty())
		WLog_DBG(TAG, "visrects id=0x%08" PRIx32 " n=0", orderInfo->windowId);
	else
		WLog_DBG(TAG, "visrects id=0x%08" PRIx32 " n=%zu first=%dx%d+%d+%d", orderInfo->windowId,
		         rects.size(), rects.at(0).w, rects.at(0).h, rects.at(0).x, rects.at(0).y);
	appWindow->setVisibilityRects(std::move(rects));
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
		/* Seed resize band margins from session default. */
		const SDL_Rect& sm = rail->_sessionMargins;
		if (marginsSet(sm))
			appWindow->setResizeMargins(sm.x, sm.y, sm.w, sm.h);
		/* The SDL window is created lazily on the main thread (reconcile), as SDL requires. */
	}

	if (!appWindow)
	{
		/* An order for a window this client does not track, e.g. one it dropped
		 * locally because its SDL parent went away (reconcile cascade) while the
		 * server still has it. Failing the order tears down the whole connection
		 * (fastpath update failure -> disconnect); ignore it instead and wait for
		 * the next WINDOW_ORDER_STATE_NEW for this id. */
		WLog_WARN(TAG, "ignoring order flags=0x%08" PRIx32 " for unknown window id=0x%08" PRIx32,
		          fieldFlags, orderInfo->windowId);
		return TRUE;
	}

	/* Show state first: a minimized window's rect is not a placement (updateWindowRect). */
	if (fieldFlags & WINDOW_ORDER_FIELD_SHOW)
		updateShowState(appWindow, orderInfo, windowState);

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
		rail->updateMargins(appWindow, orderInfo, windowState);
	if (fieldFlags & WINDOW_ORDER_FIELD_STYLE)
		appWindow->setStyle(windowState->style, windowState->extendedStyle);
	if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		char* title = rail_string_to_utf8_string(&windowState->titleInfo);
		if (title)
		{
			appWindow->setTitle(title);
			free(title);
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
		updateVisRects(appWindow, orderInfo, windowState);

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
	return &_iconCache.at(idx);
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

	/* Clear cached references to deleted window id. */
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
		/* ARC_COMPLETED also follows every secure desktop (UAC): start the app only once. */
		if (app && (strnlen(app, 1) > 0) && !rail->_appStarted)
		{
			WLog_DBG(TAG, "RAIL mode enabled (monitored desktop); launching '%s'", app);
			if (client_rail_server_start_cmd(rail->_rail) != CHANNEL_RC_OK)
			{
				WLog_ERR(TAG, "client_rail_server_start_cmd failed for '%s'", app);
				return FALSE;
			}
			rail->_appStarted = true;
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

BOOL SdlRail::non_monitored_desktop(rdpContext* context,
                                    WINPR_ATTR_UNUSED const WINDOW_ORDER_INFO* orderInfo)
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
