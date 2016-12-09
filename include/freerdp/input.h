/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input Interface API
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

#ifndef FREERDP_INPUT_H
#define FREERDP_INPUT_H

typedef struct rdp_input rdpInput;

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/scancode.h>

#include <winpr/crt.h>
#include <winpr/collections.h>

/* keyboard Flags */
#define KBD_FLAGS_EXTENDED		0x0100
#define KBD_FLAGS_DOWN			0x4000
#define KBD_FLAGS_RELEASE		0x8000

/* Pointer Flags */
#define PTR_FLAGS_HWHEEL		0x0400
#define PTR_FLAGS_WHEEL			0x0200
#define PTR_FLAGS_WHEEL_NEGATIVE	0x0100
#define PTR_FLAGS_MOVE			0x0800
#define PTR_FLAGS_DOWN			0x8000
#define PTR_FLAGS_BUTTON1		0x1000 /* left */
#define PTR_FLAGS_BUTTON2		0x2000 /* right */
#define PTR_FLAGS_BUTTON3		0x4000 /* middle */
#define WheelRotationMask		0x01FF

/* Extended Pointer Flags */
#define PTR_XFLAGS_DOWN			0x8000
#define PTR_XFLAGS_BUTTON1		0x0001
#define PTR_XFLAGS_BUTTON2		0x0002

/* Keyboard Toggle Flags */
#define KBD_SYNC_SCROLL_LOCK		0x00000001
#define KBD_SYNC_NUM_LOCK		0x00000002
#define KBD_SYNC_CAPS_LOCK		0x00000004
#define KBD_SYNC_KANA_LOCK		0x00000008

#define RDP_CLIENT_INPUT_PDU_HEADER_LENGTH	4

/* defined inside libfreerdp-core */
typedef struct rdp_input_proxy rdpInputProxy;

/* Input Interface */

typedef BOOL (*pSynchronizeEvent)(rdpInput* input, UINT32 flags);
typedef BOOL (*pKeyboardEvent)(rdpInput* input, UINT16 flags, UINT16 code);
typedef BOOL (*pUnicodeKeyboardEvent)(rdpInput* input, UINT16 flags, UINT16 code);
typedef BOOL (*pMouseEvent)(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
typedef BOOL (*pExtendedMouseEvent)(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
typedef BOOL (*pFocusInEvent)(rdpInput* input, UINT16 toggleStates);
typedef BOOL (*pKeyboardPauseEvent)(rdpInput* input);

struct rdp_input
{
	rdpContext* context; /* 0 */
	void* param1; /* 1 */
	UINT32 paddingA[16 - 2]; /* 2 */

	pSynchronizeEvent SynchronizeEvent; /* 16 */
	pKeyboardEvent KeyboardEvent; /* 17 */
	pUnicodeKeyboardEvent UnicodeKeyboardEvent; /* 18 */
	pMouseEvent MouseEvent; /* 19 */
	pExtendedMouseEvent ExtendedMouseEvent; /* 20 */
	pFocusInEvent FocusInEvent; /*21 */
	pKeyboardPauseEvent KeyboardPauseEvent; /* 22 */

	UINT32 paddingB[32 - 23]; /* 23 */

	/* Internal */

	BOOL asynchronous;
	rdpInputProxy* proxy;
	wMessageQueue* queue;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API BOOL freerdp_input_send_synchronize_event(rdpInput* input, UINT32 flags);
FREERDP_API BOOL freerdp_input_send_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code);
FREERDP_API BOOL freerdp_input_send_keyboard_event_ex(rdpInput* input, BOOL down, UINT32 rdp_scancode);
FREERDP_API BOOL freerdp_input_send_keyboard_pause_event(rdpInput* input);
FREERDP_API BOOL freerdp_input_send_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code);
FREERDP_API BOOL freerdp_input_send_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
FREERDP_API BOOL freerdp_input_send_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
FREERDP_API BOOL freerdp_input_send_focus_in_event(rdpInput* input, UINT16 toggleStates);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_INPUT_H */
