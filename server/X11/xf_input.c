/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <X11/Xlib.h>
#include <freerdp/locale/keyboard.h>

#include "xf_input.h"

void xf_input_synchronize_event(rdpInput* input, uint32 flags)
{
	printf("Client sent a synchronize event (flags:0x%X)\n", flags);
}

void xf_input_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
#ifdef WITH_XTEST
	unsigned int keycode;
	boolean extended = false;
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	xfInfo* xfi = xfp->info;

	if (flags & KBD_FLAGS_EXTENDED)
		extended = true;

	keycode = freerdp_keyboard_get_x11_keycode_from_rdp_scancode(code, extended);

	if (keycode != 0)
	{
		pthread_mutex_lock(&(xfp->mutex));

		XTestGrabControl(xfi->display, True);

		if (flags & KBD_FLAGS_DOWN)
			XTestFakeKeyEvent(xfi->display, keycode, True, 0);
		else if (flags & KBD_FLAGS_RELEASE)
			XTestFakeKeyEvent(xfi->display, keycode, False, 0);

		XTestGrabControl(xfi->display, False);

		pthread_mutex_unlock(&(xfp->mutex));
	}
#endif
}

void xf_input_unicode_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	printf("Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);
}

void xf_input_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
#ifdef WITH_XTEST
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	int button = 0;
	boolean down = false;
	xfInfo* xfi = xfp->info;

	pthread_mutex_lock(&(xfp->mutex));
	XTestGrabControl(xfi->display, True);

	if (flags & PTR_FLAGS_WHEEL)
	{
		boolean negative = false;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			negative = true;

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
			down = true;

		if (button != 0)
			XTestFakeButtonEvent(xfi->display, button, down, 0);
	}

	XTestGrabControl(xfi->display, False);
	pthread_mutex_unlock(&(xfp->mutex));
#endif
}

void xf_input_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
#ifdef WITH_XTEST
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	xfInfo* xfi = xfp->info;

	pthread_mutex_lock(&(xfp->mutex));
	XTestGrabControl(xfi->display, True);
	XTestFakeMotionEvent(xfi->display, 0, x, y, CurrentTime);
	XTestGrabControl(xfi->display, False);
	pthread_mutex_unlock(&(xfp->mutex));
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
