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
#include <freerdp/server/proxy/proxy_context.h>
#include "proxy_modules.h"

#define TAG PROXY_TAG("update")

static BOOL pf_server_refresh_rect(rdpContext* context, BYTE count, const RECTANGLE_16* areas)
{
	pServerContext* ps = (pServerContext*)context;
	rdpContext* pc;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);
	pc = (rdpContext*)ps->pdata->pc;
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->update);
	WINPR_ASSERT(pc->update->RefreshRect);
	return pc->update->RefreshRect(pc, count, areas);
}

static BOOL pf_server_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16* area)
{
	pServerContext* ps = (pServerContext*)context;
	rdpContext* pc;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);
	pc = (rdpContext*)ps->pdata->pc;
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->update);
	WINPR_ASSERT(pc->update->SuppressOutput);
	return pc->update->SuppressOutput(pc, allow, area);
}

/* Proxy from PC to PS */

/**
 * This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas.
 */
static BOOL pf_client_begin_paint(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->BeginPaint);
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->BeginPaint(ps);
}

/**
 * This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
static BOOL pf_client_end_paint(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->EndPaint);

	WLog_DBG(TAG, __FUNCTION__);

	/* proxy end paint */
	if (!ps->update->EndPaint(ps))
		return FALSE;

	if (!pf_modules_run_hook(pdata->module, HOOK_TYPE_CLIENT_END_PAINT, pdata, context))
		return FALSE;

	return TRUE;
}

static BOOL pf_client_bitmap_update(rdpContext* context, const BITMAP_UPDATE* bitmap)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->BitmapUpdate);
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->BitmapUpdate(ps, bitmap);
}

static BOOL pf_client_desktop_resize(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->DesktopResize);
	WINPR_ASSERT(context->settings);
	WINPR_ASSERT(ps->settings);
	WLog_DBG(TAG, __FUNCTION__);
	ps->settings->DesktopWidth = context->settings->DesktopWidth;
	ps->settings->DesktopHeight = context->settings->DesktopHeight;
	return ps->update->DesktopResize(ps);
}

static BOOL pf_client_remote_monitors(rdpContext* context, UINT32 count,
                                      const MONITOR_DEF* monitors)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WLog_DBG(TAG, __FUNCTION__);
	return freerdp_display_send_monitor_layout(ps, count, monitors);
}

static BOOL pf_client_send_pointer_system(rdpContext* context,
                                          const POINTER_SYSTEM_UPDATE* pointer_system)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->pointer);
	WINPR_ASSERT(ps->update->pointer->PointerSystem);
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerSystem(ps, pointer_system);
}

static BOOL pf_client_send_pointer_position(rdpContext* context,
                                            const POINTER_POSITION_UPDATE* pointerPosition)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->pointer);
	WINPR_ASSERT(ps->update->pointer->PointerPosition);
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerPosition(ps, pointerPosition);
}

static BOOL pf_client_send_pointer_color(rdpContext* context,
                                         const POINTER_COLOR_UPDATE* pointer_color)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->pointer);
	WINPR_ASSERT(ps->update->pointer->PointerColor);
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerColor(ps, pointer_color);
}

static BOOL pf_client_send_pointer_large(rdpContext* context,
                                         const POINTER_LARGE_UPDATE* pointer_large)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->pointer);
	WINPR_ASSERT(ps->update->pointer->PointerLarge);
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerLarge(ps, pointer_large);
}

static BOOL pf_client_send_pointer_new(rdpContext* context, const POINTER_NEW_UPDATE* pointer_new)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->pointer);
	WINPR_ASSERT(ps->update->pointer->PointerNew);
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerNew(ps, pointer_new);
}

static BOOL pf_client_send_pointer_cached(rdpContext* context,
                                          const POINTER_CACHED_UPDATE* pointer_cached)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->pointer);
	WINPR_ASSERT(ps->update->pointer->PointerCached);
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerCached(ps, pointer_cached);
}

static BOOL pf_client_save_session_info(rdpContext* context, UINT32 type, void* data)
{
	logon_info* logonInfo = NULL;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->SaveSessionInfo);

	WLog_DBG(TAG, __FUNCTION__);

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

	return ps->update->SaveSessionInfo(ps, type, data);
}

static BOOL pf_client_server_status_info(rdpContext* context, UINT32 status)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->ServerStatusInfo);

	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->ServerStatusInfo(ps, status);
}

static BOOL pf_client_set_keyboard_indicators(rdpContext* context, UINT16 led_flags)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->SetKeyboardIndicators);

	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->SetKeyboardIndicators(ps, led_flags);
}

static BOOL pf_client_set_keyboard_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                              UINT32 imeConvMode)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->SetKeyboardImeStatus);

	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->SetKeyboardImeStatus(ps, imeId, imeState, imeConvMode);
}

static BOOL pf_client_window_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                    const WINDOW_STATE_ORDER* windowState)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->WindowCreate);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->WindowCreate(ps, orderInfo, windowState);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_window_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                    const WINDOW_STATE_ORDER* windowState)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->WindowUpdate);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->WindowUpdate(ps, orderInfo, windowState);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_window_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                  const WINDOW_ICON_ORDER* windowIcon)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->WindowIcon);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->WindowIcon(ps, orderInfo, windowIcon);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_window_cached_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                         const WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->WindowCachedIcon);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->WindowCachedIcon(ps, orderInfo, windowCachedIcon);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->WindowDelete);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->WindowDelete(ps, orderInfo);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_notify_icon_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                         const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->NotifyIconCreate);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->NotifyIconCreate(ps, orderInfo, notifyIconState);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_notify_icon_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                         const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->NotifyIconUpdate);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->NotifyIconUpdate(ps, orderInfo, notifyIconState);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_notify_icon_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	BOOL rc;

	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->NotifyIconDelete);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->NotifyIconDelete(ps, orderInfo);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                        const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->MonitoredDesktop);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->MonitoredDesktop(ps, orderInfo, monitoredDesktop);
	rdp_update_unlock(ps->update);
	return rc;
}

static BOOL pf_client_non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	BOOL rc;
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;
	rdpContext* ps;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	ps = (rdpContext*)pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->update);
	WINPR_ASSERT(ps->update->window);
	WINPR_ASSERT(ps->update->window->NonMonitoredDesktop);

	WLog_DBG(TAG, __FUNCTION__);
	rdp_update_lock(ps->update);
	rc = ps->update->window->NonMonitoredDesktop(ps, orderInfo);
	rdp_update_unlock(ps->update);
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
