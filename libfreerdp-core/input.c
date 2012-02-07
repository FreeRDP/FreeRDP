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

void rdp_write_input_event_header(STREAM* s, uint32 time, uint16 type)
{
	stream_write_uint32(s, time); /* eventTime (4 bytes) */
	stream_write_uint16(s, type); /* messageType (2 bytes) */
}

STREAM* rdp_client_input_pdu_init(rdpRdp* rdp, uint16 type)
{
	STREAM* s;
	s = rdp_data_pdu_init(rdp);
	rdp_write_client_input_pdu_header(s, 1);
	rdp_write_input_event_header(s, 0, type);
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
	rdpRdp* rdp = input->context->rdp;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_SYNC);
	input_write_synchronize_event(s, flags);
	rdp_send_client_input_pdu(rdp, s);
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
	rdpRdp* rdp = input->context->rdp;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_SCANCODE);
	input_write_keyboard_event(s, flags, code);
	rdp_send_client_input_pdu(rdp, s);
}

void input_write_unicode_keyboard_event(STREAM* s, uint16 flags, uint16 code)
{
	stream_write_uint16(s, flags); /* keyboardFlags (2 bytes) */
	stream_write_uint16(s, code); /* unicodeCode (2 bytes) */
	stream_write_uint16(s, 0); /* pad2Octets (2 bytes) */
}

void input_send_unicode_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	STREAM* s;
	uint16 keyboardFlags = 0;
	rdpRdp* rdp = input->context->rdp;

	/*
	 * According to the specification, the slow path Unicode Keyboard Event
	 * (TS_UNICODE_KEYBOARD_EVENT) contains KBD_FLAGS_RELEASE flag when key
	 * is released, but contains no flags when it is pressed.
	 * This is different from the slow path Keyboard Event
	 * (TS_KEYBOARD_EVENT) which does contain KBD_FLAGS_DOWN flag when the
	 * key is pressed.
	 * There is no KBD_FLAGS_EXTENDED flag in TS_UNICODE_KEYBOARD_EVENT.
	 */
	keyboardFlags |= (flags & KBD_FLAGS_RELEASE) ? KBD_FLAGS_RELEASE : 0;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_UNICODE);
	input_write_unicode_keyboard_event(s, flags, code);
	rdp_send_client_input_pdu(rdp, s);
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
	rdpRdp* rdp = input->context->rdp;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_MOUSE);
	input_write_mouse_event(s, flags, x, y);
	rdp_send_client_input_pdu(rdp, s);
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
	rdpRdp* rdp = input->context->rdp;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_MOUSEX);
	input_write_extended_mouse_event(s, flags, x, y);
	rdp_send_client_input_pdu(rdp, s);
}

void input_send_fastpath_synchronize_event(rdpInput* input, uint32 flags)
{
	STREAM* s;
	rdpRdp* rdp = input->context->rdp;

	/* The FastPath Synchronization eventFlags has identical values as SlowPath */
	s = fastpath_input_pdu_init(rdp->fastpath, (uint8) flags, FASTPATH_INPUT_EVENT_SYNC);
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	STREAM* s;
	uint8 eventFlags = 0;
	rdpRdp* rdp = input->context->rdp;

	eventFlags |= (flags & KBD_FLAGS_RELEASE) ? FASTPATH_INPUT_KBDFLAGS_RELEASE : 0;
	eventFlags |= (flags & KBD_FLAGS_EXTENDED) ? FASTPATH_INPUT_KBDFLAGS_EXTENDED : 0;
	s = fastpath_input_pdu_init(rdp->fastpath, eventFlags, FASTPATH_INPUT_EVENT_SCANCODE);
	stream_write_uint8(s, code); /* keyCode (1 byte) */
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_unicode_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	STREAM* s;
	uint8 eventFlags = 0;
	rdpRdp* rdp = input->context->rdp;

	eventFlags |= (flags & KBD_FLAGS_RELEASE) ? FASTPATH_INPUT_KBDFLAGS_RELEASE : 0;
	s = fastpath_input_pdu_init(rdp->fastpath, eventFlags, FASTPATH_INPUT_EVENT_UNICODE);
	stream_write_uint16(s, code); /* unicodeCode (2 bytes) */
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	STREAM* s;
	rdpRdp* rdp = input->context->rdp;

	s = fastpath_input_pdu_init(rdp->fastpath, 0, FASTPATH_INPUT_EVENT_MOUSE);
	input_write_mouse_event(s, flags, x, y);
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	STREAM* s;
	rdpRdp* rdp = input->context->rdp;

	s = fastpath_input_pdu_init(rdp->fastpath, 0, FASTPATH_INPUT_EVENT_MOUSEX);
	input_write_extended_mouse_event(s, flags, x, y);
	fastpath_send_input_pdu(rdp->fastpath, s);
}

static boolean input_recv_sync_event(rdpInput* input, STREAM* s)
{
	uint32 toggleFlags;

	if (stream_get_left(s) < 6)
		return false;

	stream_seek(s, 2); /* pad2Octets (2 bytes) */
	stream_read_uint32(s, toggleFlags); /* toggleFlags (4 bytes) */

	IFCALL(input->SynchronizeEvent, input, toggleFlags);

	return true;
}

static boolean input_recv_keyboard_event(rdpInput* input, STREAM* s)
{
	uint16 keyboardFlags, keyCode;

	if (stream_get_left(s) < 6)
		return false;

	stream_read_uint16(s, keyboardFlags); /* keyboardFlags (2 bytes) */
	stream_read_uint16(s, keyCode); /* keyCode (2 bytes) */
	stream_seek(s, 2); /* pad2Octets (2 bytes) */

	IFCALL(input->KeyboardEvent, input, keyboardFlags, keyCode);

	return true;
}

static boolean input_recv_unicode_keyboard_event(rdpInput* input, STREAM* s)
{
	uint16 keyboardFlags, unicodeCode;

	if (stream_get_left(s) < 6)
		return false;

	stream_read_uint16(s, keyboardFlags); /* keyboardFlags (2 bytes) */
	stream_read_uint16(s, unicodeCode); /* unicodeCode (2 bytes) */
	stream_seek(s, 2); /* pad2Octets (2 bytes) */

	/*
	 * According to the specification, the slow path Unicode Keyboard Event
	 * (TS_UNICODE_KEYBOARD_EVENT) contains KBD_FLAGS_RELEASE flag when key
	 * is released, but contains no flags when it is pressed.
	 * This is different from the slow path Keyboard Event
	 * (TS_KEYBOARD_EVENT) which does contain KBD_FLAGS_DOWN flag when the
	 * key is pressed.
	 * Set the KBD_FLAGS_DOWN flag if the KBD_FLAGS_RELEASE flag is missing.
	 */

	if ((keyboardFlags & KBD_FLAGS_RELEASE) == 0)
		keyboardFlags |= KBD_FLAGS_DOWN;

	IFCALL(input->UnicodeKeyboardEvent, input, keyboardFlags, unicodeCode);

	return true;
}

static boolean input_recv_mouse_event(rdpInput* input, STREAM* s)
{
	uint16 pointerFlags, xPos, yPos;

	if (stream_get_left(s) < 6)
		return false;

	stream_read_uint16(s, pointerFlags); /* pointerFlags (2 bytes) */
	stream_read_uint16(s, xPos); /* xPos (2 bytes) */
	stream_read_uint16(s, yPos); /* yPos (2 bytes) */

	IFCALL(input->MouseEvent, input, pointerFlags, xPos, yPos);

	return true;
}

static boolean input_recv_extended_mouse_event(rdpInput* input, STREAM* s)
{
	uint16 pointerFlags, xPos, yPos;

	if (stream_get_left(s) < 6)
		return false;

	stream_read_uint16(s, pointerFlags); /* pointerFlags (2 bytes) */
	stream_read_uint16(s, xPos); /* xPos (2 bytes) */
	stream_read_uint16(s, yPos); /* yPos (2 bytes) */

	IFCALL(input->ExtendedMouseEvent, input, pointerFlags, xPos, yPos);

	return true;
}

static boolean input_recv_event(rdpInput* input, STREAM* s)
{
	uint16 messageType;

	if (stream_get_left(s) < 4)
		return false;

	stream_seek(s, 4); /* eventTime (4 bytes), ignored by the server */
	stream_read_uint16(s, messageType); /* messageType (2 bytes) */

	switch (messageType)
	{
		case INPUT_EVENT_SYNC:
			if (!input_recv_sync_event(input, s))
				return false;
			break;

		case INPUT_EVENT_SCANCODE:
			if (!input_recv_keyboard_event(input, s))
				return false;
			break;

		case INPUT_EVENT_UNICODE:
			if (!input_recv_unicode_keyboard_event(input, s))
				return false;
			break;

		case INPUT_EVENT_MOUSE:
			if (!input_recv_mouse_event(input, s))
				return false;
			break;

		case INPUT_EVENT_MOUSEX:
			if (!input_recv_extended_mouse_event(input, s))
				return false;
			break;

		default:
			printf("Unknown messageType %u\n", messageType);
			/* Each input event uses 6 bytes. */
			stream_seek(s, 6);
			break;
	}

	return true;
}

boolean input_recv(rdpInput* input, STREAM* s)
{
	uint16 i, numberEvents;

	if (stream_get_left(s) < 4)
		return false;

	stream_read_uint16(s, numberEvents); /* numberEvents (2 bytes) */
	stream_seek(s, 2); /* pad2Octets (2 bytes) */

	/* Each input event uses 6 exactly bytes. */
	if (stream_get_left(s) < 6 * numberEvents)
		return false;

	for (i = 0; i < numberEvents; i++)
	{
		if (!input_recv_event(input, s))
			return false;
	}

	return true;
}

void input_register_client_callbacks(rdpInput* input)
{
	rdpRdp* rdp = input->context->rdp;

	if (rdp->settings->fastpath_input)
	{
		input->SynchronizeEvent = input_send_fastpath_synchronize_event;
		input->KeyboardEvent = input_send_fastpath_keyboard_event;
		input->UnicodeKeyboardEvent = input_send_fastpath_unicode_keyboard_event;
		input->MouseEvent = input_send_fastpath_mouse_event;
		input->ExtendedMouseEvent = input_send_fastpath_extended_mouse_event;
	}
	else
	{
		input->SynchronizeEvent = input_send_synchronize_event;
		input->KeyboardEvent = input_send_keyboard_event;
		input->UnicodeKeyboardEvent = input_send_unicode_keyboard_event;
		input->MouseEvent = input_send_mouse_event;
		input->ExtendedMouseEvent = input_send_extended_mouse_event;
	}
}

rdpInput* input_new(rdpRdp* rdp)
{
	rdpInput* input;

	input = (rdpInput*) xzalloc(sizeof(rdpInput));

	if (input != NULL)
	{

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
