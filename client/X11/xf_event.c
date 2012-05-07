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

#include <freerdp/locale/keyboard.h>

#include "xf_rail.h"
#include "xf_window.h"
#include "xf_cliprdr.h"

#include "xf_event.h"

#ifdef WITH_DEBUG_X11
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
#endif

boolean xf_event_Expose(xfInfo* xfi, XEvent* event)
{
	int x, y;
	int w, h;

	x = event->xexpose.x;
	y = event->xexpose.y;
	w = event->xexpose.width;
	h = event->xexpose.height;

	if (xfi->remote_app != true)
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

boolean xf_event_VisibilityNotify(xfInfo* xfi, XEvent* event)
{
	xfi->unobscured = event->xvisibility.state == VisibilityUnobscured;
	return true;
}

boolean xf_event_MotionNotify(xfInfo* xfi, XEvent* event)
{
	rdpInput* input;
	Window childWindow;
	int x;
	int y;

	input = xfi->instance->input;

	if (xfi->mouse_motion != true)
	{
		if ((event->xmotion.state & (Button1Mask | Button2Mask | Button3Mask)) == 0)
			return true;
	}

	// No matter which window gets a motion notify, we need to send the motion 
	// event in root window coordinates.

        XTranslateCoordinates(xfi->display, event->xmotion.window,
		RootWindowOfScreen(xfi->screen),
		event->xmotion.x, event->xmotion.y,
		&x, &y, &childWindow);

	input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);

	if (xfi->fullscreen)
		XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);

	return true;
}

boolean xf_event_ButtonPress(xfInfo* xfi, XEvent* event)
{
	int x, y;
	int flags;
	boolean wheel;
	boolean extended;
	rdpInput* input;
	Window childWindow;

	input = xfi->instance->input;

	x = 0;
	y = 0;
	flags = 0;
	wheel = false;
	extended = false;

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

		case 6:		// wheel left or back
		case 8:		// back
		case 97:	// Xming
			extended = true;
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_XFLAGS_DOWN | PTR_XFLAGS_BUTTON1;
			break;

		case 7:		// wheel right or forward
		case 9:		// forward
		case 112:	// Xming
			extended = true;
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_XFLAGS_DOWN | PTR_XFLAGS_BUTTON2;
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
			// No matter which window gets a motion notify, we need to send the
			// event in root window coordinates.

		        XTranslateCoordinates(xfi->display, event->xmotion.window,
				RootWindowOfScreen(xfi->screen),
				x, y, &x, &y, &childWindow); 

			if (extended)
				input->ExtendedMouseEvent(input, flags, x, y);
			else
				input->MouseEvent(input, flags, x, y);
		}
	}

	return true;
}

boolean xf_event_ButtonRelease(xfInfo* xfi, XEvent* event)
{
	int x, y;
	int flags;
	boolean extended;
	rdpInput* input;
	Window childWindow;

	input = xfi->instance->input;

	x = 0;
	y = 0;
	flags = 0;
	extended = false;

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

		case 6:
		case 8:
		case 97:
			extended = true;
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_XFLAGS_BUTTON1;
			break;

		case 7:
		case 9:
		case 112:
			extended = true;
			x = event->xbutton.x;
			y = event->xbutton.y;
			flags = PTR_XFLAGS_BUTTON2;
			break;

		default:
			flags = 0;
			break;
	}

	if (flags != 0)
	{
		// No matter which window gets a motion notify, we need to send the
		// event in root window coordinates.

		XTranslateCoordinates(xfi->display, event->xmotion.window,
			RootWindowOfScreen(xfi->screen),
			x, y, &x, &y, &childWindow); 

		if (extended)
			input->ExtendedMouseEvent(input, flags, x, y);
		else
			input->MouseEvent(input, flags, x, y);
	}

	return true;
}

boolean xf_event_KeyPress(xfInfo* xfi, XEvent* event)
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

boolean xf_event_KeyRelease(xfInfo* xfi, XEvent* event)
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

boolean xf_event_FocusIn(xfInfo* xfi, XEvent* event)
{
	if (event->xfocus.mode == NotifyGrab)
		return true;

	xfi->focused = true;

	if (xfi->mouse_active && (xfi->remote_app != true))
		XGrabKeyboard(xfi->display, xfi->window->handle, true, GrabModeAsync, GrabModeAsync, CurrentTime);

	if (xfi->remote_app)
		xf_rail_send_activate(xfi, event->xany.window, true);

	xf_kbd_focus_in(xfi);

	if (xfi->remote_app != true)
		xf_cliprdr_check_owner(xfi);

	return true;
}

boolean xf_event_FocusOut(xfInfo* xfi, XEvent* event)
{
	if (event->xfocus.mode == NotifyUngrab)
		return true;

	xfi->focused = false;

	if (event->xfocus.mode == NotifyWhileGrabbed)
		XUngrabKeyboard(xfi->display, CurrentTime);

	if (xfi->remote_app)
		xf_rail_send_activate(xfi, event->xany.window, false);

	return true;
}

boolean xf_event_MappingNotify(xfInfo* xfi, XEvent* event)
{
	if (event->xmapping.request == MappingModifier)
	{
		XFreeModifiermap(xfi->modifier_map);
		xfi->modifier_map = XGetModifierMapping(xfi->display);
	}

	return true;
}

boolean xf_event_ClientMessage(xfInfo* xfi, XEvent* event)
{
	if ((event->xclient.message_type == xfi->WM_PROTOCOLS)
	    && ((Atom) event->xclient.data.l[0] == xfi->WM_DELETE_WINDOW))
	{
		if (xfi->remote_app)
		{
			DEBUG_X11("RAIL window closed");
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
			DEBUG_X11("Main window closed");
			return false;
		}
	}

	return true;
}

boolean xf_event_EnterNotify(xfInfo* xfi, XEvent* event)
{
	if (xfi->remote_app != true)
	{
		xfi->mouse_active = true;

		if (xfi->fullscreen)
			XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);

		if (xfi->focused)
			XGrabKeyboard(xfi->display, xfi->window->handle, true, GrabModeAsync, GrabModeAsync, CurrentTime);
	}
	else
	{
		/* keep track of which window has focus so that we can apply pointer updates */

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

boolean xf_event_LeaveNotify(xfInfo* xfi, XEvent* event)
{
	if (xfi->remote_app != true)
	{
		xfi->mouse_active = false;
		XUngrabKeyboard(xfi->display, CurrentTime);
	}

	return true;
}

boolean xf_event_ConfigureNotify(xfInfo* xfi, XEvent* event)
{
        rdpWindow* window;
        rdpRail* rail = ((rdpContext*) xfi->context)->rail;
	xfWindow* xfw;
	Window childWindow;

	if (xfi->remote_app && ! event->xconfigure.send_event)
	{
		/*
		* If a window is moved by the local window manager it will no longer be
		* at the location expected by the remote desktop.  In this case we
		* need to send the second half of a "local move" sequence to update
		* the RDP server for the windows new coordinates.
		*/

        	window = window_list_get_by_extra_id(rail->list, (void*) event->xconfigure.window);

		if (window != NULL)
		{
			xfw = (xfWindow*) window->extra;

			/*
			* ConfigureNotify coordinates are expressed relative to the window parent.
			* Translate these to root window coordinates.
			*/

			XTranslateCoordinates(xfi->display, xfw->handle, 
				RootWindowOfScreen(xfi->screen),
				0, 0, &xfw->left, &xfw->top, &childWindow);

			xfw->width = event->xconfigure.width;
			xfw->height = event->xconfigure.height;
			xfw->right = xfw->left + xfw->width - 1;
			xfw->bottom = xfw->top + xfw->height - 1;

			DEBUG_X11_LMS("window=0x%X rc={l=%d t=%d r=%d b=%d} w=%u h=%u send_event=%d",
				(uint32) xfw->handle, xfw->left, xfw->top, xfw->right, xfw->bottom,
				xfw->width, xfw->height, event->xconfigure.send_event);

			xf_rail_adjust_position(xfi, window);
		}
	}
        return True;
}

boolean xf_event_MapNotify(xfInfo* xfi, XEvent* event)
{
	rdpWindow* window;
	rdpRail* rail = ((rdpContext*) xfi->context)->rail;

	if (xfi->remote_app)
	{
		window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);

		if (window != NULL)
		{
			/* local restore event */
			xf_rail_send_client_system_command(xfi, window->windowId, SC_RESTORE);
			xfWindow *xfw = (xfWindow*) window->extra;
			xfw->is_mapped = true;
		}
	}

	return true;
}

boolean xf_event_UnmapNotify(xfInfo* xfi, XEvent* event)
{
	rdpWindow* window;
	rdpRail* rail = ((rdpContext*) xfi->context)->rail;

	xf_kbd_release_all_keypress(xfi);

	if (xfi->remote_app)
	{	
		window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);

		if (window != NULL)
		{
			xfWindow *xfw = (xfWindow*) window->extra;
			xfw->is_mapped = false;
		}
	}

	return true;
}

boolean xf_event_SelectionNotify(xfInfo* xfi, XEvent* event)
{
	// FIXME: make clipboard work for seamless window mode
	if (xfi->remote_app != true)
	{
		if (xf_cliprdr_process_selection_notify(xfi, event))
			return true;
	}

	return true;
}

boolean xf_event_SelectionRequest(xfInfo* xfi, XEvent* event)
{
	// FIXME: make clipboard work for seamless window mode
	if (xfi->remote_app != true)
	{
		if (xf_cliprdr_process_selection_request(xfi, event))
			return true;
	}

	return true;
}

boolean xf_event_SelectionClear(xfInfo* xfi, XEvent* event)
{
	// FIXME: make clipboard work for seamless window mode
	if (xfi->remote_app != true)
	{
		if (xf_cliprdr_process_selection_clear(xfi, event))
			return true;
	}

	return true;
}

boolean xf_event_PropertyNotify(xfInfo* xfi, XEvent* event)
{
	// FIXME: make clipboard work for seamless window mode
	if (xfi->remote_app != true)
	{
		if (xf_cliprdr_process_property_notify(xfi, event))
			return true;
	}

	return true;
}

/*
* This function exists to support RAIL local moves.  The X window specification
* does not put any requirement on window managers to tell X clients when a
* window is being reconfigured due to a local window manager move.  Therefore 
* there is not any event that clearly indicates when a user has moved a
* window (using keyboard or mouse). We have to guess based upon the order of
* ConfigureNotify and other events.
* If the X window manager could be modified to work with the RDP client
* cooperatively, the integration of RDP seamless windows would be much
* better.
*/
boolean xf_event_suppress_events(xfInfo *xfi, rdpWindow *window, XEvent*event)
{
	if (! xfi->remote_app)
		return false;

	switch (xfi->window->local_move.state)
	{
		case LMS_NOT_ACTIVE:
			// No local move in progress, nothing to do
			break;
		case LMS_STARTING:
			// Local move initiated by RDP server, but we
			// have not yet seen any updates from the X server
			switch(event->type)
			{
				case ConfigureNotify:
					// Starting to see move events 
					// from the X server. Local 
					// move is now in progress.
					xfi->window->local_move.state = LMS_ACTIVE;

					// Allow these events to be processed during move to keep
					// our state up to date.
					break;
				case ButtonPress:
				case ButtonRelease:
				case KeyPress:
				case KeyRelease:
				case UnmapNotify:
                	        	// A button release event means the X 
					// window server did not grab the
                        		// mouse before the user released it.  
					// In this case we must cancel the 
					// local move. The event will be 
					// processed below as normal, below.
	                        	break;
				case VisibilityNotify:
				case PropertyNotify:
				case Expose:
					// Allow these events to pass
					break;
				default:
					// Eat any other events 
					return true;
			}
			break;

		case LMS_ACTIVE:
			// Local move is in progress
			switch(event->type)
			{
				case ConfigureNotify:
				case VisibilityNotify:
				case PropertyNotify:
				case Expose:
					// Keep us up to date on position
					break;
				default:
					// Any other event terminates move
					xf_rail_end_local_move(xfi, window);
					break;
			}
			break;

		case LMS_TERMINATING:
			// Already sent RDP end move to sever
			// Allow events to pass.
			break;
	}	

	return false;
}


boolean xf_event_process(freerdp* instance, XEvent* event)
{
	boolean status = true;
	xfInfo* xfi = ((xfContext*) instance->context)->xfi;
	rdpRail* rail = ((rdpContext*) xfi->context)->rail;
	rdpWindow* window;

	if (xfi->remote_app)
	{
		window = window_list_get_by_extra_id(
			rail->list, (void*) event->xexpose.window);
		if (window) 
		{
			// Update "current" window for cursor change orders
			xfi->window = (xfWindow *) window->extra;

			if (xf_event_suppress_events(xfi, window, event))
				return true;
		}
	}

	if (event->type != MotionNotify)
		DEBUG_X11("%s Event(%d): wnd=0x%04X", X11_EVENT_STRINGS[event->type], event->type, (uint32) event->xany.window);

	switch (event->type)
	{
		case Expose:
			status = xf_event_Expose(xfi, event);
			break;

		case VisibilityNotify:
			status = xf_event_VisibilityNotify(xfi, event);
			break;

		case MotionNotify:
			status = xf_event_MotionNotify(xfi, event);
			break;

		case ButtonPress:
			status = xf_event_ButtonPress(xfi, event);
			break;

		case ButtonRelease:
			status = xf_event_ButtonRelease(xfi, event);
			break;

		case KeyPress:
			status = xf_event_KeyPress(xfi, event);
			break;

		case KeyRelease:
			status = xf_event_KeyRelease(xfi, event);
			break;

		case FocusIn:
			status = xf_event_FocusIn(xfi, event);
			break;

		case FocusOut:
			status = xf_event_FocusOut(xfi, event);
			break;

		case EnterNotify:
			status = xf_event_EnterNotify(xfi, event);
			break;

		case LeaveNotify:
			status = xf_event_LeaveNotify(xfi, event);
			break;

		case NoExpose:
			break;

		case GraphicsExpose:
			break;

		case ConfigureNotify:
			status = xf_event_ConfigureNotify(xfi, event);
			break;

		case MapNotify:
			status = xf_event_MapNotify(xfi, event);
			break;

		case UnmapNotify:
			status = xf_event_UnmapNotify(xfi, event);
			break;

		case ReparentNotify:
			break;

		case MappingNotify:
			status = xf_event_MappingNotify(xfi, event);
			break;

		case ClientMessage:
			status = xf_event_ClientMessage(xfi, event);
			break;

		case SelectionNotify:
			status = xf_event_SelectionNotify(xfi, event);
			break;

		case SelectionRequest:
			status = xf_event_SelectionRequest(xfi, event);
			break;

		case SelectionClear:
			status = xf_event_SelectionClear(xfi, event);
			break;

		case PropertyNotify:
			status = xf_event_PropertyNotify(xfi, event);
			break;
	}

	return status;
}
