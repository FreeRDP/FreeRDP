/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <freerdp/display.h>
#include <freerdp/session.h>
#include <winpr/assert.h>
#include <winpr/image.h>
#include <winpr/sysinfo.h>

#include <freerdp/server/proxy/proxy_log.h>

#include "pf_update.h"
#include "pf_client.h"
#include "pf_server.h"
#include <freerdp/server/proxy/proxy_context.h>
#include "proxy_modules.h"

#define TAG PROXY_TAG("update")

WINPR_ATTR_NODISCARD
static BOOL pf_server_refresh_rect(rdpContext* context, BYTE count, const RECTANGLE_16* areas)
{
	pServerContext* ps = (pServerContext*)context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pClientContext* pc = proxy_data_get_client_context(ps->pdata);
	WINPR_ASSERT(pc);

	if (!freerdp_is_active_state(&pc->cctx.context))
		return TRUE;

	WINPR_ASSERT(pc->cctx.context.update);
	WINPR_ASSERT(pc->cctx.context.update->RefreshRect);
	return pc->cctx.context.update->RefreshRect(&pc->cctx.context, count, areas);
}

WINPR_ATTR_NODISCARD
static BOOL pf_server_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16* area)
{
	pServerContext* ps = (pServerContext*)context;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	pClientContext* pc = proxy_data_get_client_context(ps->pdata);
	WINPR_ASSERT(pc);

	if (!freerdp_is_active_state(&pc->cctx.context))
		return TRUE;

	WINPR_ASSERT(pc->cctx.context.update);
	WINPR_ASSERT(pc->cctx.context.update->SuppressOutput);
	return pc->cctx.context.update->SuppressOutput(&pc->cctx.context, allow, area);
}

/* Proxy from PC to PS */

/**
 * This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas.
 */
WINPR_ATTR_NODISCARD
static BOOL pf_client_begin_paint(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);

	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->BeginPaint);
	WLog_DBG(TAG, "called");
	return ps->context.update->BeginPaint(&ps->context);
}

/**
 * This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
WINPR_ATTR_NODISCARD
static BOOL pf_client_end_paint(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->EndPaint);

	WLog_DBG(TAG, "called");

	/* proxy end paint */
	if (!ps->context.update->EndPaint(&ps->context))
		return FALSE;

	if (!pf_modules_run_hook(pdata->module, HOOK_TYPE_CLIENT_END_PAINT, pdata, context))
		return FALSE;

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_bitmap_update(rdpContext* context, const BITMAP_UPDATE* bitmap)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->BitmapUpdate);
	WLog_DBG(TAG, "called");
	return ps->context.update->BitmapUpdate(&ps->context, bitmap);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_desktop_resize(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->DesktopResize);
	WINPR_ASSERT(context->settings);
	WINPR_ASSERT(ps->context.settings);
	WLog_DBG(TAG, "called");
	if (!freerdp_settings_copy_item(ps->context.settings, context->settings, FreeRDP_DesktopWidth))
		return FALSE;
	if (!freerdp_settings_copy_item(ps->context.settings, context->settings, FreeRDP_DesktopHeight))
		return FALSE;
	return ps->context.update->DesktopResize(&ps->context);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_remote_monitors(rdpContext* context, UINT32 count,
                                      const MONITOR_DEF* monitors)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WLog_DBG(TAG, "called");
	return freerdp_display_send_monitor_layout(&ps->context, count, monitors);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_send_pointer_system(rdpContext* context,
                                          const POINTER_SYSTEM_UPDATE* pointer_system)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->pointer);
	WINPR_ASSERT(ps->context.update->pointer->PointerSystem);
	WLog_DBG(TAG, "called");
	return ps->context.update->pointer->PointerSystem(&ps->context, pointer_system);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_send_pointer_position(rdpContext* context,
                                            const POINTER_POSITION_UPDATE* pointerPosition)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->pointer);
	WINPR_ASSERT(ps->context.update->pointer->PointerPosition);
	WLog_DBG(TAG, "called");
	return ps->context.update->pointer->PointerPosition(&ps->context, pointerPosition);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_send_pointer_color(rdpContext* context,
                                         const POINTER_COLOR_UPDATE* pointer_color)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->pointer);
	WINPR_ASSERT(ps->context.update->pointer->PointerColor);
	WLog_DBG(TAG, "called");
	return ps->context.update->pointer->PointerColor(&ps->context, pointer_color);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_send_pointer_large(rdpContext* context,
                                         const POINTER_LARGE_UPDATE* pointer_large)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->pointer);
	WINPR_ASSERT(ps->context.update->pointer->PointerLarge);
	WLog_DBG(TAG, "called");
	return ps->context.update->pointer->PointerLarge(&ps->context, pointer_large);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_send_pointer_new(rdpContext* context, const POINTER_NEW_UPDATE* pointer_new)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->pointer);
	WINPR_ASSERT(ps->context.update->pointer->PointerNew);
	WLog_DBG(TAG, "called");
	return ps->context.update->pointer->PointerNew(&ps->context, pointer_new);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_send_pointer_cached(rdpContext* context,
                                          const POINTER_CACHED_UPDATE* pointer_cached)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->pointer);
	WINPR_ASSERT(ps->context.update->pointer->PointerCached);
	WLog_DBG(TAG, "called");
	return ps->context.update->pointer->PointerCached(&ps->context, pointer_cached);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_save_session_info(rdpContext* context, UINT32 type, void* data)
{
	logon_info* logonInfo = nullptr;
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->SaveSessionInfo);

	WLog_DBG(TAG, "called");

	switch (type)
	{
		case INFO_TYPE_LOGON:
		case INFO_TYPE_LOGON_LONG:
		{
			logonInfo = (logon_info*)data;
			PROXY_LOG_INFO(TAG, pc, "client logon info: Username: %s, Domain: %s",
			               logonInfo->username, logonInfo->domain);
			break;
		}

		default:
			break;
	}

	return ps->context.update->SaveSessionInfo(&ps->context, type, data);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_server_status_info(rdpContext* context, UINT32 status)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->ServerStatusInfo);

	WLog_DBG(TAG, "called");
	return ps->context.update->ServerStatusInfo(&ps->context, status);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_set_keyboard_indicators(rdpContext* context, UINT16 led_flags)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->SetKeyboardIndicators);

	WLog_DBG(TAG, "called");
	return ps->context.update->SetKeyboardIndicators(&ps->context, led_flags);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_set_keyboard_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                              UINT32 imeConvMode)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->SetKeyboardImeStatus);

	WLog_DBG(TAG, "called");
	return ps->context.update->SetKeyboardImeStatus(&ps->context, imeId, imeState, imeConvMode);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_SurfaceBits(rdpContext* context,
                                  const SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);

	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->SurfaceBits);

	WLog_DBG(TAG, "called");
	return ps->context.update->SurfaceBits(&ps->context, surfaceBitsCommand);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_SurfaceFrameMarker(rdpContext* context,
                                         const SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);

	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->SurfaceFrameMarker);

	WLog_DBG(TAG, "called");
	return ps->context.update->SurfaceFrameMarker(&ps->context, surfaceFrameMarker);
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_window_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                    const WINDOW_STATE_ORDER* windowState)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->WindowCreate);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc = ps->context.update->window->WindowCreate(&ps->context, orderInfo, windowState);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_window_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                    const WINDOW_STATE_ORDER* windowState)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->WindowUpdate);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc = ps->context.update->window->WindowUpdate(&ps->context, orderInfo, windowState);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_window_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                  const WINDOW_ICON_ORDER* windowIcon)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->WindowIcon);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc = ps->context.update->window->WindowIcon(&ps->context, orderInfo, windowIcon);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_window_cached_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                         const WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->WindowCachedIcon);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc =
	    ps->context.update->window->WindowCachedIcon(&ps->context, orderInfo, windowCachedIcon);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->WindowDelete);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc = ps->context.update->window->WindowDelete(&ps->context, orderInfo);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_notify_icon_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                         const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->NotifyIconCreate);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc =
	    ps->context.update->window->NotifyIconCreate(&ps->context, orderInfo, notifyIconState);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_notify_icon_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                         const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->NotifyIconUpdate);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc =
	    ps->context.update->window->NotifyIconUpdate(&ps->context, orderInfo, notifyIconState);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_notify_icon_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->NotifyIconDelete);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc = ps->context.update->window->NotifyIconDelete(&ps->context, orderInfo);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                        const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->MonitoredDesktop);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc =
	    ps->context.update->window->MonitoredDesktop(&ps->context, orderInfo, monitoredDesktop);
	rdp_update_unlock(ps->context.update);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL pf_client_non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_ASSERT(pc);

	proxyData* pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	pServerContext* ps = proxy_data_get_server_context(pdata);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->context.update);
	WINPR_ASSERT(ps->context.update->window);
	WINPR_ASSERT(ps->context.update->window->NonMonitoredDesktop);

	WLog_DBG(TAG, "called");
	rdp_update_lock(ps->context.update);
	const BOOL rc = ps->context.update->window->NonMonitoredDesktop(&ps->context, orderInfo);
	rdp_update_unlock(ps->context.update);
	return rc;
}

void pf_server_register_update_callbacks(rdpUpdate* update)
{
	WINPR_ASSERT(update);
	update->RefreshRect = pf_server_refresh_rect;
	update->SuppressOutput = pf_server_suppress_output;
}

void pf_client_register_update_callbacks(rdpUpdate* update)
{
	WINPR_ASSERT(update);
	update->BeginPaint = pf_client_begin_paint;
	update->EndPaint = pf_client_end_paint;
	update->BitmapUpdate = pf_client_bitmap_update;
	update->DesktopResize = pf_client_desktop_resize;
	update->RemoteMonitors = pf_client_remote_monitors;
	update->SaveSessionInfo = pf_client_save_session_info;
	update->ServerStatusInfo = pf_client_server_status_info;
	update->SetKeyboardIndicators = pf_client_set_keyboard_indicators;
	update->SetKeyboardImeStatus = pf_client_set_keyboard_ime_status;

	/* see gdi_register_update_callbacks */
	update->SurfaceBits = pf_client_SurfaceBits;
	update->SurfaceFrameMarker = pf_client_SurfaceFrameMarker;

	/* Rail window updates */
	update->window->WindowCreate = pf_client_window_create;
	update->window->WindowUpdate = pf_client_window_update;
	update->window->WindowIcon = pf_client_window_icon;
	update->window->WindowCachedIcon = pf_client_window_cached_icon;
	update->window->WindowDelete = pf_client_window_delete;
	update->window->NotifyIconCreate = pf_client_notify_icon_create;
	update->window->NotifyIconUpdate = pf_client_notify_icon_update;
	update->window->NotifyIconDelete = pf_client_notify_icon_delete;
	update->window->MonitoredDesktop = pf_client_monitored_desktop;
	update->window->NonMonitoredDesktop = pf_client_non_monitored_desktop;

	/* Pointer updates */
	update->pointer->PointerSystem = pf_client_send_pointer_system;
	update->pointer->PointerPosition = pf_client_send_pointer_position;
	update->pointer->PointerColor = pf_client_send_pointer_color;
	update->pointer->PointerLarge = pf_client_send_pointer_large;
	update->pointer->PointerNew = pf_client_send_pointer_new;
	update->pointer->PointerCached = pf_client_send_pointer_cached;
}
