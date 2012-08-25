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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

static boolean xf_event_Expose(xfInfo* xfi, XEvent* event, boolean app)
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

static boolean xf_event_VisibilityNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	xfi->unobscured = event->xvisibility.state == VisibilityUnobscured;
	return true;
}

static boolean xf_event_MotionNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpInput* input;
	int x, y;
	Window childWindow;

	input = xfi->instance->input;
	x = event->xmotion.x;
	y = event->xmotion.y;

	if (xfi->mouse_motion != true)
	{
		if ((event->xmotion.state & (Button1Mask | Button2Mask | Button3Mask)) == 0)
			return true;
	} 

	if (app)
	{
		// Translate to desktop coordinates
		XTranslateCoordinates(xfi->display, event->xmotion.window,
			RootWindowOfScreen(xfi->screen),
			x, y, &x, &y, &childWindow);
	}

	input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);

	if (xfi->fullscreen)
	{
		XSetInputFocus(xfi->display, xfi->window->handle, RevertToPointerRoot, CurrentTime);
	}

	return true;
}

static boolean xf_event_ButtonPress(xfInfo* xfi, XEvent* event, boolean app)
{
	int x, y;
	int flags;
	Window childWindow;
	boolean wheel;
	boolean extended;
	rdpInput* input;

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
			if (app)
			{
				// Translate to desktop coordinates
				XTranslateCoordinates(xfi->display, event->xmotion.window,
					RootWindowOfScreen(xfi->screen),
					x, y, &x, &y, &childWindow);
			}

			if (extended)
				input->ExtendedMouseEvent(input, flags, x, y);
			else
				input->MouseEvent(input, flags, x, y);
		}
	}

	return true;
}

static boolean xf_event_ButtonRelease(xfInfo* xfi, XEvent* event, boolean app)
{
	int x, y;
	int flags;
	Window childWindow;
	boolean extended;
	rdpInput* input;

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
		if (app)
		{
			// Translate to desktop coordinates
			XTranslateCoordinates(xfi->display, event->xmotion.window,
				RootWindowOfScreen(xfi->screen),
				x, y, &x, &y, &childWindow);
		}

		if (extended)
			input->ExtendedMouseEvent(input, flags, x, y);
		else
			input->MouseEvent(input, flags, x, y);
	}

	return true;
}

static boolean xf_event_KeyPress(xfInfo* xfi, XEvent* event, boolean app)
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

static boolean xf_event_KeyRelease(xfInfo* xfi, XEvent* event, boolean app)
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

static boolean xf_event_FocusIn(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xfocus.mode == NotifyGrab)
		return true;

	xfi->focused = true;

	if (xfi->mouse_active && (app != true))
		XGrabKeyboard(xfi->display, xfi->window->handle, true, GrabModeAsync, GrabModeAsync, CurrentTime);

	if (app)
	{
	       xf_rail_send_activate(xfi, event->xany.window, true);
		
       	       rdpWindow* window;
               rdpRail* rail = ((rdpContext*) xfi->context)->rail;
               
               window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);
       
               //Update the server with any window changes that occured while the window was not focused.
               if (window != NULL)
                       xf_rail_adjust_position(xfi, window);
	}
	xf_kbd_focus_in(xfi);

	if (app != true)
		xf_cliprdr_check_owner(xfi);

	return true;
}

static boolean xf_event_FocusOut(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xfocus.mode == NotifyUngrab)
		return true;

	xfi->focused = false;

	if (event->xfocus.mode == NotifyWhileGrabbed)
		XUngrabKeyboard(xfi->display, CurrentTime);

	xf_kbd_clear(xfi);

	if (app)
		xf_rail_send_activate(xfi, event->xany.window, false);

	return true;
}

static boolean xf_event_MappingNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (event->xmapping.request == MappingModifier)
	{
		XFreeModifiermap(xfi->modifier_map);
		xfi->modifier_map = XGetModifierMapping(xfi->display);
	}

	return true;
}

static boolean xf_event_ClientMessage(xfInfo* xfi, XEvent* event, boolean app)
{
	if ((event->xclient.message_type == xfi->WM_PROTOCOLS)
	    && ((Atom) event->xclient.data.l[0] == xfi->WM_DELETE_WINDOW))
	{
		if (app)
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

static boolean xf_event_EnterNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (app != true)
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

static boolean xf_event_LeaveNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (app != true)
	{
		xfi->mouse_active = false;
		XUngrabKeyboard(xfi->display, CurrentTime);
	}

	return true;
}

static boolean xf_event_ConfigureNotify(xfInfo* xfi, XEvent* event, boolean app)
{
        rdpWindow* window;
        rdpRail* rail = ((rdpContext*) xfi->context)->rail;

        window = window_list_get_by_extra_id(rail->list, (void*) event->xconfigure.window);

        if (window != NULL)
        {
                xfWindow* xfw;
                Window childWindow;
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

		//additonal checks for not in a local move and not ignoring configure to send position update to server,   
		//also should the window not be focused then do not send to server yet(ie. resizing using window decoration). 
		//The server will be updated when the window gets refocused.
		if (app && (!event->xconfigure.send_event || xfi->window->local_move.state == LMS_NOT_ACTIVE) 
		   && !xfw->rail_ignore_configure && xfi->focused)
			xf_rail_adjust_position(xfi, window);
        }

        return True;
}

static boolean xf_event_MapNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	RECTANGLE_16 rect;
	rdpWindow* window;
	rdpUpdate* update = xfi->instance->update;
	rdpRail* rail = ((rdpContext*) xfi->context)->rail;

	if (app != true)
	{
		if (xfi->suppress_output == true)
		{
			xfi->suppress_output = false;
			rect.left = 0;
			rect.top = 0;
			rect.right = xfi->width;
			rect.bottom = xfi->height;
			update->SuppressOutput((rdpContext*) xfi->context, 1, &rect);
		}
	}
	else
	{
		window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);

		if (window != NULL)
		{
			/* local restore event */
			//This is now handled as part of the PropertyNotify
               		//Doing this here would inhibit the ability to restore a maximized window
               		//that is minimized back to the maximized state
			//xf_rail_send_client_system_command(xfi, window->windowId, SC_RESTORE);
			xfWindow *xfw = (xfWindow*) window->extra;
			xfw->is_mapped = true;
		}
	}

	return true;
}

static boolean xf_event_UnmapNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	rdpWindow* window;
	rdpUpdate* update = xfi->instance->update;
	rdpRail* rail = ((rdpContext*) xfi->context)->rail;

	xf_kbd_release_all_keypress(xfi);

	if (app != true)
	{
		if (xfi->suppress_output == false)
		{
			xfi->suppress_output = true;
			update->SuppressOutput((rdpContext*) xfi->context, 0, NULL);
		}
	}
	else
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

static boolean xf_event_SelectionNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	if (app != true)
	{
		if (xf_cliprdr_process_selection_notify(xfi, event))
			return true;
	}

	return true;
}

static boolean xf_event_SelectionRequest(xfInfo* xfi, XEvent* event, boolean app)
{
	if (app != true)
	{
		if (xf_cliprdr_process_selection_request(xfi, event))
			return true;
	}

	return true;
}

static boolean xf_event_SelectionClear(xfInfo* xfi, XEvent* event, boolean app)
{
	if (app != true)
	{
		if (xf_cliprdr_process_selection_clear(xfi, event))
			return true;
	}

	return true;
}

static boolean xf_event_PropertyNotify(xfInfo* xfi, XEvent* event, boolean app)
{
	//This section handles sending the appropriate commands to the rail server
	//when the window has been minimized, maximized, restored locally 
	//ie. not using the buttons on the rail window itself
	if (app == true)
	{
	        rdpWindow* window;
	        rdpRail* rail = ((rdpContext*) xfi->context)->rail;
	
	        window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);           
	
	        if ((((Atom)event->xproperty.atom == xfi->_NET_WM_STATE) && (event->xproperty.state != PropertyDelete)) ||
	            (((Atom)event->xproperty.atom == xfi->WM_STATE) && (event->xproperty.state != PropertyDelete)))
	        {
	                boolean status;
	                boolean maxVert = false;
	                boolean maxHorz = false;
	                boolean minimized = false;
	                unsigned long nitems;
	                unsigned long bytes;
	                unsigned char* prop;
	                int i;
	
	                status = xf_GetWindowProperty(xfi, event->xproperty.window,
	                xfi->_NET_WM_STATE, 12, &nitems, &bytes, &prop);
	
	                if (status != true) {
	                               DEBUG_X11_LMS("No return _NET_WM_STATE, window is not maximized");
	                }               
	
	                for (i=0;i<nitems;i++)
	                {
	                        if ((Atom) ((uint16 **) prop)[i] == XInternAtom(xfi->display, "_NET_WM_STATE_MAXIMIZED_VERT", False))
	                        {
	                                maxVert = true;
	                        }

	                        if ((Atom) ((uint16 **)prop)[i] == XInternAtom(xfi->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
	                        {
	                                maxHorz = true;
	                        }
	                }

	                XFree(prop);            
	
	                status = xf_GetWindowProperty(xfi, event->xproperty.window,
	                xfi->WM_STATE, 1, &nitems, &bytes, &prop);
	
	                if (status != true) {
	                        DEBUG_X11_LMS("No return WM_STATE, window is not minimized");
	                }
	                else
	                {
	                        //If the window is in the iconic state
	                        if (((uint32) *prop == 3))
	                        {
	                                minimized = true;
	                        }
	                        else
	                                minimized = false;
	                       
	                        XFree(prop);
	                 }
	

	                 if (maxVert && maxHorz && !minimized && (xfi->window->rail_state != WINDOW_SHOW_MAXIMIZED))
                         {
                                DEBUG_X11_LMS("Send SC_MAXIMIZE command to rail server.");
                                xfi->window->rail_state = WINDOW_SHOW_MAXIMIZED;        
                                xf_rail_send_client_system_command(xfi, window->windowId, SC_MAXIMIZE);
                         }
                         else if (minimized && (xfi->window->rail_state != WINDOW_SHOW_MINIMIZED))
                         {
                                DEBUG_X11_LMS("Send SC_MINIMIZE command to rail server.");
                                xfi->window->rail_state = WINDOW_SHOW_MINIMIZED;
                                xf_rail_send_client_system_command(xfi, window->windowId, SC_MINIMIZE);
                         }
                         else if (!minimized && !maxVert && !maxHorz && (xfi->window->rail_state != WINDOW_SHOW))
                         {
                                DEBUG_X11_LMS("Send SC_RESTORE command to rail server");
                                xfi->window->rail_state = WINDOW_SHOW;
                                xf_rail_send_client_system_command(xfi, window->windowId, SC_RESTORE);
                         }
               }       
        }
	

	if (app != true)
	{
		if (xf_cliprdr_process_property_notify(xfi, event))
			return true;
	}

	return true;
}

static boolean xf_event_suppress_events(xfInfo *xfi, rdpWindow *window, XEvent*event)
{
	if (! xfi->remote_app)
		return false;

	switch (xfi->window->local_move.state)
	{
		case LMS_NOT_ACTIVE:
			// No local move in progress, nothing to do

			//Prevent Configure from happening during indeterminant state of Horz or Vert Max only
		        if ( (event->type == ConfigureNotify) && xfi->window->rail_ignore_configure)
                        {
                               DEBUG_X11_LMS("ConfigureNotify Event Ignored");
                               xfi->window->rail_ignore_configure = false;
                               return true;
                        }

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
				case GravityNotify:
					// Keep us up to date on position
					break;
				default:
					DEBUG_X11_LMS("Event Type to break LMS: %s", X11_EVENT_STRINGS[event->type]);
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
			status = xf_event_Expose(xfi, event, xfi->remote_app);
			break;

		case VisibilityNotify:
			status = xf_event_VisibilityNotify(xfi, event, xfi->remote_app);
			break;

		case MotionNotify:
			status = xf_event_MotionNotify(xfi, event, xfi->remote_app);
			break;

		case ButtonPress:
			status = xf_event_ButtonPress(xfi, event, xfi->remote_app);
			break;

		case ButtonRelease:
			status = xf_event_ButtonRelease(xfi, event, xfi->remote_app);
			break;

		case KeyPress:
			status = xf_event_KeyPress(xfi, event, xfi->remote_app);
			break;

		case KeyRelease:
			status = xf_event_KeyRelease(xfi, event, xfi->remote_app);
			break;

		case FocusIn:
			status = xf_event_FocusIn(xfi, event, xfi->remote_app);
			break;

		case FocusOut:
			status = xf_event_FocusOut(xfi, event, xfi->remote_app);
			break;

		case EnterNotify:
			status = xf_event_EnterNotify(xfi, event, xfi->remote_app);
			break;

		case LeaveNotify:
			status = xf_event_LeaveNotify(xfi, event, xfi->remote_app);
			break;

		case NoExpose:
			break;

		case GraphicsExpose:
			break;

		case ConfigureNotify:
			status = xf_event_ConfigureNotify(xfi, event, xfi->remote_app);
			break;

		case MapNotify:
			status = xf_event_MapNotify(xfi, event, xfi->remote_app);
			break;

		case UnmapNotify:
			status = xf_event_UnmapNotify(xfi, event, xfi->remote_app);
			break;

		case ReparentNotify:
			break;

		case MappingNotify:
			status = xf_event_MappingNotify(xfi, event, xfi->remote_app);
			break;

		case ClientMessage:
			status = xf_event_ClientMessage(xfi, event, xfi->remote_app);
			break;

		case SelectionNotify:
			status = xf_event_SelectionNotify(xfi, event, xfi->remote_app);
			break;

		case SelectionRequest:
			status = xf_event_SelectionRequest(xfi, event, xfi->remote_app);
			break;

		case SelectionClear:
			status = xf_event_SelectionClear(xfi, event, xfi->remote_app);
			break;

		case PropertyNotify:
			status = xf_event_PropertyNotify(xfi, event, xfi->remote_app);
			break;
	}

	return status;
}
