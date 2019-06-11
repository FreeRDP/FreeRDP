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

#include "pf_update.h"
#include "pf_context.h"

/* server callbacks */

static BOOL pf_server_refresh_rect(rdpContext* context, BYTE count,
                                   const RECTANGLE_16* areas)
{
	pServerContext* ps = (pServerContext*)context;
	rdpContext* pc = (rdpContext*) ps->pdata->pc;
	return pc->update->RefreshRect(pc, count, areas);
}

static BOOL pf_server_suppress_output(rdpContext* context, BYTE allow,
                                      const RECTANGLE_16* area)
{
	pServerContext* ps = (pServerContext*)context;
	rdpContext* pc = (rdpContext*) ps->pdata->pc;
	return pc->update->SuppressOutput(pc, allow, area);
}

/* client callbacks */

/**
 * This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas.
 */
static BOOL pf_client_begin_paint(rdpContext* context)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->BeginPaint(ps);
}

/**
 * This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
static BOOL pf_client_end_paint(rdpContext* context)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->EndPaint(ps);
}

static BOOL pf_client_bitmap_update(rdpContext* context, const BITMAP_UPDATE* bitmap)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->BitmapUpdate(ps, bitmap);
}

static BOOL pf_client_desktop_resize(rdpContext* context)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	ps->settings->DesktopWidth = context->settings->DesktopWidth;
	ps->settings->DesktopHeight = context->settings->DesktopHeight;
	return ps->update->DesktopResize(ps);
}

static BOOL pf_client_remote_monitors(rdpContext* context, UINT32 count,
                                      const MONITOR_DEF* monitors)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return freerdp_display_send_monitor_layout(ps, count, monitors);
}

static BOOL pf_client_send_pointer_system(rdpContext* context,
                                       const POINTER_SYSTEM_UPDATE* pointer_system)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->pointer->PointerSystem(ps, pointer_system);
}

static BOOL pf_client_send_pointer_position(rdpContext* context,
        const POINTER_POSITION_UPDATE* pointerPosition)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->pointer->PointerPosition(ps, pointerPosition);
}

static BOOL pf_client_send_pointer_color(rdpContext* context,
                                      const POINTER_COLOR_UPDATE* pointer_color)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->pointer->PointerColor(ps, pointer_color);
}

static BOOL pf_client_send_pointer_new(rdpContext* context,
                                    const POINTER_NEW_UPDATE* pointer_new)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->pointer->PointerNew(ps, pointer_new);
}

static BOOL pf_client_send_pointer_cached(rdpContext* context,
                                       const POINTER_CACHED_UPDATE* pointer_cached)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;
	return ps->update->pointer->PointerCached(ps, pointer_cached);
}

void pf_client_register_update_callbacks(rdpUpdate* update)
{
	update->BeginPaint = pf_client_begin_paint;
	update->EndPaint = pf_client_end_paint;
	update->BitmapUpdate = pf_client_bitmap_update;
	update->DesktopResize = pf_client_desktop_resize;
	update->RemoteMonitors = pf_client_remote_monitors;

	update->pointer->PointerSystem = pf_client_send_pointer_system;
	update->pointer->PointerPosition = pf_client_send_pointer_position;
	update->pointer->PointerColor = pf_client_send_pointer_color;
	update->pointer->PointerNew = pf_client_send_pointer_new;
	update->pointer->PointerCached = pf_client_send_pointer_cached;
}

void pf_server_register_update_callbacks(rdpUpdate* update)
{
	update->RefreshRect = pf_server_refresh_rect;
	update->SuppressOutput = pf_server_suppress_output;
}
