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

#include "wlfreerdp.h"
#include "wlf_input.h"

BOOL wlf_handle_pointer_enter(freerdp* instance, const UwacPointerEnterLeaveEvent* ev)
{
	uint32_t x, y;

	if (!instance || !ev || !instance->input)
		return FALSE;

	x = ev->x;
	y = ev->y;

	if (!wlf_scale_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	return freerdp_input_send_mouse_event(instance->input, PTR_FLAGS_MOVE, x, y);
}

BOOL wlf_handle_pointer_motion(freerdp* instance, const UwacPointerMotionEvent* ev)
{
	uint32_t x, y;

	if (!instance || !ev || !instance->input)
		return FALSE;

	x = ev->x;
	y = ev->y;

	if (!wlf_scale_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	return freerdp_input_send_mouse_event(instance->input, PTR_FLAGS_MOVE, x, y);
}

BOOL wlf_handle_pointer_buttons(freerdp* instance, const UwacPointerButtonEvent* ev)
{
	rdpInput* input;
	UINT16 flags = 0;
	UINT16 xflags = 0;
	uint32_t x, y;

	if (!instance || !ev || !instance->input)
		return FALSE;

	x = ev->x;
	y = ev->y;

	if (!wlf_scale_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	input = instance->input;

	if (ev->state == WL_POINTER_BUTTON_STATE_PRESSED)
	{
		flags |= PTR_FLAGS_DOWN;
		xflags |= PTR_XFLAGS_DOWN;
	}

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

		case BTN_SIDE:
			xflags |= PTR_XFLAGS_BUTTON1;
			break;

		case BTN_EXTRA:
			xflags |= PTR_XFLAGS_BUTTON2;
			break;

		default:
			return TRUE;
	}

	if ((flags & ~PTR_FLAGS_DOWN) != 0)
		return freerdp_input_send_mouse_event(input, flags, x, y);

	if ((xflags & ~PTR_XFLAGS_DOWN) != 0)
		return freerdp_input_send_extended_mouse_event(input, xflags, x, y);

	return FALSE;
}


BOOL wlf_handle_pointer_axis(freerdp* instance, const UwacPointerAxisEvent* ev)
{
	rdpInput* input;
	UINT16 flags = 0;
	int direction;
	uint32_t step;
	uint32_t x, y;

	if (!instance || !ev || !instance->input)
		return FALSE;

	x = ev->x;
	y = ev->y;

	if (!wlf_scale_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	input = instance->input;

	direction = wl_fixed_to_int(ev->value);
	switch (ev->axis)
	{
		case WL_POINTER_AXIS_VERTICAL_SCROLL:
			flags |= PTR_FLAGS_WHEEL;
			if (direction > 0)
				flags |= PTR_FLAGS_WHEEL_NEGATIVE;
			break;

		case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
			flags |= PTR_FLAGS_HWHEEL;
			if (direction < 0)
				flags |= PTR_FLAGS_WHEEL_NEGATIVE;
			break;

		default:
			return FALSE;
	}

	step = (uint32_t)abs(direction);
	if (step > WheelRotationMask)
		step = WheelRotationMask;
	flags |=  step;

	return freerdp_input_send_mouse_event(input, flags, (UINT16)x, (UINT16)y);
}

BOOL wlf_handle_key(freerdp* instance, const UwacKeyEvent* ev)
{
	rdpInput* input;
	DWORD rdp_scancode;

	if (!instance || !ev || !instance->input)
		return FALSE;

	input = instance->input;
	rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(ev->raw_key + 8);

	if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
		return TRUE;

	return freerdp_input_send_keyboard_event_ex(input, ev->pressed, rdp_scancode);
}

BOOL wlf_keyboard_enter(freerdp* instance, const UwacKeyboardEnterLeaveEvent* ev)
{
	rdpInput* input;

	if (!instance || !ev || !instance->input)
		return FALSE;

	input = instance->input;
	return freerdp_input_send_focus_in_event(input, 0) &&
	       freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, 0, 0);
}
