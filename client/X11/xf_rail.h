/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "xfreerdp.h"

void xf_rail_paint(xfInfo* xfi, rdpRail* rail, sint32 uleft, sint32 utop, uint32 uright, uint32 ubottom);
void xf_rail_register_callbacks(xfInfo* xfi, rdpRail* rail);
void xf_rail_send_client_system_command(xfInfo* xfi, uint32 windowId, uint16 command);
void xf_rail_send_activate(xfInfo* xfi, Window xwindow, boolean enabled);
void xf_process_rail_event(xfInfo* xfi, rdpChannels* chanman, RDP_EVENT* event);
void xf_rail_adjust_position(xfInfo* xfi, rdpWindow *window);
void xf_rail_end_local_move(xfInfo* xfi, rdpWindow *window);

#endif /* __XF_RAIL_H */
