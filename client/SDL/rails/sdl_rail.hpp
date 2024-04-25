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

#pragma once

#include <map>
#include <string>
#include <vector>

#include <stdint.h>

#include <freerdp/client/rail.h>
#include <freerdp/update.h>
#include "../sdl_types.hpp"

#include "sdl_rail_window.hpp"
#include "sdl_rail_icon_cache.hpp"

class SdlRail
{
  public:
	SdlRail(SdlContext* context);
	virtual ~SdlRail();

	bool show();

	BOOL paint(const RECTANGLE_16& rect);
	BOOL paint_surface(uint64_t windowId, const RECTANGLE_16& rect);

	void send_client_system_command(UINT32 windowId, UINT16 command);
	void send_activate(SDL_Window* window, BOOL enabled);
	void adjust_position(SdlRailWindow& appWindow);
	void end_local_move(SdlRailWindow& appWindow);
	void enable_remoteapp_mode(bool enable);

	SdlRailWindow* add_window(uint64_t id, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
	                          uint32_t surfaceId);
	SdlRailWindow* get_window(uint64_t id);

	BOOL del_window(uint64_t id);

	int init(RailClientContext* rail);
	int uninit(RailClientContext* rail);

  private:
	SdlRail(const SdlRail& other) = delete;
	SdlRail(SdlRail&& other) = delete;

  private:
	void abort_session();
	UINT set_min_max_info(const RAIL_MINMAXINFO_ORDER* minMaxInfo);
	BOOL cached_icon(uint64_t windowId, uint8_t cacheId, uint16_t entryId, uint32_t flags);
	BOOL cache_icon(uint64_t windowId, uint32_t flags, const ICON_INFO* info);

  private:
	static void register_update_callbacks(rdpUpdate* update);

	static UINT server_execute_result(RailClientContext* context,
	                                  const RAIL_EXEC_RESULT_ORDER* execResult);

	static UINT server_handshake(RailClientContext* context, const RAIL_HANDSHAKE_ORDER* handshake);
	static UINT server_handshake_ex(RailClientContext* context,
	                                const RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
	static UINT server_get_appid_response(RailClientContext* context,
	                                      const RAIL_GET_APPID_RESP_ORDER* getAppIdResp);
	static UINT server_language_bar_info(RailClientContext* context,
	                                     const RAIL_LANGBAR_INFO_ORDER* langBarInfo);
	static UINT server_min_max_info(RailClientContext* context,
	                                const RAIL_MINMAXINFO_ORDER* minMaxInfo);
	static UINT server_system_param(RailClientContext* context,
	                                const RAIL_SYSPARAM_ORDER* sysparam);
	static UINT server_local_move_size(RailClientContext* context,
	                                   const RAIL_LOCALMOVESIZE_ORDER* localMoveSize);

	static BOOL monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                              const MONITORED_DESKTOP_ORDER* monitoredDesktop);

	static BOOL non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo);

	static BOOL notify_icon_common(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                               const NOTIFY_ICON_STATE_ORDER* notifyIconState);

	static BOOL notify_icon_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                               const NOTIFY_ICON_STATE_ORDER* notifyIconState);

	static BOOL notify_icon_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                               const NOTIFY_ICON_STATE_ORDER* notifyIconState);

	static BOOL notify_icon_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo);

	static BOOL window_cached_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                               const WINDOW_CACHED_ICON_ORDER* windowCachedIcon);

	static BOOL window_common(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                          const WINDOW_STATE_ORDER* windowState);
	static BOOL window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo);
	static BOOL window_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
	                        const WINDOW_ICON_ORDER* window_icon);

  private:
	SdlContext* _context;
	bool _enabled;
	RailClientContext* _rail;
	std::map<uint64_t, SdlRailWindow> _windows;
	SdlRailIconCache _icon_cache;
};
