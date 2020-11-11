/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_CHANNEL_RDPEI_COMMON_H
#define FREERDP_CHANNEL_RDPEI_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <freerdp/channels/rdpei.h>

/** @brief input event ids */
enum
{
	EVENTID_SC_READY = 0x0001,
	EVENTID_CS_READY = 0x0002,
	EVENTID_TOUCH = 0x0003,
	EVENTID_SUSPEND_TOUCH = 0x0004,
	EVENTID_RESUME_TOUCH = 0x0005,
	EVENTID_DISMISS_HOVERING_CONTACT = 0x0006,
	EVENTID_PEN = 0x0008
};

BOOL rdpei_read_2byte_unsigned(wStream* s, UINT16* value);
BOOL rdpei_write_2byte_unsigned(wStream* s, UINT16 value);
BOOL rdpei_read_2byte_signed(wStream* s, INT16* value);
BOOL rdpei_write_2byte_signed(wStream* s, INT16 value);
BOOL rdpei_read_4byte_unsigned(wStream* s, UINT32* value);
BOOL rdpei_write_4byte_unsigned(wStream* s, UINT32 value);
BOOL rdpei_read_4byte_signed(wStream* s, INT32* value);
BOOL rdpei_write_4byte_signed(wStream* s, INT32 value);
BOOL rdpei_read_8byte_unsigned(wStream* s, UINT64* value);
BOOL rdpei_write_8byte_unsigned(wStream* s, UINT64 value);

void touch_event_reset(RDPINPUT_TOUCH_EVENT* event);
void touch_frame_reset(RDPINPUT_TOUCH_FRAME* frame);

void pen_event_reset(RDPINPUT_PEN_EVENT* event);
void pen_frame_reset(RDPINPUT_PEN_FRAME* frame);

#endif /* FREERDP_CHANNEL_RDPEI_COMMON_H */
