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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <freerdp/input.h>

#include "message.h"

#include "input.h"

void rdp_write_client_input_pdu_header(wStream* s, UINT16 number)
{
	Stream_Write_UINT16(s, 1); /* numberEvents (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */
}

void rdp_write_input_event_header(wStream* s, UINT32 time, UINT16 type)
{
	Stream_Write_UINT32(s, time); /* eventTime (4 bytes) */
	Stream_Write_UINT16(s, type); /* messageType (2 bytes) */
}

wStream* rdp_client_input_pdu_init(rdpRdp* rdp, UINT16 type)
{
	wStream* s;
	s = rdp_data_pdu_init(rdp);
	rdp_write_client_input_pdu_header(s, 1);
	rdp_write_input_event_header(s, 0, type);
	return s;
}

void rdp_send_client_input_pdu(rdpRdp* rdp, wStream* s)
{
	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_INPUT, rdp->mcs->userId);
}

void input_write_synchronize_event(wStream* s, UINT32 flags)
{
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */
	Stream_Write_UINT32(s, flags); /* toggleFlags (4 bytes) */
}

void input_send_synchronize_event(rdpInput* input, UINT32 flags)
{
	wStream* s;
	rdpRdp* rdp = input->context->rdp;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_SYNC);
	input_write_synchronize_event(s, flags);
	rdp_send_client_input_pdu(rdp, s);
}

void input_write_keyboard_event(wStream* s, UINT16 flags, UINT16 code)
{
	Stream_Write_UINT16(s, flags); /* keyboardFlags (2 bytes) */
	Stream_Write_UINT16(s, code); /* keyCode (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */
}

void input_send_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	wStream* s;
	rdpRdp* rdp = input->context->rdp;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_SCANCODE);
	input_write_keyboard_event(s, flags, code);
	rdp_send_client_input_pdu(rdp, s);
}

void input_write_unicode_keyboard_event(wStream* s, UINT16 flags, UINT16 code)
{
	Stream_Write_UINT16(s, flags); /* keyboardFlags (2 bytes) */
	Stream_Write_UINT16(s, code); /* unicodeCode (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad2Octets (2 bytes) */
}

void input_send_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	wStream* s;
	UINT16 keyboardFlags = 0;
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

void input_write_mouse_event(wStream* s, UINT16 flags, UINT16 x, UINT16 y)
{
	Stream_Write_UINT16(s, flags); /* pointerFlags (2 bytes) */
	Stream_Write_UINT16(s, x); /* xPos (2 bytes) */
	Stream_Write_UINT16(s, y); /* yPos (2 bytes) */
}

void input_send_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	wStream* s;
	rdpRdp* rdp = input->context->rdp;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_MOUSE);
	input_write_mouse_event(s, flags, x, y);
	rdp_send_client_input_pdu(rdp, s);
}

void input_write_extended_mouse_event(wStream* s, UINT16 flags, UINT16 x, UINT16 y)
{
	Stream_Write_UINT16(s, flags); /* pointerFlags (2 bytes) */
	Stream_Write_UINT16(s, x); /* xPos (2 bytes) */
	Stream_Write_UINT16(s, y); /* yPos (2 bytes) */
}

void input_send_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	wStream* s;
	rdpRdp* rdp = input->context->rdp;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_MOUSEX);
	input_write_extended_mouse_event(s, flags, x, y);
	rdp_send_client_input_pdu(rdp, s);
}

void input_send_focus_in_event(rdpInput* input, UINT16 toggleStates, UINT16 x, UINT16 y)
{
	/* send a tab up like mstsc.exe */
	input_send_keyboard_event(input, KBD_FLAGS_RELEASE, 0x0f);

	/* send the toggle key states */
	input_send_synchronize_event(input, (toggleStates & 0x1F));

	/* send another tab up like mstsc.exe */
	input_send_keyboard_event(input, KBD_FLAGS_RELEASE, 0x0f);

	/* finish with a mouse pointer position like mstsc.exe */
	input_send_mouse_event(input, PTR_FLAGS_MOVE, x, y);
}

void input_send_fastpath_synchronize_event(rdpInput* input, UINT32 flags)
{
	wStream* s;
	rdpRdp* rdp = input->context->rdp;

	/* The FastPath Synchronization eventFlags has identical values as SlowPath */
	s = fastpath_input_pdu_init(rdp->fastpath, (BYTE) flags, FASTPATH_INPUT_EVENT_SYNC);
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	wStream* s;
	BYTE eventFlags = 0;
	rdpRdp* rdp = input->context->rdp;

	eventFlags |= (flags & KBD_FLAGS_RELEASE) ? FASTPATH_INPUT_KBDFLAGS_RELEASE : 0;
	eventFlags |= (flags & KBD_FLAGS_EXTENDED) ? FASTPATH_INPUT_KBDFLAGS_EXTENDED : 0;
	s = fastpath_input_pdu_init(rdp->fastpath, eventFlags, FASTPATH_INPUT_EVENT_SCANCODE);
	Stream_Write_UINT8(s, code); /* keyCode (1 byte) */
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	wStream* s;
	BYTE eventFlags = 0;
	rdpRdp* rdp = input->context->rdp;

	eventFlags |= (flags & KBD_FLAGS_RELEASE) ? FASTPATH_INPUT_KBDFLAGS_RELEASE : 0;
	s = fastpath_input_pdu_init(rdp->fastpath, eventFlags, FASTPATH_INPUT_EVENT_UNICODE);
	Stream_Write_UINT16(s, code); /* unicodeCode (2 bytes) */
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	wStream* s;
	rdpRdp* rdp = input->context->rdp;

	s = fastpath_input_pdu_init(rdp->fastpath, 0, FASTPATH_INPUT_EVENT_MOUSE);
	input_write_mouse_event(s, flags, x, y);
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	wStream* s;
	rdpRdp* rdp = input->context->rdp;

	s = fastpath_input_pdu_init(rdp->fastpath, 0, FASTPATH_INPUT_EVENT_MOUSEX);
	input_write_extended_mouse_event(s, flags, x, y);
	fastpath_send_input_pdu(rdp->fastpath, s);
}

void input_send_fastpath_focus_in_event(rdpInput* input, UINT16 toggleStates, UINT16 x, UINT16 y)
{
	wStream* s;
	rdpRdp* rdp = input->context->rdp;
	BYTE eventFlags = 0;

	s = fastpath_input_pdu_init_header(rdp->fastpath);
	/* send a tab up like mstsc.exe */
	eventFlags = FASTPATH_INPUT_KBDFLAGS_RELEASE | FASTPATH_INPUT_EVENT_SCANCODE << 5;
	Stream_Write_UINT8(s, eventFlags); /* Key Release event (1 byte) */
	Stream_Write_UINT8(s, 0x0f); /* keyCode (1 byte) */

	/* send the toggle key states */
	eventFlags = (toggleStates & 0x1F) | FASTPATH_INPUT_EVENT_SYNC << 5;
	Stream_Write_UINT8(s, eventFlags); /* toggle state (1 byte) */

	/* send another tab up like mstsc.exe */
	eventFlags = FASTPATH_INPUT_KBDFLAGS_RELEASE | FASTPATH_INPUT_EVENT_SCANCODE << 5;
	Stream_Write_UINT8(s, eventFlags); /* Key Release event (1 byte) */
	Stream_Write_UINT8(s, 0x0f); /* keyCode (1 byte) */

	/* finish with a mouse pointer position like mstsc.exe */
	eventFlags = 0 | FASTPATH_INPUT_EVENT_MOUSE << 5;
	Stream_Write_UINT8(s, eventFlags); /* Mouse Pointer event (1 byte) */
	input_write_extended_mouse_event(s, PTR_FLAGS_MOVE, x, y);

	fastpath_send_multiple_input_pdu(rdp->fastpath, s, 4);
}

static BOOL input_recv_sync_event(rdpInput* input, wStream* s)
{
	UINT32 toggleFlags;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Seek(s, 2); /* pad2Octets (2 bytes) */
	Stream_Read_UINT32(s, toggleFlags); /* toggleFlags (4 bytes) */

	IFCALL(input->SynchronizeEvent, input, toggleFlags);

	return TRUE;
}

static BOOL input_recv_keyboard_event(rdpInput* input, wStream* s)
{
	UINT16 keyboardFlags, keyCode;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, keyboardFlags); /* keyboardFlags (2 bytes) */
	Stream_Read_UINT16(s, keyCode); /* keyCode (2 bytes) */
	Stream_Seek(s, 2); /* pad2Octets (2 bytes) */

	IFCALL(input->KeyboardEvent, input, keyboardFlags, keyCode);

	return TRUE;
}

static BOOL input_recv_unicode_keyboard_event(rdpInput* input, wStream* s)
{
	UINT16 keyboardFlags, unicodeCode;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, keyboardFlags); /* keyboardFlags (2 bytes) */
	Stream_Read_UINT16(s, unicodeCode); /* unicodeCode (2 bytes) */
	Stream_Seek(s, 2); /* pad2Octets (2 bytes) */

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

	return TRUE;
}

static BOOL input_recv_mouse_event(rdpInput* input, wStream* s)
{
	UINT16 pointerFlags, xPos, yPos;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, pointerFlags); /* pointerFlags (2 bytes) */
	Stream_Read_UINT16(s, xPos); /* xPos (2 bytes) */
	Stream_Read_UINT16(s, yPos); /* yPos (2 bytes) */

	IFCALL(input->MouseEvent, input, pointerFlags, xPos, yPos);

	return TRUE;
}

static BOOL input_recv_extended_mouse_event(rdpInput* input, wStream* s)
{
	UINT16 pointerFlags, xPos, yPos;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, pointerFlags); /* pointerFlags (2 bytes) */
	Stream_Read_UINT16(s, xPos); /* xPos (2 bytes) */
	Stream_Read_UINT16(s, yPos); /* yPos (2 bytes) */

	IFCALL(input->ExtendedMouseEvent, input, pointerFlags, xPos, yPos);

	return TRUE;
}

static BOOL input_recv_event(rdpInput* input, wStream* s)
{
	UINT16 messageType;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Seek(s, 4); /* eventTime (4 bytes), ignored by the server */
	Stream_Read_UINT16(s, messageType); /* messageType (2 bytes) */

	switch (messageType)
	{
		case INPUT_EVENT_SYNC:
			if (!input_recv_sync_event(input, s))
				return FALSE;
			break;

		case INPUT_EVENT_SCANCODE:
			if (!input_recv_keyboard_event(input, s))
				return FALSE;
			break;

		case INPUT_EVENT_UNICODE:
			if (!input_recv_unicode_keyboard_event(input, s))
				return FALSE;
			break;

		case INPUT_EVENT_MOUSE:
			if (!input_recv_mouse_event(input, s))
				return FALSE;
			break;

		case INPUT_EVENT_MOUSEX:
			if (!input_recv_extended_mouse_event(input, s))
				return FALSE;
			break;

		default:
			fprintf(stderr, "Unknown messageType %u\n", messageType);
			/* Each input event uses 6 bytes. */
			Stream_Seek(s, 6);
			break;
	}

	return TRUE;
}

BOOL input_recv(rdpInput* input, wStream* s)
{
	UINT16 i, numberEvents;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, numberEvents); /* numberEvents (2 bytes) */
	Stream_Seek(s, 2); /* pad2Octets (2 bytes) */

	/* Each input event uses 6 exactly bytes. */
	if (Stream_GetRemainingLength(s) < (size_t) (6 * numberEvents))
		return FALSE;

	for (i = 0; i < numberEvents; i++)
	{
		if (!input_recv_event(input, s))
			return FALSE;
	}

	return TRUE;
}

void input_register_client_callbacks(rdpInput* input)
{
	rdpSettings* settings = input->context->settings;

	if (settings->FastPathInput)
	{
		input->SynchronizeEvent = input_send_fastpath_synchronize_event;
		input->KeyboardEvent = input_send_fastpath_keyboard_event;
		input->UnicodeKeyboardEvent = input_send_fastpath_unicode_keyboard_event;
		input->MouseEvent = input_send_fastpath_mouse_event;
		input->ExtendedMouseEvent = input_send_fastpath_extended_mouse_event;
		input->FocusInEvent = input_send_fastpath_focus_in_event;
	}
	else
	{
		input->SynchronizeEvent = input_send_synchronize_event;
		input->KeyboardEvent = input_send_keyboard_event;
		input->UnicodeKeyboardEvent = input_send_unicode_keyboard_event;
		input->MouseEvent = input_send_mouse_event;
		input->ExtendedMouseEvent = input_send_extended_mouse_event;
		input->FocusInEvent = input_send_focus_in_event;
	}

	input->asynchronous = settings->AsyncInput;

	if (input->asynchronous)
	{
		input->proxy = input_message_proxy_new(input);
	}
}

void freerdp_input_send_synchronize_event(rdpInput* input, UINT32 flags)
{
	IFCALL(input->SynchronizeEvent, input, flags);
}

void freerdp_input_send_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	IFCALL(input->KeyboardEvent, input, flags, code);
}

void freerdp_input_send_keyboard_event_ex(rdpInput* input, BOOL down, UINT32 rdp_scancode)
{
	freerdp_input_send_keyboard_event(input,
			(RDP_SCANCODE_EXTENDED(rdp_scancode) ? KBD_FLAGS_EXTENDED : 0) |
			((down) ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE),
			RDP_SCANCODE_CODE(rdp_scancode));
}

void freerdp_input_send_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	IFCALL(input->UnicodeKeyboardEvent, input, flags, code);
}

void freerdp_input_send_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	IFCALL(input->MouseEvent, input, flags, x, y);
}

void freerdp_input_send_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	IFCALL(input->ExtendedMouseEvent, input, flags, x, y);
}

void freerdp_input_send_focus_in_event(rdpInput* input, UINT16 toggleStates, UINT16 x, UINT16 y)
{
	IFCALL(input->FocusInEvent, input, toggleStates, x, y);
}

int input_process_events(rdpInput* input)
{
	return input_message_queue_process_pending_messages(input);
}

static void input_free_queued_message(void *obj)
{
	wMessage *msg = (wMessage*)obj;
	input_message_queue_free_message(msg);
}

rdpInput* input_new(rdpRdp* rdp)
{
	const wObject cb = { NULL, NULL, NULL, input_free_queued_message, NULL };
	rdpInput* input;

	input = (rdpInput*) malloc(sizeof(rdpInput));

	if (input != NULL)
	{
		ZeroMemory(input, sizeof(rdpInput));

		input->queue = MessageQueue_New(&cb);
	}

	return input;
}

void input_free(rdpInput* input)
{
	if (input != NULL)
	{
		if (input->asynchronous)
			input_message_proxy_free(input->proxy);

		MessageQueue_Free(input->queue);

		free(input);
	}
}
