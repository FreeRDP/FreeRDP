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

#include "x11_shadow.h"

int x11_shadow_cursor_init(x11ShadowServer* server)
{
#ifdef WITH_XFIXES
	int event;
	int error;

	if (!XFixesQueryExtension(server->display, &event, &error))
	{
		fprintf(stderr, "XFixesQueryExtension failed\n");
		return -1;
	}

	server->xfixes_notify_event = event + XFixesCursorNotify;

	XFixesSelectCursorInput(server->display, DefaultRootWindow(server->display), XFixesDisplayCursorNotifyMask);
#endif

	return 0;
}

void x11_shadow_input_synchronize_event(rdpInput* input, UINT32 flags)
{
	fprintf(stderr, "Client sent a synchronize event (flags:0x%X)\n", flags);
}

void x11_shadow_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
#ifdef WITH_XTEST
	DWORD vkcode;
	DWORD keycode;
	BOOL extended = FALSE;
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	x11ShadowServer* server = xfp->server;

	if (flags & KBD_FLAGS_EXTENDED)
		extended = TRUE;

	if (extended)
		code |= KBDEXT;

	vkcode = GetVirtualKeyCodeFromVirtualScanCode(code, 4);
	keycode = GetKeycodeFromVirtualKeyCode(vkcode, KEYCODE_TYPE_EVDEV);

	if (keycode != 0)
	{
		XTestGrabControl(server->display, True);

		if (flags & KBD_FLAGS_DOWN)
			XTestFakeKeyEvent(server->display, keycode, True, 0);
		else if (flags & KBD_FLAGS_RELEASE)
			XTestFakeKeyEvent(server->display, keycode, False, 0);

		XTestGrabControl(server->display, False);
	}
#endif
}

void x11_shadow_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	fprintf(stderr, "Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);
}

void x11_shadow_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	int button = 0;
	BOOL down = FALSE;
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	x11ShadowServer* server = xfp->server;

	XTestGrabControl(server->display, True);

	if (flags & PTR_FLAGS_WHEEL)
	{
		BOOL negative = FALSE;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			negative = TRUE;

		button = (negative) ? 5 : 4;

		XTestFakeButtonEvent(server->display, button, True, 0);
		XTestFakeButtonEvent(server->display, button, False, 0);
	}
	else
	{
		if (flags & PTR_FLAGS_MOVE)
			XTestFakeMotionEvent(server->display, 0, x, y, 0);

		if (flags & PTR_FLAGS_BUTTON1)
			button = 1;
		else if (flags & PTR_FLAGS_BUTTON2)
			button = 3;
		else if (flags & PTR_FLAGS_BUTTON3)
			button = 2;

		if (flags & PTR_FLAGS_DOWN)
			down = TRUE;

		if (button != 0)
			XTestFakeButtonEvent(server->display, button, down, 0);
	}

	XTestGrabControl(server->display, False);
#endif
}

void x11_shadow_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	int button = 0;
	BOOL down = FALSE;
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	x11ShadowServer* server = xfp->server;

	XTestGrabControl(server->display, True);
	XTestFakeMotionEvent(server->display, 0, x, y, CurrentTime);

	if (flags & PTR_XFLAGS_BUTTON1)
		button = 8;
	else if (flags & PTR_XFLAGS_BUTTON2)
		button = 9;

	if (flags & PTR_XFLAGS_DOWN)
		down = TRUE;

	if (button != 0)
		XTestFakeButtonEvent(server->display, button, down, 0);

	XTestGrabControl(server->display, False);
#endif
}

void x11_shadow_input_register_callbacks(rdpInput* input)
{
	input->SynchronizeEvent = x11_shadow_input_synchronize_event;
	input->KeyboardEvent = x11_shadow_input_keyboard_event;
	input->UnicodeKeyboardEvent = x11_shadow_input_unicode_keyboard_event;
	input->MouseEvent = x11_shadow_input_mouse_event;
	input->ExtendedMouseEvent = x11_shadow_input_extended_mouse_event;
}
