/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/windows.h>

#include "wf_input.h"
#include "wf_info.h"

BOOL wf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	INPUT keyboard_event;
	WINPR_UNUSED(input);
	keyboard_event.type = INPUT_KEYBOARD;
	keyboard_event.ki.wVk = 0;
	keyboard_event.ki.wScan = code;
	keyboard_event.ki.dwFlags = KEYEVENTF_SCANCODE;
	keyboard_event.ki.dwExtraInfo = 0;
	keyboard_event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		keyboard_event.ki.dwFlags |= KEYEVENTF_KEYUP;

	if (flags & KBD_FLAGS_EXTENDED)
		keyboard_event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

	SendInput(1, &keyboard_event, sizeof(INPUT));
	return TRUE;
}

BOOL wf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	INPUT keyboard_event;
	WINPR_UNUSED(input);
	keyboard_event.type = INPUT_KEYBOARD;
	keyboard_event.ki.wVk = 0;
	keyboard_event.ki.wScan = code;
	keyboard_event.ki.dwFlags = KEYEVENTF_UNICODE;
	keyboard_event.ki.dwExtraInfo = 0;
	keyboard_event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		keyboard_event.ki.dwFlags |= KEYEVENTF_KEYUP;

	SendInput(1, &keyboard_event, sizeof(INPUT));
	return TRUE;
}

BOOL wf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	INPUT mouse_event = { 0 };
	float width, height;
	WINPR_UNUSED(input);

	WINPR_ASSERT(input);
	mouse_event.type = INPUT_MOUSE;

	if (flags & PTR_FLAGS_WHEEL)
	{
		mouse_event.mi.dwFlags = MOUSEEVENTF_WHEEL;
		mouse_event.mi.mouseData = flags & WheelRotationMask;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			mouse_event.mi.mouseData *= -1;

		SendInput(1, &mouse_event, sizeof(INPUT));
	}
	else
	{
		wfInfo* wfi;
		wfi = wf_info_get_instance();

		if (!wfi)
			return FALSE;

		// width and height of primary screen (even in multimon setups
		width = (float)GetSystemMetrics(SM_CXSCREEN);
		height = (float)GetSystemMetrics(SM_CYSCREEN);
		x += wfi->servscreen_xoffset;
		y += wfi->servscreen_yoffset;
		mouse_event.mi.dx = (LONG)((float)x * (65535.0f / width));
		mouse_event.mi.dy = (LONG)((float)y * (65535.0f / height));
		mouse_event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_MOVE)
		{
			mouse_event.mi.dwFlags |= MOUSEEVENTF_MOVE;
			SendInput(1, &mouse_event, sizeof(INPUT));
		}

		mouse_event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_BUTTON1)
		{
			if (flags & PTR_FLAGS_DOWN)
				mouse_event.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
			else
				mouse_event.mi.dwFlags |= MOUSEEVENTF_LEFTUP;

			SendInput(1, &mouse_event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			if (flags & PTR_FLAGS_DOWN)
				mouse_event.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			else
				mouse_event.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;

			SendInput(1, &mouse_event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			if (flags & PTR_FLAGS_DOWN)
				mouse_event.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			else
				mouse_event.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

			SendInput(1, &mouse_event, sizeof(INPUT));
		}
	}

	return TRUE;
}

BOOL wf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	if ((flags & PTR_XFLAGS_BUTTON1) || (flags & PTR_XFLAGS_BUTTON2))
	{
		INPUT mouse_event = { 0 };
		mouse_event.type = INPUT_MOUSE;

		if (flags & PTR_FLAGS_MOVE)
		{
			float width, height;
			wfInfo* wfi;
			wfi = wf_info_get_instance();

			if (!wfi)
				return FALSE;

			// width and height of primary screen (even in multimon setups
			width = (float)GetSystemMetrics(SM_CXSCREEN);
			height = (float)GetSystemMetrics(SM_CYSCREEN);
			x += wfi->servscreen_xoffset;
			y += wfi->servscreen_yoffset;
			mouse_event.mi.dx = (LONG)((float)x * (65535.0f / width));
			mouse_event.mi.dy = (LONG)((float)y * (65535.0f / height));
			mouse_event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			SendInput(1, &mouse_event, sizeof(INPUT));
		}

		mouse_event.mi.dx = mouse_event.mi.dy = mouse_event.mi.dwFlags = 0;

		if (flags & PTR_XFLAGS_DOWN)
			mouse_event.mi.dwFlags |= MOUSEEVENTF_XDOWN;
		else
			mouse_event.mi.dwFlags |= MOUSEEVENTF_XUP;

		if (flags & PTR_XFLAGS_BUTTON1)
			mouse_event.mi.mouseData = XBUTTON1;
		else if (flags & PTR_XFLAGS_BUTTON2)
			mouse_event.mi.mouseData = XBUTTON2;

		SendInput(1, &mouse_event, sizeof(INPUT));
	}
	else
	{
		wf_peer_mouse_event(input, flags, x, y);
	}

	return TRUE;
}

BOOL wf_peer_keyboard_event_dummy(rdpInput* input, UINT16 flags, UINT8 code)
{
	WINPR_UNUSED(input);
	WINPR_UNUSED(flags);
	WINPR_UNUSED(code);
	return TRUE;
}

BOOL wf_peer_unicode_keyboard_event_dummy(rdpInput* input, UINT16 flags, UINT16 code)
{
	WINPR_UNUSED(input);
	WINPR_UNUSED(flags);
	WINPR_UNUSED(code);
	return TRUE;
}

BOOL wf_peer_mouse_event_dummy(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	WINPR_UNUSED(input);
	WINPR_UNUSED(flags);
	WINPR_UNUSED(x);
	WINPR_UNUSED(y);
	return TRUE;
}

BOOL wf_peer_extended_mouse_event_dummy(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	WINPR_UNUSED(input);
	WINPR_UNUSED(flags);
	WINPR_UNUSED(x);
	WINPR_UNUSED(y);
	return TRUE;
}
