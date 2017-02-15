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

#ifndef __XF_RAIL_H
#define __XF_RAIL_H

#include "xf_client.h"
#include "xfreerdp.h"

#include <freerdp/client/rail.h>

void xf_rail_paint(xfContext* xfc, INT32 uleft, INT32 utop, UINT32 uright, UINT32 ubottom);
void xf_rail_send_client_system_command(xfContext* xfc, UINT32 windowId, UINT16 command);
void xf_rail_send_activate(xfContext* xfc, Window xwindow, BOOL enabled);
void xf_rail_adjust_position(xfContext* xfc, xfAppWindow* appWindow);
void xf_rail_end_local_move(xfContext* xfc, xfAppWindow* appWindow);
void xf_rail_enable_remoteapp_mode(xfContext* xfc);
void xf_rail_disable_remoteapp_mode(xfContext* xfc);

int xf_rail_init(xfContext* xfc, RailClientContext* rail);
int xf_rail_uninit(xfContext* xfc, RailClientContext* rail);

#endif /* __XF_RAIL_H */
