/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Input
 *
 * Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
 * Copyright 2015 David Fort <contact@hardening-consulting.com>
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

#include <stdlib.h>
#include <linux/input.h>

#include <freerdp/locale/keyboard.h>

#include "wlf_input.h"

BOOL wlf_handle_pointer_enter(freerdp *instance, UwacPointerEnterLeaveEvent *ev) {
	rdpInput* input = instance->input;

	return input->MouseEvent(input, PTR_FLAGS_MOVE, ev->x, ev->y);
}

BOOL wlf_handle_pointer_motion(freerdp *instance, UwacPointerMotionEvent *ev) {
	rdpInput* input = instance->input;

	return input->MouseEvent(input, PTR_FLAGS_MOVE, ev->x, ev->y);
}

BOOL wlf_handle_pointer_buttons(freerdp *instance, UwacPointerButtonEvent *ev) {
	rdpInput* input;
	UINT16 flags;

	input = instance->input;

	if (ev->state == WL_POINTER_BUTTON_STATE_PRESSED)
		flags = PTR_FLAGS_DOWN;
	else
		flags = 0;

	switch (ev->button)
	{
		case BTN_LEFT:
			flags |= PTR_FLAGS_BUTTON1;
			break;
		case BTN_RIGHT:
			flags |= PTR_FLAGS_BUTTON2;
			break;
		case BTN_MIDDLE:
			flags |= PTR_FLAGS_BUTTON3;
			break;
		default:
			return TRUE;
	}

	return input->MouseEvent(input, flags, ev->x, ev->y);
}


BOOL wlf_handle_pointer_axis(freerdp *instance, UwacPointerAxisEvent *ev) {
	rdpInput* input;
	UINT16 flags;
	int direction;

	input = instance->input;

	flags = PTR_FLAGS_WHEEL;

	if (ev->axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
	{
		direction = wl_fixed_to_int(ev->value);
		if (direction < 0)
			flags |= 0x0078;
		else
			flags |= PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;
	}

	return input->MouseEvent(input, flags, ev->x, ev->y);
}

BOOL wlf_handle_key(freerdp *instance, UwacKeyEvent *ev) {
	rdpInput* input = instance->input;
	DWORD rdp_scancode;

	rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(ev->raw_key + 8);

	if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
		return TRUE;

	return freerdp_input_send_keyboard_event_ex(input, ev->pressed, rdp_scancode);
}

BOOL wlf_keyboard_enter(freerdp *instance, UwacKeyboardEnterLeaveEvent *ev) {
	rdpInput* input = instance->input;

	return input->FocusInEvent(input, 0) &&
			input->MouseEvent(input, PTR_FLAGS_MOVE, 0, 0);

}
