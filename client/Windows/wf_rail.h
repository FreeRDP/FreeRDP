/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows RAIL
 *
 * Copyright 2012 Jason Champion <jchampion@zetacentauri.com>
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
#ifndef __WF_RAIL_H
#define __WF_RAIL_H

#include "wf_interface.h"

void wf_rail_paint(wfContext* wfc, rdpRail* rail, INT32 uleft, INT32 utop, UINT32 uright, UINT32 ubottom);
void wf_rail_register_callbacks(wfContext* wfc, rdpRail* rail);
void wf_rail_send_client_system_command(wfContext* wfc, UINT32 windowId, UINT16 command);
void wf_rail_send_activate(wfContext* wfc, HWND window, BOOL enabled);
void wf_process_rail_event(wfContext* wfc, rdpChannels* channels, wMessage* event);
void wf_rail_adjust_position(wfContext* wfc, rdpWindow* window);
void wf_rail_end_local_move(wfContext* wfc, rdpWindow* window);

#endif
