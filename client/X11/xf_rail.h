/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 RAIL
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_X11_RAIL_H
#define FREERDP_CLIENT_X11_RAIL_H

#include "xf_client.h"
#include "xfreerdp.h"

#include <freerdp/client/rail.h>

BOOL xf_rail_paint(xfContext* xfc, const RECTANGLE_16* rect);
BOOL xf_rail_paint_surface(xfContext* xfc, UINT64 windowId, const RECTANGLE_16* rect);

void xf_rail_send_client_system_command(xfContext* xfc, UINT32 windowId, UINT16 command);
void xf_rail_send_activate(xfContext* xfc, Window xwindow, BOOL enabled);
void xf_rail_adjust_position(xfContext* xfc, xfAppWindow* appWindow);
void xf_rail_end_local_move(xfContext* xfc, xfAppWindow* appWindow);
void xf_rail_enable_remoteapp_mode(xfContext* xfc);
void xf_rail_disable_remoteapp_mode(xfContext* xfc);

xfAppWindow* xf_rail_add_window(xfContext* xfc, UINT64 id, UINT32 x, UINT32 y, UINT32 width,
                                UINT32 height, UINT32 surfaceId);
xfAppWindow* xf_rail_get_window(xfContext* xfc, UINT64 id);

BOOL xf_rail_del_window(xfContext* xfc, UINT64 id);

int xf_rail_init(xfContext* xfc, RailClientContext* rail);
int xf_rail_uninit(xfContext* xfc, RailClientContext* rail);

#endif /* FREERDP_CLIENT_X11_RAIL_H */
