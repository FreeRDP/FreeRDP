/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Server Input
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

#include <X11/Xlib.h>

#include <winpr/crt.h>
#include <winpr/input.h>

#include "x11_shadow.h"

void x11_shadow_input_synchronize_event(x11ShadowSubsystem* subsystem, UINT32 flags)
{
	fprintf(stderr, "Client sent a synchronize event (flags:0x%X)\n", flags);
}

void x11_shadow_input_keyboard_event(x11ShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
#ifdef WITH_XTEST
	DWORD vkcode;
	DWORD keycode;
	BOOL extended = FALSE;

	if (flags & KBD_FLAGS_EXTENDED)
		extended = TRUE;

	if (extended)
		code |= KBDEXT;

	vkcode = GetVirtualKeyCodeFromVirtualScanCode(code, 4);
	keycode = GetKeycodeFromVirtualKeyCode(vkcode, KEYCODE_TYPE_EVDEV);

	if (keycode != 0)
	{
		XTestGrabControl(subsystem->display, True);

		if (flags & KBD_FLAGS_DOWN)
			XTestFakeKeyEvent(subsystem->display, keycode, True, 0);
		else if (flags & KBD_FLAGS_RELEASE)
			XTestFakeKeyEvent(subsystem->display, keycode, False, 0);

		XTestGrabControl(subsystem->display, False);
	}
#endif
}

void x11_shadow_input_unicode_keyboard_event(x11ShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
	fprintf(stderr, "Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);
}

void x11_shadow_input_mouse_event(x11ShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	int button = 0;
	BOOL down = FALSE;

	XTestGrabControl(subsystem->display, True);

	if (flags & PTR_FLAGS_WHEEL)
	{
		BOOL negative = FALSE;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			negative = TRUE;

		button = (negative) ? 5 : 4;

		XTestFakeButtonEvent(subsystem->display, button, True, 0);
		XTestFakeButtonEvent(subsystem->display, button, False, 0);
	}
	else
	{
		if (flags & PTR_FLAGS_MOVE)
			XTestFakeMotionEvent(subsystem->display, 0, x, y, 0);

		if (flags & PTR_FLAGS_BUTTON1)
			button = 1;
		else if (flags & PTR_FLAGS_BUTTON2)
			button = 3;
		else if (flags & PTR_FLAGS_BUTTON3)
			button = 2;

		if (flags & PTR_FLAGS_DOWN)
			down = TRUE;

		if (button != 0)
			XTestFakeButtonEvent(subsystem->display, button, down, 0);
	}

	XTestGrabControl(subsystem->display, False);
#endif
}

void x11_shadow_input_extended_mouse_event(x11ShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	int button = 0;
	BOOL down = FALSE;

	XTestGrabControl(subsystem->display, True);
	XTestFakeMotionEvent(subsystem->display, 0, x, y, CurrentTime);

	if (flags & PTR_XFLAGS_BUTTON1)
		button = 8;
	else if (flags & PTR_XFLAGS_BUTTON2)
		button = 9;

	if (flags & PTR_XFLAGS_DOWN)
		down = TRUE;

	if (button != 0)
		XTestFakeButtonEvent(subsystem->display, button, down, 0);

	XTestGrabControl(subsystem->display, False);
#endif
}

