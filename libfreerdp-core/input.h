/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Input PDUs
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

#ifndef __INPUT_H
#define __INPUT_H

#include "rdp.h"
#include "fastpath.h"

#include <freerdp/input.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

/* Input Events */
#define INPUT_EVENT_SYNC		0x0000
#define INPUT_EVENT_SCANCODE		0x0004
#define INPUT_EVENT_UNICODE		0x0005
#define INPUT_EVENT_MOUSE		0x8001
#define INPUT_EVENT_MOUSEX		0x8002

#define RDP_CLIENT_INPUT_PDU_HEADER_LENGTH	4

void input_send_synchronize_event(rdpInput* input, uint32 flags);
void input_send_keyboard_event(rdpInput* input, uint16 flags, uint16 code);
void input_send_unicode_keyboard_event(rdpInput* input, uint16 flags, uint16 code);
void input_send_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y);
void input_send_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y);

void input_send_fastpath_synchronize_event(rdpInput* input, uint32 flags);
void input_send_fastpath_keyboard_event(rdpInput* input, uint16 flags, uint16 code);
void input_send_fastpath_unicode_keyboard_event(rdpInput* input, uint16 flags, uint16 code);
void input_send_fastpath_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y);
void input_send_fastpath_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y);

boolean input_recv(rdpInput* input, STREAM* s);

void input_register_client_callbacks(rdpInput* input);

rdpInput* input_new(rdpRdp* rdp);
void input_free(rdpInput* input);

#endif /* __INPUT_H */
