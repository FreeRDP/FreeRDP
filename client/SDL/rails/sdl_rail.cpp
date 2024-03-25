/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL RAIL
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <winpr/wlog.h>
#include <winpr/print.h>

#include <freerdp/client/rail.h>

#include "../sdl_freerdp.hpp"
#include "sdl_rail.hpp"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

static const char* error_code_names[] = { "RAIL_EXEC_S_OK",
	                                      "RAIL_EXEC_E_HOOK_NOT_LOADED",
	                                      "RAIL_EXEC_E_DECODE_FAILED",
	                                      "RAIL_EXEC_E_NOT_IN_ALLOWLIST",
	                                      "RAIL_EXEC_E_FILE_NOT_FOUND",
	                                      "RAIL_EXEC_E_FAIL",
	                                      "RAIL_EXEC_E_SESSION_LOCKED" };

#ifdef WITH_DEBUG_RAIL
static const char* movetype_names[] = {
	"(invalid)",        "RAIL_WMSZ_LEFT",       "RAIL_WMSZ_RIGHT",
	"RAIL_WMSZ_TOP",    "RAIL_WMSZ_TOPLEFT",    "RAIL_WMSZ_TOPRIGHT",
	"RAIL_WMSZ_BOTTOM", "RAIL_WMSZ_BOTTOMLEFT", "RAIL_WMSZ_BOTTOMRIGHT",
	"RAIL_WMSZ_MOVE",   "RAIL_WMSZ_KEYMOVE",    "RAIL_WMSZ_KEYSIZE"
};
#endif

SdlRail::SdlRail(SdlContext* context) : _context(context), _enabled(false), _rail(nullptr)
{
}

SdlRail::~SdlRail()
{
}

bool SdlRail::show()
{
	// TODO
	return true;
}

BOOL SdlRail::paint(const RECTANGLE_16& rect)
{
	for (auto& it : _windows)
	{
		if (!paint_surface(it.first, rect))
			return FALSE;
	}

	return TRUE;
}

BOOL SdlRail::paint_surface(UINT64 windowId, const RECTANGLE_16& rect)
{
	auto appWindow = get_window(windowId);

	if (!appWindow)
		return FALSE;

	auto apprect = appWindow->rect();
	const RECTANGLE_16 windowRect = { .left = static_cast<uint16_t>(MAX(apprect.x, 0u)),
		                              .top = static_cast<uint16_t>(MAX(apprect.y, 0u)),
		                              .right =
		                                  static_cast<uint16_t>(MAX(apprect.x + apprect.w, 0u)),
		                              .bottom =
		                                  static_cast<uint16_t>(MAX(apprect.y + apprect.h, 0u)) };

	REGION16 windowInvalidRegion = { 0 };
	region16_init(&windowInvalidRegion);
	region16_union_rect(&windowInvalidRegion, &windowInvalidRegion, &windowRect);
	region16_intersect_rect(&windowInvalidRegion, &windowInvalidRegion, &rect);

	if (!region16_is_empty(&windowInvalidRegion))
	{
		auto apprect = appWindow->rect();
		const RECTANGLE_16* extents = region16_extents(&windowInvalidRegion);
		auto x = extents->left - apprect.x;
		auto y = extents->top - apprect.y;
		const SDL_Rect updateRect = { x, y, (extents->right - apprect.x) - x,
			                          (extents->bottom - apprect.y) - y };

		appWindow->update(updateRect);
	}
	region16_uninit(&windowInvalidRegion);
	return TRUE;
}

void SdlRail::send_client_system_command(UINT32 windowId, UINT16 command)
{
	RAIL_SYSCOMMAND_ORDER syscommand{ windowId, command };
	WINPR_ASSERT(_rail);
	WINPR_ASSERT(_rail->ClientSystemCommand);
	_rail->ClientSystemCommand(_rail, &syscommand);
}

void SdlRail::send_activate(SDL_Window* window, BOOL enabled)
{
	if (!window)
		return;

	auto id = SDL_GetWindowID(window);
	auto appWindow = get_window(id);
	if (!appWindow)
		return;

	appWindow->applyStyle(enabled);
	RAIL_ACTIVATE_ORDER activate{ static_cast<uint32_t>(appWindow->id()), enabled };

	WINPR_ASSERT(_rail);
	WINPR_ASSERT(_rail->ClientActivate);
	_rail->ClientActivate(_rail, &activate);
}

void SdlRail::adjust_position(SdlRailWindow& appWindow)
{
	if (!appWindow.mapped() || (appWindow.localMoveState() != SdlRailWindow::LMS_NOT_ACTIVE))
		return;

	const auto& rect = appWindow.rect();
	const auto& wrect = appWindow.windowRect();

	/* If current window position disagrees with RDP window position, send update to RDP server */
	if ((rect.x != wrect.x) || (rect.y != wrect.y) || (rect.w != wrect.w) || (rect.h != wrect.h))
	{
		RAIL_WINDOW_MOVE_ORDER windowMove;

		if (!appWindow.mapped() || (appWindow.localMoveState() != SdlRailWindow::LMS_NOT_ACTIVE))
			return;

		/* If current window position disagrees with RDP window position, send update to RDP server
		 */
		if ((rect.x != wrect.x) || (rect.y != wrect.y) || (rect.w != wrect.w) ||
		    (rect.h != wrect.h))
		{
			windowMove.windowId = appWindow.id();
			/*
			 * Calculate new size/position for the rail window(new values for
			 * windowOffsetX/windowOffsetY/windowWidth/windowHeight) on the server
			 */
			auto margins = appWindow.margins();
			windowMove.left = rect.x - margins.x;
			windowMove.top = rect.y - margins.y;
			windowMove.right = rect.x + rect.w + margins.w;
			windowMove.bottom = rect.y + rect.h + margins.h;

			WINPR_ASSERT(_rail);
			WINPR_ASSERT(_rail->ClientWindowMove);
			_rail->ClientWindowMove(_rail, &windowMove);
		}
		windowMove.windowId = appWindow.id();
		/*
		 * Calculate new size/position for the rail window(new values for
		 * windowOffsetX/windowOffsetY/windowWidth/windowHeight) on the server
		 */
		auto margins = appWindow.margins();
		windowMove.left = rect.x - margins.x;
		windowMove.top = rect.y - margins.y;
		windowMove.right = rect.x + rect.w + margins.w;
		windowMove.bottom = rect.y + rect.h + margins.h;

		WINPR_ASSERT(_rail);
		WINPR_ASSERT(_rail->ClientWindowMove);
		_rail->ClientWindowMove(_rail, &windowMove);
	}
}

void SdlRail::end_local_move(SdlRailWindow& appWindow)
{
	auto rect = appWindow.rect();

	if ((appWindow.localMoveType() == RAIL_WMSZ_KEYMOVE) ||
	    (appWindow.localMoveType() == RAIL_WMSZ_KEYSIZE))
	{
		RAIL_WINDOW_MOVE_ORDER windowMove{};

		/*
		 * For keyboard moves send and explicit update to RDP server
		 */
		windowMove.windowId = appWindow.id();
		/*
		 * Calculate new size/position for the rail window(new values for
		 * windowOffsetX/windowOffsetY/windowWidth/windowHeight) on the server
		 *
		 */
		auto margins = appWindow.margins();
		windowMove.left = rect.x - margins.x;
		windowMove.top = rect.y - margins.y;
		windowMove.right = rect.x + rect.w +
		                   margins.w; /* In the update to RDP the position is one past the window */
		windowMove.bottom = rect.y + rect.h + margins.h;

		WINPR_ASSERT(_rail);
		WINPR_ASSERT(_rail->ClientWindowMove);
		_rail->ClientWindowMove(_rail, &windowMove);
	}

	/*
	 * Simulate button up at new position to end the local move (per RDP spec)
	 */

	/* only send the mouse coordinates if not a keyboard move or size */
	if ((appWindow.localMoveType() != RAIL_WMSZ_KEYMOVE) &&
	    (appWindow.localMoveType() != RAIL_WMSZ_KEYSIZE))
	{
		int x = 0;
		int y = 0;
		SDL_GetMouseState(&x, &y);
		freerdp_client_send_button_event(_context->common(), FALSE, PTR_FLAGS_BUTTON1, x, y);
	}

	/*
	 * Proactively update the RAIL window dimensions.  There is a race condition where
	 * we can start to receive GDI orders for the new window dimensions before we
	 * receive the RAIL ORDER for the new window size.  This avoids that race condition.
	 */
	appWindow.updateWindowRect(rect);
	appWindow.updateLocalMoveState(SdlRailWindow::LMS_TERMINATING);
}

void SdlRail::enable_remoteapp_mode(bool enable)
{
	if (!_enabled && enable)
	{
		_enabled = true;
		// TODO: Dummy window?
	}
	else if (_enabled && !enable)
	{
		// TODO: destroy dummy?
		_enabled = false;
	}
}

SdlRailWindow* SdlRail::add_window(uint64_t id, uint32_t x, uint32_t y, uint32_t width,
                                   uint32_t height, uint32_t surfaceId)
{
	if (_windows.find(id) != _windows.end())
		return nullptr;

	const SDL_Rect rect{ static_cast<int>(x), static_cast<int>(y), static_cast<int>(width),
		                 static_cast<int>(height) };
	SdlRailWindow w{ id, surfaceId, rect };
	_windows.emplace(id, std::move(w));
	auto it = _windows.find(id);
	if (it == _windows.end())
		return nullptr;

	auto& window = it->second;

	window.create();
	return &window;
}

SdlRailWindow* SdlRail::get_window(uint64_t id)
{
	auto it = _windows.find(id);
	if (it == _windows.end())
		return nullptr;

	return &it->second;
}

BOOL SdlRail::del_window(uint64_t id)
{
	auto it = _windows.find(id);
	if (it == _windows.end())
		return FALSE;
	_windows.erase(it);
	return TRUE;
}

int SdlRail::init(RailClientContext* rail)
{
	_rail = rail;
	if (!rail)
		return 0;

	WINPR_ASSERT(_context);
	register_update_callbacks(_context->context()->update);

	rail->custom = this;
	rail->ServerExecuteResult = SdlRail::server_execute_result;
	rail->ServerSystemParam = SdlRail::server_system_param;
	rail->ServerHandshake = SdlRail::server_handshake;
	rail->ServerHandshakeEx = SdlRail::server_handshake_ex;
	rail->ServerLocalMoveSize = SdlRail::server_local_move_size;
	rail->ServerMinMaxInfo = SdlRail::server_min_max_info;
	rail->ServerLanguageBarInfo = SdlRail::server_language_bar_info;
	rail->ServerGetAppIdResponse = SdlRail::server_get_appid_response;

	_windows.clear();

	auto num_caches =
	    freerdp_settings_get_uint32(_context->context()->settings, FreeRDP_RemoteAppNumIconCaches);
	auto num_cache_entries = freerdp_settings_get_uint32(_context->context()->settings,
	                                                     FreeRDP_RemoteAppNumIconCacheEntries);
	if (!_icon_cache.prepare(num_caches, num_cache_entries))
		return 0;

	return 1;
}

int SdlRail::uninit(RailClientContext* rail)
{
	WINPR_ASSERT(rail);
	_windows.clear();
	_icon_cache.clear();
	return 1;
}

void SdlRail::abort_session()
{
	freerdp_abort_connect_context(_context->context());
}

UINT SdlRail::set_min_max_info(const RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	auto window = get_window(minMaxInfo->windowId);
	if (window)
	{
		/* TODO
	xf_SetWindowMinMaxInfo(xfc, appWindow, minMaxInfo->maxWidth, minMaxInfo->maxHeight,
		                   minMaxInfo->maxPosX, minMaxInfo->maxPosY, minMaxInfo->minTrackWidth,
		                   minMaxInfo->minTrackHeight, minMaxInfo->maxTrackWidth,
		                   minMaxInfo->maxTrackHeight);
		                   */
	}
	return CHANNEL_RC_OK;
}

BOOL SdlRail::cached_icon(uint64_t windowId, uint8_t cacheId, uint16_t entryId, uint32_t flags)
{
	auto window = get_window(windowId);
	if (!window)
		return FALSE;
	auto icon = _icon_cache.lookup(cacheId, entryId);
	if (!icon)
		return FALSE;

	auto replace = (flags & WINDOW_ORDER_STATE_NEW) != 0;
	return window->setIcon(*icon, replace);
}

BOOL SdlRail::cache_icon(uint64_t windowId, uint32_t flags, const ICON_INFO* info)
{
	auto window = get_window(windowId);
	if (!window)
		return FALSE;
	auto icon = _icon_cache.lookup(info->cacheId, info->cacheEntry);
	if (!icon)
		return FALSE;
	if (!icon->update(info))
		return FALSE;
	auto replace = (flags & WINDOW_ORDER_STATE_NEW) != 0;
	return window->setIcon(*icon, replace);
}

void SdlRail::register_update_callbacks(rdpUpdate* update)
{
	WINPR_ASSERT(update);

	auto window = update->window;
	WINPR_ASSERT(window);

	window->WindowCreate = SdlRail::window_common;
	window->WindowUpdate = SdlRail::window_common;
	window->WindowDelete = SdlRail::window_delete;
	window->WindowIcon = SdlRail::window_icon;
	window->WindowCachedIcon = SdlRail::window_cached_icon;
	window->NotifyIconCreate = SdlRail::notify_icon_create;
	window->NotifyIconUpdate = SdlRail::notify_icon_update;
	window->NotifyIconDelete = SdlRail::notify_icon_delete;
	window->MonitoredDesktop = SdlRail::monitored_desktop;
	window->NonMonitoredDesktop = SdlRail::non_monitored_desktop;
}

UINT SdlRail::server_execute_result(RailClientContext* context,
                                    const RAIL_EXEC_RESULT_ORDER* execResult)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(execResult);

	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);

	if (execResult->execResult != RAIL_EXEC_S_OK)
	{
		WLog_ERR(TAG, "RAIL exec error: execResult=%s NtError=0x%X\n",
		         error_code_names[execResult->execResult], execResult->rawResult);
		rail->abort_session();
	}
	else
		rail->enable_remoteapp_mode(true);

	return CHANNEL_RC_OK;
}

UINT SdlRail::server_handshake(RailClientContext* context, const RAIL_HANDSHAKE_ORDER* handshake)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(handshake);

	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);
	return client_rail_server_start_cmd(context);
}

UINT SdlRail::server_handshake_ex(RailClientContext* context,
                                  const RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(handshakeEx);

	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);
	return client_rail_server_start_cmd(context);
}

UINT SdlRail::server_get_appid_response(RailClientContext* context,
                                        const RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(getAppIdResp);

	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);
	return CHANNEL_RC_OK;
}

UINT SdlRail::server_language_bar_info(RailClientContext* context,
                                       const RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(langBarInfo);

	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);
	return CHANNEL_RC_OK;
}

UINT SdlRail::server_min_max_info(RailClientContext* context,
                                  const RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(minMaxInfo);

	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);
	return rail->set_min_max_info(minMaxInfo);
}

UINT SdlRail::server_system_param(RailClientContext* context, const RAIL_SYSPARAM_ORDER* sysparam)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(sysparam);

	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);
	// TODO: Actually apply param
	return CHANNEL_RC_OK;
}

BOOL SdlRail::monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(monitoredDesktop);

	auto sdl = get_context(context);
	auto& rail = sdl->rail;
	return TRUE;
}

BOOL SdlRail::non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	WINPR_ASSERT(orderInfo);

	auto sdl = get_context(context);
	auto& rail = sdl->rail;
	rail.enable_remoteapp_mode(false);
	return TRUE;
}

BOOL SdlRail::notify_icon_common(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                 const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(notifyIconState);

	auto sdl = get_context(context);
	auto& rail = sdl->rail;

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_VERSION)
	{
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP)
	{
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP)
	{
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_STATE)
	{
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_ICON)
	{
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{
	}

	return TRUE;
}

BOOL SdlRail::notify_icon_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                 const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	return notify_icon_common(context, orderInfo, notifyIconState);
}

BOOL SdlRail::notify_icon_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                 const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	return notify_icon_common(context, orderInfo, notifyIconState);
}

BOOL SdlRail::notify_icon_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	WINPR_ASSERT(orderInfo);

	auto sdl = get_context(context);
	auto& rail = sdl->rail;

	return TRUE;
}

BOOL SdlRail::window_cached_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                 const WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(windowCachedIcon);

	auto sdl = get_context(context);
	auto& rail = sdl->rail;

	return rail.cached_icon(orderInfo->windowId, windowCachedIcon->cachedIcon.cacheId,
	                        windowCachedIcon->cachedIcon.cacheEntry, orderInfo->fieldFlags);
}

BOOL SdlRail::window_common(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                            const WINDOW_STATE_ORDER* windowState)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(windowState);

	auto sdl = get_context(context);
	auto& rail = sdl->rail;

	UINT32 fieldFlags = orderInfo->fieldFlags;
	BOOL position_or_size_updated = FALSE;

	auto appWindow = rail.get_window(orderInfo->windowId);
	if (fieldFlags & WINDOW_ORDER_STATE_NEW)
	{
		if (!appWindow)
			appWindow = rail.add_window(orderInfo->windowId, windowState->windowOffsetX,
			                            windowState->windowOffsetY, windowState->windowWidth,
			                            windowState->windowHeight, 0xFFFFFFFF);

		if (!appWindow)
			return FALSE;

		if (!appWindow->setStyle(windowState->style, windowState->extendedStyle))
			return FALSE;

		/* Ensure window always gets a window title */
		if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
		{
			auto str = reinterpret_cast<const char16_t*>(windowState->titleInfo.string);
			if (!appWindow->setTitle(str, windowState->titleInfo.length))
			{
				WLog_ERR(TAG, "failed to convert window title");
				/* error handled below */
			}
		}
		else
		{
			if (!appWindow->setTitle("RdpRailWindow"))
				WLog_ERR(TAG, "failed to duplicate default window title string");
		}

		appWindow->create();
	}

	if (!appWindow)
		return FALSE;

	/* Keep track of any position/size update so that we can force a refresh of the window */
	if ((fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET) ||
	    (fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE) ||
	    (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET) ||
	    (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE) ||
	    (fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA) ||
	    (fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET) ||
	    (fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY))
	{
		position_or_size_updated = TRUE;
	}

	/* Update Parameters */

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		appWindow->updateWindowRect({ static_cast<int>(windowState->windowOffsetX),
		                              static_cast<int>(windowState->windowOffsetY), -1, -1 });
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		appWindow->updateWindowRect({ -1, -1, static_cast<int>(windowState->windowWidth),
		                              static_cast<int>(windowState->windowHeight) });
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_X)
	{
		appWindow->updateMargins({ static_cast<int>(windowState->resizeMarginLeft), -1,
		                           static_cast<int>(windowState->resizeMarginRight), -1 });
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y)
	{
		appWindow->updateMargins({ -1, static_cast<int>(windowState->resizeMarginTop), -1,
		                           static_cast<int>(windowState->resizeMarginBottom) });
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		appWindow->setOwner(windowState->ownerWindowId);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		appWindow->setStyle(windowState->style, windowState->extendedStyle);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		appWindow->setShowState(windowState->showState);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		auto str = reinterpret_cast<const char16_t*>(windowState->titleInfo.string);
		appWindow->setTitle(str, windowState->titleInfo.length);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{
		appWindow->setClientOffset({ windowState->clientOffsetX, windowState->clientOffsetY });
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		appWindow->setClientArea({ static_cast<int>(windowState->clientAreaWidth),
		                           static_cast<int>(windowState->clientAreaHeight) });
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		appWindow->setClientDelta(
		    { windowState->windowClientDeltaX, windowState->windowClientDeltaY });
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		appWindow->updateWindowRects(windowState->windowRects, windowState->numWindowRects);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		appWindow->setVisibleOffset({ windowState->visibleOffsetX, windowState->visibleOffsetY });
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		appWindow->updateVisibilityRects(windowState->visibilityRects,
		                                 windowState->numVisibilityRects);
	}

	if (position_or_size_updated)
	{
		auto clientOffset = appWindow->clientOffset();
		auto clientDelta = appWindow->clientDelta();
		auto visibleOffset = appWindow->visibleOffset();
		UINT32 visibilityRectsOffsetX = (visibleOffset.x - (clientOffset.x - clientDelta.x));
		UINT32 visibilityRectsOffsetY = (visibleOffset.y - (clientOffset.y - clientDelta.y));

		/*
		 * The rail server like to set the window to a small size when it is minimized even though
		 * it is hidden in some cases this can cause the window not to restore back to its original
		 * size. Therefore we don't update our local window when that rail window state is minimized
		 */
		if (appWindow->railState() != WINDOW_SHOW_MINIMIZED)
		{
			/* Redraw window area if already in the correct position */
			const auto windowRect = appWindow->windowRect();
			const auto rect = appWindow->rect();
			if ((rect.x == windowRect.x) && (rect.y == windowRect.y) && (rect.w == windowRect.w) &&
			    (rect.h == windowRect.h))
				appWindow->update(appWindow->rect());
			else
				appWindow->move(appWindow->windowRect());
		}
	}
	return TRUE;
}

BOOL SdlRail::window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	WINPR_ASSERT(orderInfo);

	auto sdl = get_context(context);
	auto& rail = sdl->rail;

	return rail.del_window(orderInfo->windowId);
}

BOOL SdlRail::window_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                          const WINDOW_ICON_ORDER* window_icon)
{
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(window_icon);

	auto sdl = get_context(context);
	auto& rail = sdl->rail;

	return rail.cache_icon(orderInfo->windowId, orderInfo->fieldFlags, window_icon->iconInfo);
}

UINT SdlRail::server_local_move_size(RailClientContext* context,
                                     const RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(localMoveSize);

	auto rail = static_cast<SdlRail*>(context->custom);
	WINPR_ASSERT(rail);

	int x = 0, y = 0;

	auto appWindow = rail->get_window(localMoveSize->windowId);

	if (!appWindow)
		return ERROR_INTERNAL_ERROR;

	switch (localMoveSize->moveSizeType)
	{
		case RAIL_WMSZ_LEFT:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_RIGHT:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_TOP:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_TOPLEFT:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_TOPRIGHT:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_BOTTOM:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_BOTTOMLEFT:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_BOTTOMRIGHT:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			break;

		case RAIL_WMSZ_MOVE:
			// TODO
			break;

		case RAIL_WMSZ_KEYMOVE:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			/* FIXME: local keyboard moves not working */
			return CHANNEL_RC_OK;

		case RAIL_WMSZ_KEYSIZE:
			x = localMoveSize->posX;
			y = localMoveSize->posY;
			/* FIXME: local keyboard moves not working */
			return CHANNEL_RC_OK;
		default:
			return ERROR_INVALID_DATA;
	}

	if (localMoveSize->isMoveSizeStart)
		appWindow->StartLocalMoveSize(localMoveSize->moveSizeType, x, y);
	else
		appWindow->EndLocalMoveSize();

	return CHANNEL_RC_OK;
}
