/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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
#include "message.h"

#include <freerdp/input.h>
#include <freerdp/freerdp.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

/* Input Events */
#define INPUT_EVENT_SYNC		0x0000
#define INPUT_EVENT_SCANCODE		0x0004
#define INPUT_EVENT_UNICODE		0x0005
#define INPUT_EVENT_MOUSE		0x8001
#define INPUT_EVENT_MOUSEX		0x8002

#define RDP_CLIENT_INPUT_PDU_HEADER_LENGTH	4

FREERDP_LOCAL BOOL input_send_synchronize_event(rdpInput* input, UINT32 flags);
FREERDP_LOCAL BOOL input_send_keyboard_event(rdpInput* input, UINT16 flags,
        UINT16 code);
FREERDP_LOCAL BOOL input_send_unicode_keyboard_event(rdpInput* input,
        UINT16 flags, UINT16 code);
FREERDP_LOCAL BOOL input_send_mouse_event(rdpInput* input, UINT16 flags,
        UINT16 x, UINT16 y);
FREERDP_LOCAL BOOL input_send_extended_mouse_event(rdpInput* input,
        UINT16 flags, UINT16 x, UINT16 y);

FREERDP_LOCAL BOOL input_send_fastpath_synchronize_event(rdpInput* input,
        UINT32 flags);
FREERDP_LOCAL BOOL input_send_fastpath_keyboard_event(rdpInput* input,
        UINT16 flags, UINT16 code);
FREERDP_LOCAL BOOL input_send_fastpath_unicode_keyboard_event(rdpInput* input,
        UINT16 flags, UINT16 code);
FREERDP_LOCAL BOOL input_send_fastpath_mouse_event(rdpInput* input,
        UINT16 flags, UINT16 x, UINT16 y);
FREERDP_LOCAL BOOL input_send_fastpath_extended_mouse_event(rdpInput* input,
        UINT16 flags, UINT16 x, UINT16 y);

FREERDP_LOCAL BOOL input_recv(rdpInput* input, wStream* s);

FREERDP_LOCAL int input_process_events(rdpInput* input);
FREERDP_LOCAL BOOL input_register_client_callbacks(rdpInput* input);

FREERDP_LOCAL rdpInput* input_new(rdpRdp* rdp);
FREERDP_LOCAL void input_free(rdpInput* input);

#endif /* __INPUT_H */
