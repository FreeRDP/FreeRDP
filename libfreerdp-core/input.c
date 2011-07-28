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

#include "input.h"

void rdp_write_client_input_pdu_header(STREAM* s, uint16 number)
{
	stream_write_uint16(s, 1); /* numberEvents (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */
}

STREAM* rdp_client_input_pdu_init(rdpRdp* rdp)
{
	STREAM* s;
	s = rdp_data_pdu_init(rdp);
	rdp_write_client_input_pdu_header(s, 1);
	return s;
}

void rdp_send_client_input_pdu(rdpRdp* rdp, STREAM* s)
{
	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_INPUT, rdp->mcs->user_id);
}

void input_write_synchronize_event(STREAM* s, uint32 flags)
{
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */
	stream_write_uint32(s, flags); /* toggleFlags (4 bytes) */
}

void input_send_synchronize_event(rdpInput* input, uint32 flags)
{
	STREAM* s;
	s = rdp_client_input_pdu_init(input->rdp);
	input_write_synchronize_event(s, flags);
	rdp_send_client_input_pdu(input->rdp, s);
}

void input_write_keyboard_event(STREAM* s, uint16 flags, uint16 code)
{
	stream_write_uint16(s, flags); /* keyboardFlags (2 bytes) */
	stream_write_uint16(s, code); /* keyCode (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */
}

void input_send_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	STREAM* s;
	s = rdp_client_input_pdu_init(input->rdp);
	input_write_keyboard_event(s, flags, code);
	rdp_send_client_input_pdu(input->rdp, s);
}

void input_write_unicode_keyboard_event(STREAM* s, uint16 code)
{
	stream_write_uint16(s, 0); /* pad2OctetsA (2 bytes) */
	stream_write_uint16(s, code); /* unicodeCode (2 bytes) */
	stream_write_uint16(s, 0); /* pad2OctetsB (2 bytes) */
}

void input_send_unicode_keyboard_event(rdpInput* input, uint16 code)
{
	STREAM* s;
	s = rdp_client_input_pdu_init(input->rdp);
	input_write_unicode_keyboard_event(s, code);
	rdp_send_client_input_pdu(input->rdp, s);
}

void input_write_mouse_event(STREAM* s, uint16 flags, uint16 x, uint16 y)
{
	stream_write_uint16(s, flags); /* pointerFlags (2 bytes) */
	stream_write_uint16(s, x); /* xPos (2 bytes) */
	stream_write_uint16(s, y); /* yPos (2 bytes) */
}

void input_send_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	STREAM* s;
	s = rdp_client_input_pdu_init(input->rdp);
	input_write_mouse_event(s, flags, x, y);
	rdp_send_client_input_pdu(input->rdp, s);
}

void input_write_extended_mouse_event(STREAM* s, uint16 flags, uint16 x, uint16 y)
{
	stream_write_uint16(s, flags); /* pointerFlags (2 bytes) */
	stream_write_uint16(s, x); /* xPos (2 bytes) */
	stream_write_uint16(s, y); /* yPos (2 bytes) */
}

void input_send_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	STREAM* s;
	s = rdp_client_input_pdu_init(input->rdp);
	input_write_extended_mouse_event(s, flags, x, y);
	rdp_send_client_input_pdu(input->rdp, s);
}

rdpInput* input_new(rdpRdp* rdp)
{
	rdpInput* input;

	input = (rdpInput*) xzalloc(sizeof(rdpInput));

	if (input != NULL)
	{
		input->rdp = rdp;
		input->SynchronizeEvent = input_send_synchronize_event;
		input->KeyboardEvent = input_send_keyboard_event;
		input->UnicodeKeyboardEvent = input_send_unicode_keyboard_event;
		input->MouseEvent = input_send_mouse_event;
		input->ExtendedMouseEvent = input_send_extended_mouse_event;
	}

	return input;
}

void input_free(rdpInput* input)
{
	if (input != NULL)
	{
		xfree(input);
	}
}

