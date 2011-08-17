/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Event Handling
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

#include <freerdp/kbd/kbd.h>
#include <freerdp/kbd/vkcodes.h>

#include "xf_event.h"

void xf_send_mouse_motion_event(rdpInput* input, boolean down, uint32 button, uint16 x, uint16 y)
{
	input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);
}

boolean xf_event_Expose(xfInfo* xfi, XEvent* event, boolean app)
{
	int x;
	int y;
	int cx;
	int cy;

	if (event->xexpose.window == xfi->window->handle)
	{
		x = event->xexpose.x;
		y = event->xexpose.y;
		cx = event->xexpose.width;
		cy = event->xexpose.height;
		XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc_default, x, y, cx, cy, x, y);
	}

	return True;
}

boolean xf_event_VisibilityNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xvisibility.window == xfi->window->handle)
		xfi->unobscured = event->xvisibility.state == VisibilityUnobscured;

	return True;
}

boolean xf_event_MotionNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpInput* input;

	input = xfi->instance->input;

	if (event->xmotion.window == xfi->window->handle)
	{
		if (xfi->mouse_motion != True)
		{
			if ((event->xmotion.state & (Button1Mask | Button2Mask | Button3Mask)) == 0)
				return True;
		}

		input->MouseEvent(input, PTR_FLAGS_MOVE, event->xmotion.x, event->xmotion.y);
	}

	if (xfi->fullscreen)
		XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);

	return True;
}

boolean xf_event_ButtonPress(xfInfo* xfi, XEvent* event, boolean app)
{
	uint16 x, y;
	uint16 flags;
	boolean wheel;
	rdpInput* input;

	input = xfi->instance->input;

	x = 0;
	y = 0;
	flags = 0;
	wheel = False;

	switch (event->xbutton.button)
	{
		case 1:
			x = event->xmotion.x;
			y = event->xmotion.y;
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1;
			break;

		case 2:
			x = event->xmotion.x;
			y = event->xmotion.y;
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3;
			break;

		case 3:
			x = event->xmotion.x;
			y = event->xmotion.y;
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2;
			break;

		case 4:
			wheel = True;
			flags = PTR_FLAGS_WHEEL | 0x0078;
			break;

		case 5:
			wheel = True;
			flags = PTR_FLAGS_WHEEL | 0x0088;
			break;

		default:
			x = 0;
			y = 0;
			flags = 0;
			break;
	}

	if (flags != 0)
	{
		if (wheel)
		{
			input->MouseEvent(input, flags, 0, 0);
		}
		else
		{
			if (app)
			{

			}

			input->MouseEvent(input, flags, x, y);
		}
	}

	return True;
}

boolean xf_event_ButtonRelease(xfInfo* xfi, XEvent* event, boolean app)
{
	uint16 flags;
	rdpInput* input;

	input = xfi->instance->input;

	flags = 0;

	switch (event->xbutton.button)
	{
		case 1:
			flags = PTR_FLAGS_BUTTON1;
			break;

		case 2:
			flags = PTR_FLAGS_BUTTON3;
			break;

		case 3:
			flags = PTR_FLAGS_BUTTON2;
			break;

		default:
			flags = 0;
			break;
	}

	if (flags != 0)
	{
		if (app)
		{

		}

		input->MouseEvent(input, flags, event->xbutton.x, event->xbutton.y);
	}

	return True;
}

boolean xf_event_KeyPress(xfInfo* xfi, XEvent* event, boolean app)
{
	KeySym keysym;
	char str[256];

	XLookupString((XKeyEvent*) event, str, sizeof(str), &keysym, NULL);

	xf_kbd_set_keypress(xfi, event->xkey.keycode, keysym);

	if (xfi->fullscreen_toggle && xf_kbd_handle_special_keys(xfi, keysym))
		return True;

	xf_kbd_send_key(xfi, True, event->xkey.keycode);

	return True;
}

boolean xf_event_KeyRelease(xfInfo* xfi, XEvent* event, boolean app)
{
	XEvent next_event;

	if (XPending(xfi->display))
	{
		memset(&next_event, 0, sizeof(next_event));
		XPeekEvent(xfi->display, &next_event);

		if (next_event.type == KeyPress)
		{
			if (next_event.xkey.keycode == event->xkey.keycode)
				return True;
		}
	}

	xf_kbd_unset_keypress(xfi, event->xkey.keycode);
	xf_kbd_send_key(xfi, False, event->xkey.keycode);

	return True;
}

boolean xf_event_FocusIn(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xfocus.mode == NotifyGrab)
		return True;

	xfi->focused = True;

	if (xfi->mouse_active)
		XGrabKeyboard(xfi->display, xfi->window->handle, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	xf_kbd_focus_in(xfi);

	return True;
}

boolean xf_event_FocusOut(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xfocus.mode == NotifyUngrab)
		return True;

	xfi->focused = False;

	if (event->xfocus.mode == NotifyWhileGrabbed)
		XUngrabKeyboard(xfi->display, CurrentTime);

	return True;
}

boolean xf_event_MappingNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xmapping.request == MappingModifier)
	{
		XFreeModifiermap(xfi->modifier_map);
		xfi->modifier_map = XGetModifierMapping(xfi->display);
	}

	return True;
}

boolean xf_event_ClientMessage(xfInfo* xfi, XEvent* event, boolean app)
{
	Atom kill_atom;
	Atom protocol_atom;

	protocol_atom = XInternAtom(xfi->display, "WM_PROTOCOLS", True);
	kill_atom = XInternAtom(xfi->display, "WM_DELETE_WINDOW", True);

	if ((event->xclient.message_type == protocol_atom)
	    && ((Atom) event->xclient.data.l[0] == kill_atom))
	{
		return False;
	}

	return True;
}

boolean xf_event_EnterNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	xfi->mouse_active = True;

	if (xfi->fullscreen)
		XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);

	if (xfi->focused)
		XGrabKeyboard(xfi->display, xfi->window->handle, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	return True;
}

boolean xf_event_LeaveNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	xfi->mouse_active = False;
	XUngrabKeyboard(xfi->display, CurrentTime);

	return True;
}

boolean xf_event_process(freerdp* instance, XEvent* event)
{
	boolean app = False;
	boolean status = True;
	xfInfo* xfi = GET_XFI(instance);

	if (event->xany.window != xfi->window->handle)
		app = True;

	switch (event->type)
	{
		case Expose:
			status = xf_event_Expose(xfi, event, app);
			break;

		case VisibilityNotify:
			status = xf_event_VisibilityNotify(xfi, event, app);
			break;

		case MotionNotify:
			status = xf_event_MotionNotify(xfi, event, app);
			break;

		case ButtonPress:
			status = xf_event_ButtonPress(xfi, event, app);
			break;

		case ButtonRelease:
			status = xf_event_ButtonRelease(xfi, event, app);
			break;

		case KeyPress:
			status = xf_event_KeyPress(xfi, event, app);
			break;

		case KeyRelease:
			status = xf_event_KeyRelease(xfi, event, app);
			break;

		case FocusIn:
			status = xf_event_FocusIn(xfi, event, app);
			break;

		case FocusOut:
			status = xf_event_FocusOut(xfi, event, app);
			break;

		case EnterNotify:
			status = xf_event_EnterNotify(xfi, event, app);
			break;

		case LeaveNotify:
			status = xf_event_LeaveNotify(xfi, event, app);
			break;

		case NoExpose:
			break;

		case GraphicsExpose:
			break;

		case ConfigureNotify:
			break;

		case MapNotify:
			break;

		case ReparentNotify:
			break;

		case MappingNotify:
			status = xf_event_MappingNotify(xfi, event, app);
			break;

		case ClientMessage:
			status = xf_event_ClientMessage(xfi, event, app);
			break;

		default:
			printf("xf_event_process unknown event %d\n", event->type);
			break;
	}

	return status;
}
