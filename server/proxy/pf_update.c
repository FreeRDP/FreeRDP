/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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
#include <winpr/image.h>
#include <winpr/sysinfo.h>

#include "pf_update.h"
#include "pf_capture.h"
#include "pf_context.h"
#include "pf_log.h"

#define TAG PROXY_TAG("update")


static BOOL pf_server_refresh_rect(rdpContext* context, BYTE count, const RECTANGLE_16* areas)
{
	pServerContext* ps = (pServerContext*)context;
	rdpContext* pc = (rdpContext*)ps->pdata->pc;
	return pc->update->RefreshRect(pc, count, areas);
}

static BOOL pf_server_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16* area)
{
	pServerContext* ps = (pServerContext*)context;
	rdpContext* pc = (rdpContext*)ps->pdata->pc;
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
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
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
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	rdpGdi* gdi = context->gdi;

	/* proxy end paint */
	if (!ps->update->EndPaint(ps))
		return FALSE;

	if (!pdata->config->SessionCapture)
		return TRUE;

	if (gdi->suppressOutput)
		return TRUE;

	if (gdi->primary->hdc->hwnd->ninvalid < 1)
		return TRUE;

	if (!pf_capture_save_frame(pc, gdi->primary_buffer))
		WLog_ERR(TAG, "failed to save captured frame!");

	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	gdi->primary->hdc->hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL pf_client_bitmap_update(rdpContext* context, const BITMAP_UPDATE* bitmap)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->BitmapUpdate(ps, bitmap);
}

static BOOL pf_client_desktop_resize(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	ps->settings->DesktopWidth = context->settings->DesktopWidth;
	ps->settings->DesktopHeight = context->settings->DesktopHeight;
	return ps->update->DesktopResize(ps);
}

static BOOL pf_client_remote_monitors(rdpContext* context, UINT32 count,
                                      const MONITOR_DEF* monitors)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return freerdp_display_send_monitor_layout(ps, count, monitors);
}

static BOOL pf_client_send_pointer_system(rdpContext* context,
                                          const POINTER_SYSTEM_UPDATE* pointer_system)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerSystem(ps, pointer_system);
}

static BOOL pf_client_send_pointer_position(rdpContext* context,
                                            const POINTER_POSITION_UPDATE* pointerPosition)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerPosition(ps, pointerPosition);
}

static BOOL pf_client_send_pointer_color(rdpContext* context,
                                         const POINTER_COLOR_UPDATE* pointer_color)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerColor(ps, pointer_color);
}

static BOOL pf_client_send_pointer_new(rdpContext* context, const POINTER_NEW_UPDATE* pointer_new)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerNew(ps, pointer_new);
}

static BOOL pf_client_send_pointer_cached(rdpContext* context,
                                          const POINTER_CACHED_UPDATE* pointer_cached)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->pointer->PointerCached(ps, pointer_cached);
}

static BOOL pf_client_save_session_info(rdpContext* context, UINT32 type, void* data)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->SaveSessionInfo(ps, type, data);
}

static BOOL pf_client_server_status_info(rdpContext* context, UINT32 status)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->ServerStatusInfo(ps, status);
}

static BOOL pf_client_window_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                    const WINDOW_STATE_ORDER* windowState)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->WindowCreate(ps, orderInfo, windowState);
}

static BOOL pf_client_window_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                    const WINDOW_STATE_ORDER* windowState)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->WindowUpdate(ps, orderInfo, windowState);
}

static BOOL pf_client_window_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                  const WINDOW_ICON_ORDER* windowIcon)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->WindowIcon(ps, orderInfo, windowIcon);
}

static BOOL pf_client_window_cached_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
        const WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->WindowCachedIcon(ps, orderInfo, windowCachedIcon);
}

static BOOL pf_client_window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->WindowDelete(ps, orderInfo);
}

static BOOL pf_client_notify_icon_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
        const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->NotifyIconCreate(ps, orderInfo, notifyIconState);
}

static BOOL pf_client_notify_icon_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
        const NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->NotifyIconUpdate(ps, orderInfo, notifyIconState);
}

static BOOL pf_client_notify_icon_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->NotifyIconDelete(ps, orderInfo);
}

static BOOL pf_client_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                        const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->MonitoredDesktop(ps, orderInfo, monitoredDesktop);
}

static BOOL pf_client_non_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	pClientContext* pc = (pClientContext*)context;
	rdpContext* ps = (rdpContext*) pc->pdata->ps;
	WLog_DBG(TAG, __FUNCTION__);
	return ps->update->window->NonMonitoredDesktop(ps, orderInfo);
}

void pf_server_register_update_callbacks(rdpUpdate* update)
{
	update->RefreshRect = pf_server_refresh_rect;
	update->SuppressOutput = pf_server_suppress_output;
}

void pf_client_register_update_callbacks(rdpUpdate* update)
{
	update->BeginPaint = pf_client_begin_paint;
	update->EndPaint = pf_client_end_paint;
	update->BitmapUpdate = pf_client_bitmap_update;
	update->DesktopResize = pf_client_desktop_resize;
	update->RemoteMonitors = pf_client_remote_monitors;
	update->SaveSessionInfo = pf_client_save_session_info;
	update->ServerStatusInfo = pf_client_server_status_info;
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
	update->pointer->PointerNew = pf_client_send_pointer_new;
	update->pointer->PointerCached = pf_client_send_pointer_cached;
}
