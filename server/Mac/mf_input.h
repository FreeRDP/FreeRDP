/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server (Input)
 *
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

#ifndef FREERDP_SERVER_MAC_INPUT_H
#define FREERDP_SERVER_MAC_INPUT_H

#include "mf_interface.h"

BOOL mf_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code);
BOOL mf_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code);
BOOL mf_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
BOOL mf_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);

//dummy versions
BOOL mf_input_keyboard_event_dummy(rdpInput* input, UINT16 flags, UINT16 code);
BOOL mf_input_unicode_keyboard_event_dummy(rdpInput* input, UINT16 flags, UINT16 code);
BOOL mf_input_mouse_event_dummy(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
BOOL mf_input_extended_mouse_event_dummy(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);

#endif /* FREERDP_SERVER_MAC_INPUT_H */
