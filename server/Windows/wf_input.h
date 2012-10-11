/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WF_INPUT_H
#define WF_INPUT_H

#include "wf_interface.h"

void wf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code);
void wf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code);
void wf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
void wf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);

//dummy versions
void wf_peer_keyboard_event_dummy(rdpInput* input, UINT16 flags, UINT16 code);
void wf_peer_unicode_keyboard_event_dummy(rdpInput* input, UINT16 flags, UINT16 code);
void wf_peer_mouse_event_dummy(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
void wf_peer_extended_mouse_event_dummy(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);

#endif /* WF_INPUT_H */
