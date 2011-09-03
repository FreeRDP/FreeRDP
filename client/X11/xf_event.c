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

#include "xf_rail.h"

#include "xf_event.h"

uint8 X11_EVENT_STRINGS[][20] =
{
	"", "",
	"KeyPress",
	"KeyRelease",
	"ButtonPress",
	"ButtonRelease",
	"MotionNotify",
	"EnterNotify",
	"LeaveNotify",
	"FocusIn",
	"FocusOut",
	"KeymapNotify",
	"Expose",
	"GraphicsExpose",
	"NoExpose",
	"VisibilityNotify",
	"CreateNotify",
	"DestroyNotify",
	"UnmapNotify",
	"MapNotify",
	"MapRequest",
	"ReparentNotify",
	"ConfigureNotify",
	"ConfigureRequest",
	"GravityNotify",
	"ResizeRequest",
	"CirculateNotify",
	"CirculateRequest",
	"PropertyNotify",
	"SelectionClear",
	"SelectionRequest",
	"SelectionNotify",
	"ColormapNotify",
	"ClientMessage",
	"MappingNotify",
	"GenericEvent",
};

void xf_send_mouse_motion_event(rdpInput* input, boolean down, uint32 button, uint16 x, uint16 y)
{
	input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);
}

boolean xf_event_Expose(xfInfo* xfi, XEvent* event, boolean app)
{
	int x, y;
	int cx, cy;

	if (app != True)
	{
		x = event->xexpose.x;
		y = event->xexpose.y;
		cx = event->xexpose.width;
		cy = event->xexpose.height;
		XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc, x, y, cx, cy, x, y);
	}
	else
	{
#if 0
		xfWindow* xfw;
		rdpWindow* window;

		window = window_list_get_by_extra_id(xfi->rail->list, (void*) event->xany.window);

		if (window != NULL)
		{
			xfw = (xfWindow*) window->extra;

			XPutImage(xfi->display, xfi->primary, xfw->gc, xfi->image,
					xfw->left, xfw->top, xfw->left, xfw->top, xfw->width, xfw->height);

			XCopyArea(xfi->display, xfi->primary, xfw->handle, xfw->gc,
					xfw->left, xfw->top, xfw->width, xfw->height, 0, 0);

			XFlush(xfi->display);

			xfw = (xfWindow*) window->extra;
		}
#endif
	}

	return True;
}

boolean xf_event_VisibilityNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	xfi->unobscured = event->xvisibility.state == VisibilityUnobscured;
	return True;
}

boolean xf_event_MotionNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpInput* input;

	input = xfi->instance->input;

	if (app != True)
	{
		if (xfi->mouse_motion != True)
		{
			if ((event->xmotion.state & (Button1Mask | Button2Mask | Button3Mask)) == 0)
				return True;
		}

		input->MouseEvent(input, PTR_FLAGS_MOVE, event->xmotion.x, event->xmotion.y);

		if (xfi->fullscreen)
			XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);
	}
	else if (xfi->mouse_motion == True)
	{
		xfWindow* xfw;
		rdpWindow* window;
		int x = event->xmotion.x;
		int y = event->xmotion.y;
		window = window_list_get_by_extra_id(xfi->rail->list, (void*)event->xmotion.window);

		if (window != NULL)
		{
			xfw = (xfWindow*) window->extra;
			x += xfw->left;
			y += xfw->top;

			if (!xfw->isLocalMoveSizeModeEnabled)
				input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);
		}
	}

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
			flags = PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;
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
				xfWindow* xfw;
				rdpWindow* window;
				window = window_list_get_by_extra_id(xfi->rail->list, (void*) event->xbutton.window);

				if (window != NULL)
				{
					xfw = (xfWindow*) window->extra;
					x += xfw->left;
					y += xfw->top;
				}
			}

			input->MouseEvent(input, flags, x, y);
		}
	}

	return True;
}

boolean xf_event_ButtonRelease(xfInfo* xfi, XEvent* event, boolean app)
{
	uint16 x, y;
	uint16 flags;
	rdpInput* input;

	input = xfi->instance->input;

	x = 0;
	y = 0;
	flags = 0;

	switch (event->xbutton.button)
	{
		case 1:
			x = event->xmotion.x;
			y = event->xmotion.y;
			flags = PTR_FLAGS_BUTTON1;
			break;

		case 2:
			x = event->xmotion.x;
			y = event->xmotion.y;
			flags = PTR_FLAGS_BUTTON3;
			break;

		case 3:
			x = event->xmotion.x;
			y = event->xmotion.y;
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
			xfWindow* xfw;
			rdpWindow* window;
			window = window_list_get_by_extra_id(xfi->rail->list, (void*) event->xany.window);

			if (window != NULL)
			{
				xfw = (xfWindow*) window->extra;
				x += xfw->left;
				y += xfw->top;
			}
		}

		input->MouseEvent(input, flags, x, y);
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

	if (xfi->mouse_active && (app != True))
		XGrabKeyboard(xfi->display, xfi->window->handle, True, GrabModeAsync, GrabModeAsync, CurrentTime);

	//xf_rail_send_activate(xfi, event->xany.window, True);
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

	//xf_rail_send_activate(xfi, event->xany.window, False);

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
	if (app != True)
	{
		xfi->mouse_active = True;

		if (xfi->fullscreen)
			XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);

		if (xfi->focused)
			XGrabKeyboard(xfi->display, xfi->window->handle, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	}

	return True;
}

boolean xf_event_LeaveNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (app != True)
	{
		xfi->mouse_active = False;
		XUngrabKeyboard(xfi->display, CurrentTime);
	}

	return True;
}

boolean xf_event_ConfigureNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpWindow* window;

	window = window_list_get_by_extra_id(xfi->rail->list, (void*) event->xconfigure.window);

	if (window != NULL)
	{
		xfWindow* xfw;
		xfw = (xfWindow*) window->extra;

		DEBUG_X11_LMS("ConfigureNotify: send_event=%d eventWindow=0x%X window=0x%X above=0x%X rc={l=%d t=%d r=%d b=%d} "
			"w=%d h=%d override_redirect=%d",
			event->xconfigure.send_event,
			(uint32)event->xconfigure.event,
			(uint32)event->xconfigure.window,
			(uint32)event->xconfigure.above,
			event->xconfigure.x,
			event->xconfigure.y,
			event->xconfigure.x + event->xconfigure.width - 1,
			event->xconfigure.y + event->xconfigure.height - 1,
			event->xconfigure.width,
			event->xconfigure.height,
			event->xconfigure.override_redirect);

		if (xfw->isLocalMoveSizeModeEnabled && event->xconfigure.above != 0)
		{
			uint32 left = event->xconfigure.x;
			uint32 top = event->xconfigure.y;
			uint32 right = event->xconfigure.x + event->xconfigure.width;
			uint32 bottom = event->xconfigure.y + event->xconfigure.height - 1;

			DEBUG_X11_LMS("MoveSendToServer: windowId=0x%X rc={l=%d t=%d r=%d b=%d} w=%d h=%d \n",
				(uint32)xfw->handle, left, top, right, bottom, event->xconfigure.width,
				event->xconfigure.height);

			xf_rail_send_windowmove(xfi, window->windowId, left, top, right, bottom);
		}

		XPutImage(xfi->display, xfi->primary, xfw->gc, xfi->image,
				xfw->left, xfw->top, xfw->left, xfw->top, xfw->width, xfw->height);

		XCopyArea(xfi->display, xfi->primary, xfw->handle, xfw->gc,
				xfw->left, xfw->top, xfw->width, xfw->height, 0, 0);

		XFlush(xfi->display);
	}

	return True;
}

boolean xf_event_MapNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpWindow* window;

	if (app != True)
		return True;

	window = window_list_get_by_extra_id(xfi->rail->list, (void*) event->xany.window);

	if (window != NULL)
	{
		/* local restore event */
		xf_rail_send_client_system_command(xfi, window->windowId, SC_RESTORE);
	}

	return True;
}

boolean xf_event_process(freerdp* instance, XEvent* event)
{
	boolean app = False;
	boolean status = True;
	xfInfo* xfi = GET_XFI(instance);

	if (xfi->remote_app == True)
	{
		app = True;
	}
	else
	{
		if (event->xany.window != xfi->window->handle)
			app = True;
	}


	if (event->type != MotionNotify)
		DEBUG_X11("%s Event: wnd=0x%04X", X11_EVENT_STRINGS[event->type], (uint32) event->xany.window);

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
			status = xf_event_ConfigureNotify(xfi, event, app);
			break;

		case MapNotify:
			status = xf_event_MapNotify(xfi, event, app);
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
			DEBUG_X11("xf_event_process unknown event %d", event->type);
			break;
	}

	return status;
}
