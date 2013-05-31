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

#include <freerdp/locale/keyboard.h>

#include <winpr/crt.h>
#include <winpr/input.h>

#include "xf_peer.h"

#include "xf_input.h"

void xf_input_synchronize_event(rdpInput* input, UINT32 flags)
{
	fprintf(stderr, "Client sent a synchronize event (flags:0x%X)\n", flags);
}

void xf_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
#ifdef WITH_XTEST
	DWORD vkcode;
	DWORD keycode;
	BOOL extended = FALSE;
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	xfInfo* xfi = xfp->info;

	if (flags & KBD_FLAGS_EXTENDED)
		extended = TRUE;

	if (extended)
		code |= KBDEXT;

	vkcode = GetVirtualKeyCodeFromVirtualScanCode(code, 4);
	keycode = GetKeycodeFromVirtualKeyCode(vkcode, KEYCODE_TYPE_EVDEV);

	if (keycode != 0)
	{
		XTestGrabControl(xfi->display, True);

		if (flags & KBD_FLAGS_DOWN)
			XTestFakeKeyEvent(xfi->display, keycode, True, 0);
		else if (flags & KBD_FLAGS_RELEASE)
			XTestFakeKeyEvent(xfi->display, keycode, False, 0);

		XTestGrabControl(xfi->display, False);
	}
#endif
}

void xf_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	fprintf(stderr, "Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);
}

void xf_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	int button = 0;
	BOOL down = FALSE;
	xfInfo* xfi = xfp->info;

	XTestGrabControl(xfi->display, True);

	if (flags & PTR_FLAGS_WHEEL)
	{
		BOOL negative = FALSE;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			negative = TRUE;

		button = (negative) ? 5 : 4;

		XTestFakeButtonEvent(xfi->display, button, True, 0);
		XTestFakeButtonEvent(xfi->display, button, False, 0);
	}
	else
	{
		if (flags & PTR_FLAGS_MOVE)
			XTestFakeMotionEvent(xfi->display, 0, x, y, 0);

		if (flags & PTR_FLAGS_BUTTON1)
			button = 1;
		else if (flags & PTR_FLAGS_BUTTON2)
			button = 3;
		else if (flags & PTR_FLAGS_BUTTON3)
			button = 2;

		if (flags & PTR_FLAGS_DOWN)
			down = TRUE;

		if (button != 0)
			XTestFakeButtonEvent(xfi->display, button, down, 0);
	}

	XTestGrabControl(xfi->display, False);
#endif
}

void xf_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	xfInfo* xfi = xfp->info;

	XTestGrabControl(xfi->display, True);
	XTestFakeMotionEvent(xfi->display, 0, x, y, CurrentTime);
	XTestGrabControl(xfi->display, False);
#endif
}

void xf_input_register_callbacks(rdpInput* input)
{
	input->SynchronizeEvent = xf_input_synchronize_event;
	input->KeyboardEvent = xf_input_keyboard_event;
	input->UnicodeKeyboardEvent = xf_input_unicode_keyboard_event;
	input->MouseEvent = xf_input_mouse_event;
	input->ExtendedMouseEvent = xf_input_extended_mouse_event;
}
