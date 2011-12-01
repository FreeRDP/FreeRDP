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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <freerdp/kbd/kbd.h>
#include <freerdp/kbd/vkcodes.h>

#include "xf_rail.h"
#include "xf_cliprdr.h"

#include "xf_event.h"

static const char* const X11_EVENT_STRINGS[] =
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
	int w, h;

	x = event->xexpose.x;
	y = event->xexpose.y;
	w = event->xexpose.width;
	h = event->xexpose.height;

	if (app != true)
	{
		XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc, x, y, w, h, x, y);
	}
	else
	{
		xfWindow* xfw;
		rdpWindow* window;
		rdpRail* rail = ((rdpContext*) xfi->context)->rail;

		window = window_list_get_by_extra_id(rail->list, (void*) event->xexpose.window);

		if (window != NULL)
		{
			xfw = (xfWindow*) window->extra;
			xf_UpdateWindowArea(xfi, xfw, x, y, w, h);
		}
	}

	return true;
}

boolean xf_event_VisibilityNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	xfi->unobscured = event->xvisibility.state == VisibilityUnobscured;
	return true;
}

boolean xf_event_MotionNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpInput* input;

	input = xfi->instance->input;

	if (app != true)
	{
		if (xfi->mouse_motion != true)
		{
			if ((event->xmotion.state & (Button1Mask | Button2Mask | Button3Mask)) == 0)
				return true;
		}

		input->MouseEvent(input, PTR_FLAGS_MOVE, event->xmotion.x, event->xmotion.y);

		if (xfi->fullscreen)
			XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);
	}
	else if (xfi->mouse_motion == true)
	{
		rdpWindow* window;
		int x = event->xmotion.x;
		int y = event->xmotion.y;
		rdpRail* rail = ((rdpContext*) xfi->context)->rail;

		window = window_list_get_by_extra_id(rail->list, (void*) event->xmotion.window);

		if (window != NULL)
		{
			x += window->windowOffsetX;
			y += window->windowOffsetY;
			input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);
		}
	}

	return true;
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
	wheel = false;

	switch (event->xbutton.button)
	{
		case 1:
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1;
			break;

		case 2:
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3;
			break;

		case 3:
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2;
			break;

		case 4:
			wheel = true;
			flags = PTR_FLAGS_WHEEL | 0x0078;
			break;

		case 5:
			wheel = true;
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
				rdpWindow* window;
				rdpRail* rail = ((rdpContext*) xfi->context)->rail;

				window = window_list_get_by_extra_id(rail->list, (void*) event->xbutton.window);

				if (window != NULL)
				{
					x += window->windowOffsetX;
					y += window->windowOffsetY;
				}
			}

			input->MouseEvent(input, flags, x, y);
		}
	}

	return true;
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
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_FLAGS_BUTTON1;
			break;

		case 2:
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_FLAGS_BUTTON3;
			break;

		case 3:
			x = event->xbutton.x;
			y = event->xbutton.y;
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
			rdpWindow* window;
			rdpRail* rail = ((rdpContext*) xfi->context)->rail;

			window = window_list_get_by_extra_id(rail->list, (void*) event->xbutton.window);

			if (window != NULL)
			{
				x += window->windowOffsetX;
				y += window->windowOffsetY;
			}
		}

		input->MouseEvent(input, flags, x, y);
	}

	return true;
}

boolean xf_event_KeyPress(xfInfo* xfi, XEvent* event, boolean app)
{
	KeySym keysym;
	char str[256];

	XLookupString((XKeyEvent*) event, str, sizeof(str), &keysym, NULL);

	xf_kbd_set_keypress(xfi, event->xkey.keycode, keysym);

	if (xfi->fullscreen_toggle && xf_kbd_handle_special_keys(xfi, keysym))
		return true;

	xf_kbd_send_key(xfi, true, event->xkey.keycode);

	return true;
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
				return true;
		}
	}

	xf_kbd_unset_keypress(xfi, event->xkey.keycode);
	xf_kbd_send_key(xfi, false, event->xkey.keycode);

	return true;
}

boolean xf_event_FocusIn(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xfocus.mode == NotifyGrab)
		return true;

	xfi->focused = true;

	if (xfi->mouse_active && (app != true))
		XGrabKeyboard(xfi->display, xfi->window->handle, true, GrabModeAsync, GrabModeAsync, CurrentTime);

	xf_rail_send_activate(xfi, event->xany.window, true);
	xf_kbd_focus_in(xfi);

	if (xfi->remote_app != true)
		xf_cliprdr_check_owner(xfi);

	return true;
}

boolean xf_event_FocusOut(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xfocus.mode == NotifyUngrab)
		return true;

	xfi->focused = false;

	if (event->xfocus.mode == NotifyWhileGrabbed)
		XUngrabKeyboard(xfi->display, CurrentTime);

	xf_rail_send_activate(xfi, event->xany.window, false);

	return true;
}

boolean xf_event_MappingNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xmapping.request == MappingModifier)
	{
		XFreeModifiermap(xfi->modifier_map);
		xfi->modifier_map = XGetModifierMapping(xfi->display);
	}

	return true;
}

boolean xf_event_ClientMessage(xfInfo* xfi, XEvent* event, boolean app)
{
	if ((event->xclient.message_type == xfi->WM_PROTOCOLS)
	    && ((Atom) event->xclient.data.l[0] == xfi->WM_DELETE_WINDOW))
	{
		if (app)
		{
			rdpWindow* window;
			rdpRail* rail = ((rdpContext*) xfi->context)->rail;

			window = window_list_get_by_extra_id(rail->list, (void*) event->xclient.window);

			if (window != NULL)
			{
				xf_rail_send_client_system_command(xfi, window->windowId, SC_CLOSE);
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

boolean xf_event_EnterNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (app != true)
	{
		xfi->mouse_active = true;

		if (xfi->fullscreen)
			XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);

		if (xfi->focused)
			XGrabKeyboard(xfi->display, xfi->window->handle, true, GrabModeAsync, GrabModeAsync, CurrentTime);
	} else {
		// Keep track of which window has focus so that we can apply pointer updates
		xfWindow* xfw;
		rdpWindow* window;
		rdpRail* rail = ((rdpContext*) xfi->context)->rail;
		window = window_list_get_by_extra_id(rail->list, (void*) event->xexpose.window);
		if (window != NULL)
		{
			xfw = (xfWindow*) window->extra;
			xfi->window = xfw;
		}
	}

	return true;
}

boolean xf_event_LeaveNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (app != true)
	{
		xfi->mouse_active = false;
		XUngrabKeyboard(xfi->display, CurrentTime);
	}

	return true;
}

boolean xf_event_ConfigureNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpWindow* window;
	rdpRail* rail = ((rdpContext*) xfi->context)->rail;

	window = window_list_get_by_extra_id(rail->list, (void*) event->xconfigure.window);

	if (window != NULL)
	{
		xfWindow* xfw;
		uint32 left, top;
		uint32 right, bottom;
		uint32 width, height;
		xfw = (xfWindow*) window->extra;

		left = event->xconfigure.x;
		top = event->xconfigure.y;
		width = event->xconfigure.width;
		height = event->xconfigure.height;
		right = left + width - 1;
		bottom = top + height - 1;

		DEBUG_X11_LMS("ConfigureNotify: send_event=%d eventWindow=0x%X window=0x%X above=0x%X rc={l=%d t=%d r=%d b=%d} "
			"w=%d h=%d override_redirect=%d",
			event->xconfigure.send_event,
			(uint32) event->xconfigure.event,
			(uint32) event->xconfigure.window,
			(uint32) event->xconfigure.above,
			left, top, right, bottom, width, height,
			event->xconfigure.override_redirect);
	}

	return true;
}

boolean xf_event_MapNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpWindow* window;
	rdpRail* rail = ((rdpContext*) xfi->context)->rail;

	if (app != true)
		return true;

	window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);

	if (window != NULL)
	{
		/* local restore event */
		xf_rail_send_client_system_command(xfi, window->windowId, SC_RESTORE);
	}

	return true;
}

boolean xf_event_SelectionNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (xfi->remote_app != true)
	{
		if (xf_cliprdr_process_selection_notify(xfi, event))
			return true;
	}

	return true;
}

boolean xf_event_SelectionRequest(xfInfo* xfi, XEvent* event, boolean app)
{
	if (xfi->remote_app != true)
	{
		if (xf_cliprdr_process_selection_request(xfi, event))
			return true;
	}

	return true;
}

boolean xf_event_SelectionClear(xfInfo* xfi, XEvent* event, boolean app)
{
	if (xfi->remote_app != true)
	{
		if (xf_cliprdr_process_selection_clear(xfi, event))
			return true;
	}

	return true;
}

boolean xf_event_PropertyNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (xfi->remote_app != true)
	{
		if (xf_cliprdr_process_property_notify(xfi, event))
			return true;
	}

	return true;
}

boolean xf_event_process(freerdp* instance, XEvent* event)
{
	boolean app = false;
	boolean status = true;
	xfInfo* xfi = ((xfContext*) instance->context)->xfi;

	if (xfi->remote_app == true)
	{
		app = true;
	}
	else
	{
		if (event->xany.window != xfi->window->handle)
			app = true;
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

		case SelectionNotify:
			status = xf_event_SelectionNotify(xfi, event, app);
			break;

		case SelectionRequest:
			status = xf_event_SelectionRequest(xfi, event, app);
			break;

		case SelectionClear:
			status = xf_event_SelectionClear(xfi, event, app);
			break;

		case PropertyNotify:
			status = xf_event_PropertyNotify(xfi, event, app);
			break;

		default:
			DEBUG_X11("xf_event_process unknown event %d", event->type);
			break;
	}

	return status;
}
