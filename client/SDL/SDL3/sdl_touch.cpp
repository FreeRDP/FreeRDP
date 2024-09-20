/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP SDL touch/mouse input
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#include "sdl_touch.hpp"
#include "sdl_freerdp.hpp"

#include <winpr/wtypes.h>
#include <winpr/assert.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <SDL3/SDL.h>

BOOL sdl_scale_coordinates(SdlContext* sdl, Uint32 windowId, INT32* px, INT32* py,
                           BOOL fromLocalToRDP, BOOL applyOffset)
{
	rdpGdi* gdi = nullptr;
	double sx = 1.0;
	double sy = 1.0;

	if (!sdl || !px || !py || !sdl->context()->gdi)
		return FALSE;

	WINPR_ASSERT(sdl->context()->gdi);
	WINPR_ASSERT(sdl->context()->settings);

	gdi = sdl->context()->gdi;

	// TODO: Make this multimonitor ready!
	// TODO: Need to find the primary monitor, get the scale
	// TODO: Need to find the destination monitor, get the scale
	// TODO: All intermediate monitors, get the scale

	int offset_x = 0;
	int offset_y = 0;
	for (const auto& it : sdl->windows)
	{
		auto& window = it.second;
		const auto id = window.id();
		if (id != windowId)
		{
			continue;
		}

		auto size = window.rect();

		sx = size.w / static_cast<double>(gdi->width);
		sy = size.h / static_cast<double>(gdi->height);
		offset_x = window.offsetX();
		offset_y = window.offsetY();
		break;
	}

	if (freerdp_settings_get_bool(sdl->context()->settings, FreeRDP_SmartSizing))
	{
		if (!fromLocalToRDP)
		{
			*px = static_cast<INT32>(*px * sx);
			*py = static_cast<INT32>(*py * sy);
		}
		else
		{
			*px = static_cast<INT32>(*px / sx);
			*py = static_cast<INT32>(*py / sy);
		}
	}
	else if (applyOffset)
	{
		*px -= offset_x;
		*py -= offset_y;
	}

	return TRUE;
}

static BOOL sdl_get_touch_scaled(SdlContext* sdl, const SDL_TouchFingerEvent* ev, INT32* px,
                                 INT32* py, BOOL local)
{
	Uint32 windowID = 0;

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(ev);
	WINPR_ASSERT(px);
	WINPR_ASSERT(py);

	SDL_Window* window = SDL_GetWindowFromID(ev->windowID);

	if (!window)
		return FALSE;

	windowID = SDL_GetWindowID(window);
	SDL_Surface* surface = SDL_GetWindowSurface(window);
	if (!surface)
		return FALSE;

	// TODO: Add the offset of the surface in the global coordinates
	*px = static_cast<INT32>(ev->x * static_cast<float>(surface->w));
	*py = static_cast<INT32>(ev->y * static_cast<float>(surface->h));
	return sdl_scale_coordinates(sdl, windowID, px, py, local, TRUE);
}

static BOOL send_mouse_wheel(SdlContext* sdl, UINT16 flags, INT32 avalue)
{
	WINPR_ASSERT(sdl);
	if (avalue < 0)
	{
		flags |= PTR_FLAGS_WHEEL_NEGATIVE;
		avalue = -avalue;
	}

	while (avalue > 0)
	{
		const UINT16 cval = (avalue > 0xFF) ? 0xFF : static_cast<UINT16>(avalue);
		UINT16 cflags = flags | cval;
		/* Convert negative values to 9bit twos complement */
		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			cflags = (flags & 0xFF00) | (0x100 - cval);
		if (!freerdp_client_send_wheel_event(sdl->common(), cflags))
			return FALSE;

		avalue -= cval;
	}
	return TRUE;
}

static UINT32 sdl_scale_pressure(const float pressure)
{
	const float val = pressure * 0x400; /* [MS-RDPEI] 2.2.3.3.1.1 RDPINPUT_TOUCH_CONTACT */
	if (val < 0.0f)
		return 0;
	if (val > 0x400)
		return 0x400;
	return static_cast<UINT32>(val);
}

BOOL sdl_handle_touch_up(SdlContext* sdl, const SDL_TouchFingerEvent* ev)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(ev);

	INT32 x = 0;
	INT32 y = 0;
	if (!sdl_get_touch_scaled(sdl, ev, &x, &y, TRUE))
		return FALSE;
	return freerdp_client_handle_touch(sdl->common(), FREERDP_TOUCH_UP | FREERDP_TOUCH_HAS_PRESSURE,
	                                   static_cast<INT32>(ev->fingerID),
	                                   sdl_scale_pressure(ev->pressure), x, y);
}

BOOL sdl_handle_touch_down(SdlContext* sdl, const SDL_TouchFingerEvent* ev)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(ev);

	INT32 x = 0;
	INT32 y = 0;
	if (!sdl_get_touch_scaled(sdl, ev, &x, &y, TRUE))
		return FALSE;
	return freerdp_client_handle_touch(
	    sdl->common(), FREERDP_TOUCH_DOWN | FREERDP_TOUCH_HAS_PRESSURE,
	    static_cast<INT32>(ev->fingerID), sdl_scale_pressure(ev->pressure), x, y);
}

BOOL sdl_handle_touch_motion(SdlContext* sdl, const SDL_TouchFingerEvent* ev)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(ev);

	INT32 x = 0;
	INT32 y = 0;
	if (!sdl_get_touch_scaled(sdl, ev, &x, &y, TRUE))
		return FALSE;
	return freerdp_client_handle_touch(
	    sdl->common(), FREERDP_TOUCH_MOTION | FREERDP_TOUCH_HAS_PRESSURE,
	    static_cast<INT32>(ev->fingerID), sdl_scale_pressure(ev->pressure), x, y);
}

BOOL sdl_handle_mouse_motion(SdlContext* sdl, const SDL_MouseMotionEvent* ev)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(ev);

	sdl->input.mouse_focus(ev->windowID);
	const BOOL relative = freerdp_client_use_relative_mouse_events(sdl->common());
	INT32 x = relative ? ev->xrel : ev->x;
	INT32 y = relative ? ev->yrel : ev->y;
	sdl_scale_coordinates(sdl, ev->windowID, &x, &y, TRUE, TRUE);
	return freerdp_client_send_button_event(sdl->common(), relative, PTR_FLAGS_MOVE, x, y);
}

BOOL sdl_handle_mouse_wheel(SdlContext* sdl, const SDL_MouseWheelEvent* ev)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(ev);

	const BOOL flipped = (ev->direction == SDL_MOUSEWHEEL_FLIPPED);
	const INT32 x = ev->x * (flipped ? -1 : 1) * 0x78;
	const INT32 y = ev->y * (flipped ? -1 : 1) * 0x78;
	UINT16 flags = 0;

	if (y != 0)
	{
		flags |= PTR_FLAGS_WHEEL;
		send_mouse_wheel(sdl, flags, y);
	}

	if (x != 0)
	{
		flags |= PTR_FLAGS_HWHEEL;
		send_mouse_wheel(sdl, flags, x);
	}
	return TRUE;
}

BOOL sdl_handle_mouse_button(SdlContext* sdl, const SDL_MouseButtonEvent* ev)
{
	UINT16 flags = 0;
	UINT16 xflags = 0;

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(ev);

	if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
	{
		flags |= PTR_FLAGS_DOWN;
		xflags |= PTR_XFLAGS_DOWN;
	}

	switch (ev->button)
	{
		case 1:
			flags |= PTR_FLAGS_BUTTON1;
			break;
		case 2:
			flags |= PTR_FLAGS_BUTTON3;
			break;
		case 3:
			flags |= PTR_FLAGS_BUTTON2;
			break;
		case 4:
			xflags |= PTR_XFLAGS_BUTTON1;
			break;
		case 5:
			xflags |= PTR_XFLAGS_BUTTON2;
			break;
		default:
			break;
	}

	const BOOL relative = freerdp_client_use_relative_mouse_events(sdl->common());
	INT32 x = relative ? 0 : ev->x;
	INT32 y = relative ? 0 : ev->y;
	sdl_scale_coordinates(sdl, ev->windowID, &x, &y, TRUE, TRUE);
	if ((flags & (~PTR_FLAGS_DOWN)) != 0)
		return freerdp_client_send_button_event(sdl->common(), relative, flags, x, y);
	else if ((xflags & (~PTR_XFLAGS_DOWN)) != 0)
		return freerdp_client_send_extended_button_event(sdl->common(), relative, xflags, x, y);
	else
		return FALSE;
}
