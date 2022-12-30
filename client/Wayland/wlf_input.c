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
#include <float.h>

#include <linux/input.h>

#include <winpr/assert.h>

#include <freerdp/config.h>
#include <freerdp/locale/keyboard.h>
#if defined(CHANNEL_RDPEI_CLIENT)
#include <freerdp/client/rdpei.h>
#endif
#include <uwac/uwac.h>

#include "wlfreerdp.h"
#include "wlf_input.h"

#define TAG CLIENT_TAG("wayland.input")

static BOOL scale_signed_coordinates(rdpContext* context, int32_t* x, int32_t* y,
                                     BOOL fromLocalToRDP)
{
	BOOL rc;
	UINT32 ux;
	UINT32 uy;
	WINPR_ASSERT(context);
	WINPR_ASSERT(x);
	WINPR_ASSERT(y);
	WINPR_ASSERT(*x >= 0);
	WINPR_ASSERT(*y >= 0);

	ux = (UINT32)*x;
	uy = (UINT32)*y;
	rc = wlf_scale_coordinates(context, &ux, &uy, fromLocalToRDP);
	WINPR_ASSERT(ux < INT32_MAX);
	WINPR_ASSERT(uy < INT32_MAX);
	*x = (int32_t)ux;
	*y = (int32_t)uy;
	return rc;
}

BOOL wlf_handle_pointer_enter(freerdp* instance, const UwacPointerEnterLeaveEvent* ev)
{
	uint32_t x, y;
	rdpClientContext* cctx;

	if (!instance || !ev)
		return FALSE;

	x = ev->x;
	y = ev->y;

	if (!wlf_scale_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	WINPR_ASSERT(x <= UINT16_MAX);
	WINPR_ASSERT(y <= UINT16_MAX);
	cctx = (rdpClientContext*)instance->context;
	return freerdp_client_send_button_event(cctx, FALSE, PTR_FLAGS_MOVE, x, y);
}

BOOL wlf_handle_pointer_motion(freerdp* instance, const UwacPointerMotionEvent* ev)
{
	uint32_t x, y;
	rdpClientContext* cctx;

	if (!instance || !ev)
		return FALSE;

	cctx = (rdpClientContext*)instance->context;
	WINPR_ASSERT(cctx);

	x = ev->x;
	y = ev->y;

	if (!wlf_scale_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	WINPR_ASSERT(x <= UINT16_MAX);
	WINPR_ASSERT(y <= UINT16_MAX);
	return freerdp_client_send_button_event(cctx, FALSE, PTR_FLAGS_MOVE, x, y);
}

BOOL wlf_handle_pointer_buttons(freerdp* instance, const UwacPointerButtonEvent* ev)
{
	rdpClientContext* cctx;
	UINT16 flags = 0;
	UINT16 xflags = 0;
	uint32_t x, y;

	if (!instance || !ev)
		return FALSE;

	cctx = (rdpClientContext*)instance->context;
	WINPR_ASSERT(cctx);

	x = ev->x;
	y = ev->y;

	if (!wlf_scale_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

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

	WINPR_ASSERT(x <= UINT16_MAX);
	WINPR_ASSERT(y <= UINT16_MAX);

	if ((flags & ~PTR_FLAGS_DOWN) != 0)
		return freerdp_client_send_button_event(cctx, FALSE, flags, x, y);

	if ((xflags & ~PTR_XFLAGS_DOWN) != 0)
		return freerdp_client_send_extended_button_event(cctx, FALSE, xflags, x, y);

	return FALSE;
}

BOOL wlf_handle_pointer_axis(freerdp* instance, const UwacPointerAxisEvent* ev)
{
	wlfContext* context;
	if (!instance || !instance->context || !ev)
		return FALSE;

	context = (wlfContext*)instance->context;
	return ArrayList_Append(context->events, ev);
}

BOOL wlf_handle_pointer_axis_discrete(freerdp* instance, const UwacPointerAxisEvent* ev)
{
	wlfContext* context;
	if (!instance || !instance->context || !ev)
		return FALSE;

	context = (wlfContext*)instance->context;
	return ArrayList_Append(context->events, ev);
}

static BOOL wlf_handle_wheel(freerdp* instance, uint32_t x, uint32_t y, uint32_t axis,
                             int32_t value)
{
	rdpClientContext* cctx;
	UINT16 flags = 0;
	int32_t direction;
	uint32_t avalue = (uint32_t)abs(value);

	WINPR_ASSERT(instance);

	cctx = (rdpClientContext*)instance->context;
	WINPR_ASSERT(cctx);

	if (!wlf_scale_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	direction = value;
	switch (axis)
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

	/* Wheel rotation steps:
	 *
	 * positive: 0 ... 0xFF  -> slow ... fast
	 * negative: 0 ... 0xFF  -> fast ... slow
	 */

	while (avalue > 0)
	{
		const UINT16 cval = (avalue > 0xFF) ? 0xFF : (UINT16)avalue;
		UINT16 cflags = flags | cval;
		/* Convert negative values to 9bit twos complement */
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			cflags = (flags & 0xFF00) | (0x100 - cval);
		if (!freerdp_client_send_wheel_event(cctx, cflags))
			return FALSE;

		avalue -= cval;
	}
	return TRUE;
}

BOOL wlf_handle_pointer_frame(freerdp* instance, const UwacPointerFrameEvent* ev)
{
	BOOL success = TRUE;
	BOOL handle = FALSE;
	size_t x;
	wlfContext* context;
	enum wl_pointer_axis_source source = WL_POINTER_AXIS_SOURCE_CONTINUOUS;

	if (!instance || !ev || !instance->context)
		return FALSE;

	context = (wlfContext*)instance->context;

	for (x = 0; x < ArrayList_Count(context->events); x++)
	{
		UwacEvent* cev = ArrayList_GetItem(context->events, x);
		if (!cev)
			continue;
		if (cev->type == UWAC_EVENT_POINTER_SOURCE)
		{
			handle = TRUE;
			source = cev->mouse_source.axis_source;
		}
	}

	/* We need source events to determine how to interpret the data */
	if (handle)
	{
		for (x = 0; x < ArrayList_Count(context->events); x++)
		{
			UwacEvent* cev = ArrayList_GetItem(context->events, x);
			if (!cev)
				continue;

			switch (source)
			{
				/* If we have a mouse wheel, just use discrete data */
				case WL_POINTER_AXIS_SOURCE_WHEEL:
#if defined(WL_POINTER_AXIS_SOURCE_WHEEL_TILT_SINCE_VERSION)
				case WL_POINTER_AXIS_SOURCE_WHEEL_TILT:
#endif
					if (cev->type == UWAC_EVENT_POINTER_AXIS_DISCRETE)
					{
						/* Get the number of steps, multiply by default step width of 120 */
						int32_t val = cev->mouse_axis.value * 0x78;
						/* No wheel event received, success! */
						if (!wlf_handle_wheel(instance, cev->mouse_axis.x, cev->mouse_axis.y,
						                      cev->mouse_axis.axis, val))
							success = FALSE;
					}
					break;
					/* If we have a touch pad we get actual data, scale */
				case WL_POINTER_AXIS_SOURCE_FINGER:
				case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
					if (cev->type == UWAC_EVENT_POINTER_AXIS)
					{
						double dval = wl_fixed_to_double(cev->mouse_axis.value);
						int32_t val = (int32_t)(dval * 0x78 / 10.0);
						if (!wlf_handle_wheel(instance, cev->mouse_axis.x, cev->mouse_axis.y,
						                      cev->mouse_axis.axis, val))
							success = FALSE;
					}
					break;
				default:
					break;
			}
		}
	}
	ArrayList_Clear(context->events);
	return success;
}

BOOL wlf_handle_pointer_source(freerdp* instance, const UwacPointerSourceEvent* ev)
{
	wlfContext* context;
	if (!instance || !instance->context || !ev)
		return FALSE;

	context = (wlfContext*)instance->context;
	return ArrayList_Append(context->events, ev);
}

BOOL wlf_handle_key(freerdp* instance, const UwacKeyEvent* ev)
{
	rdpInput* input;
	DWORD rdp_scancode;

	if (!instance || !ev)
		return FALSE;

	WINPR_ASSERT(instance->context);
	if (instance->context->settings->GrabKeyboard && ev->raw_key == KEY_RIGHTCTRL)
		wlf_handle_ungrab_key(instance, ev);

	input = instance->context->input;
	rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(ev->raw_key + 8);

	if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
		return TRUE;

	return freerdp_input_send_keyboard_event_ex(input, ev->pressed, ev->repeated, rdp_scancode);
}

BOOL wlf_handle_ungrab_key(freerdp* instance, const UwacKeyEvent* ev)
{
	wlfContext* context;
	if (!instance || !instance->context || !ev)
		return FALSE;

	context = (wlfContext*)instance->context;

	return UwacSeatInhibitShortcuts(context->seat, false) == UWAC_SUCCESS;
}

BOOL wlf_keyboard_enter(freerdp* instance, const UwacKeyboardEnterLeaveEvent* ev)
{
	if (!instance || !ev)
		return FALSE;

	((wlfContext*)instance->context)->focusing = TRUE;
	return TRUE;
}

BOOL wlf_keyboard_modifiers(freerdp* instance, const UwacKeyboardModifiersEvent* ev)
{
	rdpInput* input;
	UINT16 syncFlags;
	wlfContext* wlf;

	if (!instance || !ev)
		return FALSE;

	wlf = (wlfContext*)instance->context;
	WINPR_ASSERT(wlf);

	input = instance->context->input;
	WINPR_ASSERT(input);

	syncFlags = 0;

	if (ev->modifiers & UWAC_MOD_CAPS_MASK)
		syncFlags |= KBD_SYNC_CAPS_LOCK;
	if (ev->modifiers & UWAC_MOD_NUM_MASK)
		syncFlags |= KBD_SYNC_NUM_LOCK;

	if (!wlf->focusing)
		return TRUE;

	((wlfContext*)instance->context)->focusing = FALSE;

	return freerdp_input_send_focus_in_event(input, syncFlags) &&
	       freerdp_client_send_button_event(&wlf->common, FALSE, PTR_FLAGS_MOVE, 0, 0);
}

BOOL wlf_handle_touch_up(freerdp* instance, const UwacTouchUp* ev)
{
	int32_t x, y;
	wlfContext* wlf;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(ev);
	wlf = instance->context;
	WINPR_ASSERT(wlf);

	x = ev->x;
	y = ev->y;

	if (!scale_signed_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	return freerdp_client_handle_touch(&wlf->common, FREERDP_TOUCH_UP, ev->id, 0, x, y);
}

BOOL wlf_handle_touch_down(freerdp* instance, const UwacTouchDown* ev)
{
	int32_t x, y;
	wlfContext* wlf;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(ev);
	wlf = instance->context;
	WINPR_ASSERT(wlf);

	x = ev->x;
	y = ev->y;

	if (!scale_signed_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	return freerdp_client_handle_touch(&wlf->common, FREERDP_TOUCH_DOWN, ev->id, 0, x, y);
}

BOOL wlf_handle_touch_motion(freerdp* instance, const UwacTouchMotion* ev)
{
	int32_t x, y;
	wlfContext* wlf;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(ev);
	wlf = instance->context;
	WINPR_ASSERT(wlf);

	x = ev->x;
	y = ev->y;

	if (!scale_signed_coordinates(instance->context, &x, &y, TRUE))
		return FALSE;

	return freerdp_client_handle_touch(&wlf->common, FREERDP_TOUCH_MOTION, 0, ev->id, x, y);
}
