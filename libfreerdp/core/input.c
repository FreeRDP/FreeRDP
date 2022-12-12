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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <freerdp/input.h>
#include <freerdp/log.h>

#include "message.h"

#include "input.h"

#define TAG FREERDP_TAG("core")

/* Input Events */
#define INPUT_EVENT_SYNC 0x0000
#define INPUT_EVENT_SCANCODE 0x0004
#define INPUT_EVENT_UNICODE 0x0005
#define INPUT_EVENT_MOUSE 0x8001
#define INPUT_EVENT_MOUSEX 0x8002

#define RDP_CLIENT_INPUT_PDU_HEADER_LENGTH 4

static void rdp_write_client_input_pdu_header(wStream* s, UINT16 number)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 4);
	Stream_Write_UINT16(s, number); /* numberEvents (2 bytes) */
	Stream_Write_UINT16(s, 0);      /* pad2Octets (2 bytes) */
}

static void rdp_write_input_event_header(wStream* s, UINT32 time, UINT16 type)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 6);
	Stream_Write_UINT32(s, time); /* eventTime (4 bytes) */
	Stream_Write_UINT16(s, type); /* messageType (2 bytes) */
}

static wStream* rdp_client_input_pdu_init(rdpRdp* rdp, UINT16 type)
{
	wStream* s;
	s = rdp_data_pdu_init(rdp);

	if (!s)
		return NULL;

	rdp_write_client_input_pdu_header(s, 1);
	rdp_write_input_event_header(s, 0, type);
	return s;
}

static BOOL rdp_send_client_input_pdu(rdpRdp* rdp, wStream* s)
{
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_INPUT, rdp->mcs->userId);
}

static void input_write_synchronize_event(wStream* s, UINT32 flags)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(Stream_GetRemainingCapacity(s) >= 6);
	Stream_Write_UINT16(s, 0);     /* pad2Octets (2 bytes) */
	Stream_Write_UINT32(s, flags); /* toggleFlags (4 bytes) */
}

static BOOL input_ensure_client_running(rdpInput* input)
{
	WINPR_ASSERT(input);
	if (freerdp_shall_disconnect_context(input->context))
	{
		WLog_WARN(TAG, "[APPLICATION BUG] input functions called after the session terminated");
		return FALSE;
	}
	return TRUE;
}

static BOOL input_send_synchronize_event(rdpInput* input, UINT32 flags)
{
	wStream* s;
	rdpRdp* rdp;

	if (!input || !input->context)
		return FALSE;

	rdp = input->context->rdp;

	if (!input_ensure_client_running(input))
		return FALSE;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_SYNC);

	if (!s)
		return FALSE;

	input_write_synchronize_event(s, flags);
	return rdp_send_client_input_pdu(rdp, s);
}

static void input_write_keyboard_event(wStream* s, UINT16 flags, UINT16 code)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(code <= UINT8_MAX);

	Stream_Write_UINT16(s, flags); /* keyboardFlags (2 bytes) */
	Stream_Write_UINT16(s, code);  /* keyCode (2 bytes) */
	Stream_Write_UINT16(s, 0);     /* pad2Octets (2 bytes) */
}

static BOOL input_send_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	wStream* s;
	rdpRdp* rdp;

	if (!input || !input->context)
		return FALSE;

	rdp = input->context->rdp;

	if (!input_ensure_client_running(input))
		return FALSE;

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_SCANCODE);

	if (!s)
		return FALSE;

	input_write_keyboard_event(s, flags, code);
	return rdp_send_client_input_pdu(rdp, s);
}

static void input_write_unicode_keyboard_event(wStream* s, UINT16 flags, UINT16 code)
{
	Stream_Write_UINT16(s, flags); /* keyboardFlags (2 bytes) */
	Stream_Write_UINT16(s, code);  /* unicodeCode (2 bytes) */
	Stream_Write_UINT16(s, 0);     /* pad2Octets (2 bytes) */
}

static BOOL input_send_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	wStream* s;
	rdpRdp* rdp;

	if (!input || !input->context)
		return FALSE;

	if (!input_ensure_client_running(input))
		return FALSE;

	if (!freerdp_settings_get_bool(input->context->settings, FreeRDP_UnicodeInput))
	{
		WLog_WARN(TAG, "Unicode input not supported by server.");
		return FALSE;
	}

	rdp = input->context->rdp;
	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_UNICODE);

	if (!s)
		return FALSE;

	input_write_unicode_keyboard_event(s, flags, code);
	return rdp_send_client_input_pdu(rdp, s);
}

static void input_write_mouse_event(wStream* s, UINT16 flags, UINT16 x, UINT16 y)
{
	Stream_Write_UINT16(s, flags); /* pointerFlags (2 bytes) */
	Stream_Write_UINT16(s, x);     /* xPos (2 bytes) */
	Stream_Write_UINT16(s, y);     /* yPos (2 bytes) */
}

static BOOL input_send_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	wStream* s;
	rdpRdp* rdp;

	if (!input || !input->context || !input->context->settings)
		return FALSE;

	rdp = input->context->rdp;

	if (!input_ensure_client_running(input))
		return FALSE;

	if (!freerdp_settings_get_bool(input->context->settings, FreeRDP_HasHorizontalWheel))
	{
		if (flags & PTR_FLAGS_HWHEEL)
		{
			WLog_WARN(TAG,
			          "skip mouse event %" PRIu16 "x%" PRIu16 " flags=0x%04" PRIX16
			          ", no horizontal mouse wheel supported",
			          x, y, flags);
			return TRUE;
		}
	}

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_MOUSE);

	if (!s)
		return FALSE;

	input_write_mouse_event(s, flags, x, y);
	return rdp_send_client_input_pdu(rdp, s);
}

static void input_write_extended_mouse_event(wStream* s, UINT16 flags, UINT16 x, UINT16 y)
{
	Stream_Write_UINT16(s, flags); /* pointerFlags (2 bytes) */
	Stream_Write_UINT16(s, x);     /* xPos (2 bytes) */
	Stream_Write_UINT16(s, y);     /* yPos (2 bytes) */
}

static BOOL input_send_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	wStream* s;
	rdpRdp* rdp;

	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);
	WINPR_ASSERT(input->context->settings);

	rdp = input->context->rdp;
	WINPR_ASSERT(rdp);

	if (!input_ensure_client_running(input))
		return FALSE;

	if (!freerdp_settings_get_bool(input->context->settings, FreeRDP_HasExtendedMouseEvent))
	{
		WLog_WARN(TAG,
		          "skip extended mouse event %" PRIu16 "x%" PRIu16 " flags=0x%04" PRIX16
		          ", no extended mouse events supported",
		          x, y, flags);
		return TRUE;
	}

	s = rdp_client_input_pdu_init(rdp, INPUT_EVENT_MOUSEX);

	if (!s)
		return FALSE;

	input_write_extended_mouse_event(s, flags, x, y);
	return rdp_send_client_input_pdu(rdp, s);
}

static BOOL input_send_focus_in_event(rdpInput* input, UINT16 toggleStates)
{
	/* send a tab up like mstsc.exe */
	if (!input_send_keyboard_event(input, KBD_FLAGS_RELEASE, 0x0f))
		return FALSE;

	/* send the toggle key states */
	if (!input_send_synchronize_event(input, (toggleStates & 0x1F)))
		return FALSE;

	/* send another tab up like mstsc.exe */
	return input_send_keyboard_event(input, KBD_FLAGS_RELEASE, 0x0f);
}

static BOOL input_send_keyboard_pause_event(rdpInput* input)
{
	/* In ancient days, pause-down without control sent E1 1D 45 E1 9D C5,
	 * and pause-up sent nothing.  However, reverse engineering mstsc shows
	 * it sending the following sequence:
	 */

	/* Control down (0x1D) */
	if (!input_send_keyboard_event(input, KBD_FLAGS_EXTENDED1,
	                               RDP_SCANCODE_CODE(RDP_SCANCODE_LCONTROL)))
		return FALSE;

	/* Numlock down (0x45) */
	if (!input_send_keyboard_event(input, 0, RDP_SCANCODE_CODE(RDP_SCANCODE_NUMLOCK)))
		return FALSE;

	/* Control up (0x1D) */
	if (!input_send_keyboard_event(input, KBD_FLAGS_RELEASE | KBD_FLAGS_EXTENDED1,
	                               RDP_SCANCODE_CODE(RDP_SCANCODE_LCONTROL)))
		return FALSE;

	/* Numlock up (0x45) */
	return input_send_keyboard_event(input, KBD_FLAGS_RELEASE,
	                                 RDP_SCANCODE_CODE(RDP_SCANCODE_NUMLOCK));
}

static BOOL input_send_fastpath_synchronize_event(rdpInput* input, UINT32 flags)
{
	wStream* s;
	rdpRdp* rdp;

	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);

	rdp = input->context->rdp;
	WINPR_ASSERT(rdp);

	if (!input_ensure_client_running(input))
		return FALSE;

	/* The FastPath Synchronization eventFlags has identical values as SlowPath */
	s = fastpath_input_pdu_init(rdp->fastpath, (BYTE)flags, FASTPATH_INPUT_EVENT_SYNC);

	if (!s)
		return FALSE;

	return fastpath_send_input_pdu(rdp->fastpath, s);
}

static BOOL input_send_fastpath_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	wStream* s;
	BYTE eventFlags = 0;
	rdpRdp* rdp;

	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);

	rdp = input->context->rdp;
	WINPR_ASSERT(rdp);

	if (!input_ensure_client_running(input))
		return FALSE;

	eventFlags |= (flags & KBD_FLAGS_RELEASE) ? FASTPATH_INPUT_KBDFLAGS_RELEASE : 0;
	eventFlags |= (flags & KBD_FLAGS_EXTENDED) ? FASTPATH_INPUT_KBDFLAGS_EXTENDED : 0;
	eventFlags |= (flags & KBD_FLAGS_EXTENDED1) ? FASTPATH_INPUT_KBDFLAGS_PREFIX_E1 : 0;
	s = fastpath_input_pdu_init(rdp->fastpath, eventFlags, FASTPATH_INPUT_EVENT_SCANCODE);

	if (!s)
		return FALSE;

	WINPR_ASSERT(code <= UINT8_MAX);
	Stream_Write_UINT8(s, (UINT8)code); /* keyCode (1 byte) */
	return fastpath_send_input_pdu(rdp->fastpath, s);
}

static BOOL input_send_fastpath_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	wStream* s;
	BYTE eventFlags = 0;
	rdpRdp* rdp;

	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);
	WINPR_ASSERT(input->context->settings);

	rdp = input->context->rdp;
	WINPR_ASSERT(rdp);

	if (!input_ensure_client_running(input))
		return FALSE;

	if (!freerdp_settings_get_bool(input->context->settings, FreeRDP_UnicodeInput))
	{
		WLog_WARN(TAG, "Unicode input not supported by server.");
		return FALSE;
	}

	eventFlags |= (flags & KBD_FLAGS_RELEASE) ? FASTPATH_INPUT_KBDFLAGS_RELEASE : 0;
	s = fastpath_input_pdu_init(rdp->fastpath, eventFlags, FASTPATH_INPUT_EVENT_UNICODE);

	if (!s)
		return FALSE;

	Stream_Write_UINT16(s, code); /* unicodeCode (2 bytes) */
	return fastpath_send_input_pdu(rdp->fastpath, s);
}

static BOOL input_send_fastpath_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	wStream* s;
	rdpRdp* rdp;

	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);
	WINPR_ASSERT(input->context->settings);

	rdp = input->context->rdp;
	WINPR_ASSERT(rdp);

	if (!input_ensure_client_running(input))
		return FALSE;

	if (!freerdp_settings_get_bool(input->context->settings, FreeRDP_HasHorizontalWheel))
	{
		if (flags & PTR_FLAGS_HWHEEL)
		{
			WLog_WARN(TAG,
			          "skip mouse event %" PRIu16 "x%" PRIu16 " flags=0x%04" PRIX16
			          ", no horizontal mouse wheel supported",
			          x, y, flags);
			return TRUE;
		}
	}

	s = fastpath_input_pdu_init(rdp->fastpath, 0, FASTPATH_INPUT_EVENT_MOUSE);

	if (!s)
		return FALSE;

	input_write_mouse_event(s, flags, x, y);
	return fastpath_send_input_pdu(rdp->fastpath, s);
}

static BOOL input_send_fastpath_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x,
                                                     UINT16 y)
{
	wStream* s;
	rdpRdp* rdp;

	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);

	rdp = input->context->rdp;
	WINPR_ASSERT(rdp);

	if (!input_ensure_client_running(input))
		return FALSE;

	if (!freerdp_settings_get_bool(input->context->settings, FreeRDP_HasExtendedMouseEvent))
	{
		WLog_WARN(TAG,
		          "skip extended mouse event %" PRIu16 "x%" PRIu16 " flags=0x%04" PRIX16
		          ", no extended mouse events supported",
		          x, y, flags);
		return TRUE;
	}

	s = fastpath_input_pdu_init(rdp->fastpath, 0, FASTPATH_INPUT_EVENT_MOUSEX);

	if (!s)
		return FALSE;

	input_write_extended_mouse_event(s, flags, x, y);
	return fastpath_send_input_pdu(rdp->fastpath, s);
}

static BOOL input_send_fastpath_focus_in_event(rdpInput* input, UINT16 toggleStates)
{
	wStream* s;
	BYTE eventFlags = 0;
	rdpRdp* rdp;

	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);

	rdp = input->context->rdp;
	WINPR_ASSERT(rdp);

	if (!input_ensure_client_running(input))
		return FALSE;

	s = fastpath_input_pdu_init_header(rdp->fastpath);

	if (!s)
		return FALSE;

	/* send a tab up like mstsc.exe */
	eventFlags = FASTPATH_INPUT_KBDFLAGS_RELEASE | FASTPATH_INPUT_EVENT_SCANCODE << 5;
	Stream_Write_UINT8(s, eventFlags); /* Key Release event (1 byte) */
	Stream_Write_UINT8(s, 0x0f);       /* keyCode (1 byte) */
	/* send the toggle key states */
	eventFlags = (toggleStates & 0x1F) | FASTPATH_INPUT_EVENT_SYNC << 5;
	Stream_Write_UINT8(s, eventFlags); /* toggle state (1 byte) */
	/* send another tab up like mstsc.exe */
	eventFlags = FASTPATH_INPUT_KBDFLAGS_RELEASE | FASTPATH_INPUT_EVENT_SCANCODE << 5;
	Stream_Write_UINT8(s, eventFlags); /* Key Release event (1 byte) */
	Stream_Write_UINT8(s, 0x0f);       /* keyCode (1 byte) */
	return fastpath_send_multiple_input_pdu(rdp->fastpath, s, 3);
}

static BOOL input_send_fastpath_keyboard_pause_event(rdpInput* input)
{
	/* In ancient days, pause-down without control sent E1 1D 45 E1 9D C5,
	 * and pause-up sent nothing.  However, reverse engineering mstsc shows
	 * it sending the following sequence:
	 */
	wStream* s;
	const BYTE keyDownEvent = FASTPATH_INPUT_EVENT_SCANCODE << 5;
	const BYTE keyUpEvent = (FASTPATH_INPUT_EVENT_SCANCODE << 5) | FASTPATH_INPUT_KBDFLAGS_RELEASE;
	rdpRdp* rdp;

	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);

	rdp = input->context->rdp;
	WINPR_ASSERT(rdp);

	if (!input_ensure_client_running(input))
		return FALSE;

	s = fastpath_input_pdu_init_header(rdp->fastpath);

	if (!s)
		return FALSE;

	/* Control down (0x1D) */
	Stream_Write_UINT8(s, keyDownEvent | FASTPATH_INPUT_KBDFLAGS_PREFIX_E1);
	Stream_Write_UINT8(s, RDP_SCANCODE_CODE(RDP_SCANCODE_LCONTROL));
	/* Numlock down (0x45) */
	Stream_Write_UINT8(s, keyDownEvent);
	Stream_Write_UINT8(s, RDP_SCANCODE_CODE(RDP_SCANCODE_NUMLOCK));
	/* Control up (0x1D) */
	Stream_Write_UINT8(s, keyUpEvent | FASTPATH_INPUT_KBDFLAGS_PREFIX_E1);
	Stream_Write_UINT8(s, RDP_SCANCODE_CODE(RDP_SCANCODE_LCONTROL));
	/* Numlock down (0x45) */
	Stream_Write_UINT8(s, keyUpEvent);
	Stream_Write_UINT8(s, RDP_SCANCODE_CODE(RDP_SCANCODE_NUMLOCK));
	return fastpath_send_multiple_input_pdu(rdp->fastpath, s, 4);
}

static BOOL input_recv_sync_event(rdpInput* input, wStream* s)
{
	UINT32 toggleFlags;

	WINPR_ASSERT(input);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	Stream_Seek(s, 2);                  /* pad2Octets (2 bytes) */
	Stream_Read_UINT32(s, toggleFlags); /* toggleFlags (4 bytes) */
	return IFCALLRESULT(TRUE, input->SynchronizeEvent, input, toggleFlags);
}

static BOOL input_recv_keyboard_event(rdpInput* input, wStream* s)
{
	UINT16 keyboardFlags, keyCode;

	WINPR_ASSERT(input);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	Stream_Read_UINT16(s, keyboardFlags); /* keyboardFlags (2 bytes) */
	Stream_Read_UINT16(s, keyCode);       /* keyCode (2 bytes) */
	Stream_Seek(s, 2);                    /* pad2Octets (2 bytes) */

	if (keyboardFlags & KBD_FLAGS_RELEASE)
		keyboardFlags &= ~KBD_FLAGS_DOWN;

	if (keyCode & 0xFF00)
		WLog_WARN(TAG,
		          "Problematic [MS-RDPBCGR] 2.2.8.1.1.3.1.1.1 Keyboard Event (TS_KEYBOARD_EVENT) "
		          "keyCode=0x%04" PRIx16
		          ", high byte values should be sent in keyboardFlags field, ignoring.",
		          keyCode);
	return IFCALLRESULT(TRUE, input->KeyboardEvent, input, keyboardFlags, keyCode & 0xFF);
}

static BOOL input_recv_unicode_keyboard_event(rdpInput* input, wStream* s)
{
	UINT16 keyboardFlags, unicodeCode;

	WINPR_ASSERT(input);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	Stream_Read_UINT16(s, keyboardFlags); /* keyboardFlags (2 bytes) */
	Stream_Read_UINT16(s, unicodeCode);   /* unicodeCode (2 bytes) */
	Stream_Seek(s, 2);                    /* pad2Octets (2 bytes) */

	/* "fix" keyboardFlags - see comment in input_recv_keyboard_event() */

	if (keyboardFlags & KBD_FLAGS_RELEASE)
		keyboardFlags &= ~KBD_FLAGS_DOWN;

	return IFCALLRESULT(TRUE, input->UnicodeKeyboardEvent, input, keyboardFlags, unicodeCode);
}

static BOOL input_recv_mouse_event(rdpInput* input, wStream* s)
{
	UINT16 pointerFlags, xPos, yPos;

	WINPR_ASSERT(input);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	Stream_Read_UINT16(s, pointerFlags); /* pointerFlags (2 bytes) */
	Stream_Read_UINT16(s, xPos);         /* xPos (2 bytes) */
	Stream_Read_UINT16(s, yPos);         /* yPos (2 bytes) */
	return IFCALLRESULT(TRUE, input->MouseEvent, input, pointerFlags, xPos, yPos);
}

static BOOL input_recv_extended_mouse_event(rdpInput* input, wStream* s)
{
	UINT16 pointerFlags, xPos, yPos;

	WINPR_ASSERT(input);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	Stream_Read_UINT16(s, pointerFlags); /* pointerFlags (2 bytes) */
	Stream_Read_UINT16(s, xPos);         /* xPos (2 bytes) */
	Stream_Read_UINT16(s, yPos);         /* yPos (2 bytes) */
	return IFCALLRESULT(TRUE, input->ExtendedMouseEvent, input, pointerFlags, xPos, yPos);
}

static BOOL input_recv_event(rdpInput* input, wStream* s)
{
	UINT16 messageType;

	WINPR_ASSERT(input);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	Stream_Seek(s, 4);                  /* eventTime (4 bytes), ignored by the server */
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
			WLog_ERR(TAG, "Unknown messageType %" PRIu16 "", messageType);
			/* Each input event uses 6 bytes. */
			Stream_Seek(s, 6);
			break;
	}

	return TRUE;
}

BOOL input_recv(rdpInput* input, wStream* s)
{
	UINT16 i, numberEvents;

	WINPR_ASSERT(input);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, numberEvents); /* numberEvents (2 bytes) */
	Stream_Seek(s, 2);                   /* pad2Octets (2 bytes) */

	/* Each input event uses 6 exactly bytes. */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6ull * numberEvents))
		return FALSE;

	for (i = 0; i < numberEvents; i++)
	{
		if (!input_recv_event(input, s))
			return FALSE;
	}

	return TRUE;
}

BOOL input_register_client_callbacks(rdpInput* input)
{
	rdpSettings* settings;

	if (!input->context)
		return FALSE;

	settings = input->context->settings;

	if (!settings)
		return FALSE;

	if (freerdp_settings_get_bool(settings, FreeRDP_FastPathInput))
	{
		input->SynchronizeEvent = input_send_fastpath_synchronize_event;
		input->KeyboardEvent = input_send_fastpath_keyboard_event;
		input->KeyboardPauseEvent = input_send_fastpath_keyboard_pause_event;
		input->UnicodeKeyboardEvent = input_send_fastpath_unicode_keyboard_event;
		input->MouseEvent = input_send_fastpath_mouse_event;
		input->ExtendedMouseEvent = input_send_fastpath_extended_mouse_event;
		input->FocusInEvent = input_send_fastpath_focus_in_event;
	}
	else
	{
		input->SynchronizeEvent = input_send_synchronize_event;
		input->KeyboardEvent = input_send_keyboard_event;
		input->KeyboardPauseEvent = input_send_keyboard_pause_event;
		input->UnicodeKeyboardEvent = input_send_unicode_keyboard_event;
		input->MouseEvent = input_send_mouse_event;
		input->ExtendedMouseEvent = input_send_extended_mouse_event;
		input->FocusInEvent = input_send_focus_in_event;
	}

	return TRUE;
}

BOOL freerdp_input_send_synchronize_event(rdpInput* input, UINT32 flags)
{
	if (!input || !input->context)
		return FALSE;

	if (freerdp_settings_get_bool(input->context->settings, FreeRDP_SuspendInput))
		return TRUE;

	return IFCALLRESULT(TRUE, input->SynchronizeEvent, input, flags);
}

BOOL freerdp_input_send_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	if (!input || !input->context)
		return FALSE;

	if (freerdp_settings_get_bool(input->context->settings, FreeRDP_SuspendInput))
		return TRUE;

	return IFCALLRESULT(TRUE, input->KeyboardEvent, input, flags, code);
}

BOOL freerdp_input_send_keyboard_event_ex(rdpInput* input, BOOL down, BOOL repeat,
                                          UINT32 rdp_scancode)
{
	UINT16 flags = (RDP_SCANCODE_EXTENDED(rdp_scancode) ? KBD_FLAGS_EXTENDED : 0);
	if (down && repeat)
		flags |= KBD_FLAGS_DOWN;
	else if (!down)
		flags |= KBD_FLAGS_RELEASE;

	return freerdp_input_send_keyboard_event(input, flags, RDP_SCANCODE_CODE(rdp_scancode));
}

BOOL freerdp_input_send_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	if (!input || !input->context)
		return FALSE;

	if (freerdp_settings_get_bool(input->context->settings, FreeRDP_SuspendInput))
		return TRUE;

	return IFCALLRESULT(TRUE, input->UnicodeKeyboardEvent, input, flags, code);
}

BOOL freerdp_input_send_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	if (!input || !input->context)
		return FALSE;

	if (freerdp_settings_get_bool(input->context->settings, FreeRDP_SuspendInput))
		return TRUE;

	return IFCALLRESULT(TRUE, input->MouseEvent, input, flags, x, y);
}

BOOL freerdp_input_send_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	if (!input || !input->context)
		return FALSE;

	if (freerdp_settings_get_bool(input->context->settings, FreeRDP_SuspendInput))
		return TRUE;

	return IFCALLRESULT(TRUE, input->ExtendedMouseEvent, input, flags, x, y);
}

BOOL freerdp_input_send_focus_in_event(rdpInput* input, UINT16 toggleStates)
{
	if (!input || !input->context)
		return FALSE;

	if (freerdp_settings_get_bool(input->context->settings, FreeRDP_SuspendInput))
		return TRUE;

	return IFCALLRESULT(TRUE, input->FocusInEvent, input, toggleStates);
}

BOOL freerdp_input_send_keyboard_pause_event(rdpInput* input)
{
	if (!input || !input->context)
		return FALSE;

	if (freerdp_settings_get_bool(input->context->settings, FreeRDP_SuspendInput))
		return TRUE;

	return IFCALLRESULT(TRUE, input->KeyboardPauseEvent, input);
}

int input_process_events(rdpInput* input)
{
	if (!input)
		return FALSE;

	return input_message_queue_process_pending_messages(input);
}

static void input_free_queued_message(void* obj)
{
	wMessage* msg = (wMessage*)obj;
	input_message_queue_free_message(msg);
}

rdpInput* input_new(rdpRdp* rdp)
{
	const wObject cb = { NULL, NULL, NULL, input_free_queued_message, NULL };
	rdp_input_internal* input = (rdp_input_internal*)calloc(1, sizeof(rdp_input_internal));

	WINPR_UNUSED(rdp);

	if (!input)
		return NULL;

	input->common.context = rdp->context;
	input->queue = MessageQueue_New(&cb);

	if (!input->queue)
	{
		free(input);
		return NULL;
	}

	return &input->common;
}

void input_free(rdpInput* input)
{
	if (input != NULL)
	{
		rdp_input_internal* in = input_cast(input);

		MessageQueue_Free(in->queue);
		free(in);
	}
}
