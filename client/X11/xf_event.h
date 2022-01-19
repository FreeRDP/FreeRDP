/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Event Handling
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

#ifndef FREERDP_CLIENT_X11_EVENT_H
#define FREERDP_CLIENT_X11_EVENT_H

#include "xf_keyboard.h"

#include "xf_client.h"
#include "xfreerdp.h"

BOOL xf_event_action_script_init(xfContext* xfc);
void xf_event_action_script_free(xfContext* xfc);

BOOL xf_event_process(freerdp* instance, const XEvent* event);
void xf_event_SendClientEvent(xfContext* xfc, xfWindow* window, Atom atom, unsigned int numArgs,
                              ...);

void xf_event_adjust_coordinates(xfContext* xfc, int* x, int* y);
void xf_adjust_coordinates_to_screen(xfContext* xfc, UINT32* x, UINT32* y);

BOOL xf_generic_MotionNotify(xfContext* xfc, int x, int y, int state, Window window, BOOL app);
BOOL xf_generic_RawMotionNotify(xfContext* xfc, int x, int y, Window window, BOOL app);
BOOL xf_generic_ButtonEvent(xfContext* xfc, int x, int y, int button, Window window, BOOL app,
                            BOOL down);
BOOL xf_generic_RawButtonEvent(xfContext* xfc, int button, BOOL app, BOOL down);

#endif /* FREERDP_CLIENT_X11_EVENT_H */
