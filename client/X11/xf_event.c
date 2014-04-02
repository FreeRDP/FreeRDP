/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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
#include "xf_input.h"

#include "xf_event.h"
#include "xf_input.h"

#define CLAMP_COORDINATES(x, y) if (x < 0) x = 0; if (y < 0) y = 0

const char* const X11_EVENT_STRINGS[] =
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

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(fmt, ...) DEBUG_CLASS(X11, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#ifdef WITH_DEBUG_X11_LOCAL_MOVESIZE
#define DEBUG_X11_LMS(fmt, ...) DEBUG_CLASS(X11_LMS, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11_LMS(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

int xf_event_action_script_init(xfContext* xfc)
{
	int exitCode;
	char* xevent;
	FILE* actionScript;
	char buffer[1024] = { 0 };
	char command[1024] = { 0 };

	xfc->xevents = ArrayList_New(TRUE);
	ArrayList_Object(xfc->xevents)->fnObjectFree = free;

	sprintf_s(command, sizeof(command), "%s xevent", xfc->actionScript);

	actionScript = popen(command, "r");

	if (actionScript < 0)
		return -1;

	while (fgets(buffer, sizeof(buffer), actionScript) != NULL)
	{
		strtok(buffer, "\n");
		xevent = _strdup(buffer);
		ArrayList_Add(xfc->xevents, xevent);
	}

	exitCode = pclose(actionScript);

	return 1;
}

void xf_event_action_script_free(xfContext* xfc)
{
	if (xfc->xevents)
	{
		ArrayList_Free(xfc->xevents);
		xfc->xevents = NULL;
	}
}

int xf_event_execute_action_script(xfContext* xfc, XEvent* event)
{
	int index;
	int count;
	char* name;
	int exitCode;
	FILE* actionScript;
	BOOL match = FALSE;
	const char* xeventName;
	char buffer[1024] = { 0 };
	char command[1024] = { 0 };

	if (!xfc->actionScript)
		return 1;

	if (event->type > (sizeof(X11_EVENT_STRINGS) / sizeof(const char*)))
		return 1;

	xeventName = X11_EVENT_STRINGS[event->type];

	count = ArrayList_Count(xfc->xevents);

	for (index = 0; index < count; index++)
	{
		name = (char*) ArrayList_GetItem(xfc->xevents, index);

		if (_stricmp(name, xeventName) == 0)
		{
			match = TRUE;
			break;
		}
	}

	if (!match)
		return 1;

	sprintf_s(command, sizeof(command), "%s xevent %s %d",
			xfc->actionScript, xeventName, (int) xfc->window->handle);

	actionScript = popen(command, "r");

	if (actionScript < 0)
		return -1;

	while (fgets(buffer, sizeof(buffer), actionScript) != NULL)
	{
		strtok(buffer, "\n");
	}

	exitCode = pclose(actionScript);

	return 1;
}

static BOOL xf_event_Expose(xfContext* xfc, XEvent* event, BOOL app)
{
	int x, y;
	int w, h;
	
	x = event->xexpose.x;
	y = event->xexpose.y;
	w = event->xexpose.width;
	h = event->xexpose.height;
	
	if (!app)
	{
		if ((xfc->settings->ScalingFactor != 1.0) || (xfc->offset_x) || (xfc->offset_y))
		{
			xf_draw_screen_scaled(xfc, x - xfc->offset_x,
					      y - xfc->offset_y, w, h, FALSE);
		}
		else
		{
			XCopyArea(xfc->display, xfc->primary,
				  xfc->window->handle, xfc->gc, x, y, w,
				  h, x, y);
		}
	}
	else
	{
		xfWindow* xfw;
		rdpWindow* window;
		rdpRail* rail = ((rdpContext*) xfc)->rail;
		
		window = window_list_get_by_extra_id(rail->list,
						     (void*) event->xexpose.window);
		
		if (window)
		{
			xfw = (xfWindow*) window->extra;
			xf_UpdateWindowArea(xfc, xfw, x, y, w, h);
		}
	}
	
	return TRUE;
}

static BOOL xf_event_VisibilityNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	xfc->unobscured = event->xvisibility.state == VisibilityUnobscured;
	return TRUE;
}

BOOL xf_generic_MotionNotify(xfContext* xfc, int x, int y, int state, Window window, BOOL app)
{
	rdpInput* input;
	Window childWindow;

	input = xfc->instance->input;

	if (!xfc->settings->MouseMotion)
	{
		if ((state & (Button1Mask | Button2Mask | Button3Mask)) == 0)
			return TRUE;
	}

	if (app)
	{
		/* make sure window exists */
		if (xf_rdpWindowFromWindow(xfc, window) == 0)
		{
			return TRUE;
		}

		/* Translate to desktop coordinates */
		XTranslateCoordinates(xfc->display, window,
			RootWindowOfScreen(xfc->screen),
			x, y, &x, &y, &childWindow);
	}
	
	/* Take scaling in to consideration */
	if ( (xfc->settings->ScalingFactor != 1.0) || (xfc->offset_x) || (xfc->offset_y) )
	{
		x = (int)((x - xfc->offset_x) * (1.0 / xfc->settings->ScalingFactor) );
		y = (int)((y - xfc->offset_y) * (1.0 / xfc->settings->ScalingFactor) );
	}
	CLAMP_COORDINATES(x,y);

	input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);

	if (xfc->fullscreen)
	{
		XSetInputFocus(xfc->display, xfc->window->handle, RevertToPointerRoot, CurrentTime);
	}

	return TRUE;
}

static BOOL xf_event_MotionNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	if (xfc->use_xinput)
		return TRUE;

	return xf_generic_MotionNotify(xfc, event->xmotion.x, event->xmotion.y,
			event->xmotion.state, event->xmotion.window, app);
}

BOOL xf_generic_ButtonPress(xfContext* xfc, int x, int y, int button, Window window, BOOL app)
{
	int flags;
	BOOL wheel;
	BOOL extended;
	rdpInput* input;
	Window childWindow;

	flags = 0;
	wheel = FALSE;
	extended = FALSE;
	input = xfc->instance->input;

	switch (button)
	{
		case 1:
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1;
			break;

		case 2:
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3;
			break;

		case 3:
			flags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2;
			break;

		case 4:
			wheel = TRUE;
			flags = PTR_FLAGS_WHEEL | 0x0078;
			break;

		case 5:
			wheel = TRUE;
			flags = PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;
			break;

		case 6:		/* wheel left or back */
		case 8:		/* back */
		case 97:	/* Xming */
			extended = TRUE;
			flags = PTR_XFLAGS_DOWN | PTR_XFLAGS_BUTTON1;
			break;

		case 7:		/* wheel right or forward */
		case 9:		/* forward */
		case 112:	/* Xming */
			extended = TRUE;
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
				/* make sure window exists */
				if (xf_rdpWindowFromWindow(xfc, window) == 0)
				{
					return TRUE;
				}
				/* Translate to desktop coordinates */
				XTranslateCoordinates(xfc->display, window,
					RootWindowOfScreen(xfc->screen),
					x, y, &x, &y, &childWindow);

			}

			if ((xfc->settings->ScalingFactor != 1.0) || (xfc->offset_x)
			    || (xfc->offset_y))
			{
				x = (int) ((x - xfc->offset_x)
					   * (1.0 / xfc->settings->ScalingFactor));
				y = (int) ((y - xfc->offset_y)
					   * (1.0 / xfc->settings->ScalingFactor));
			}

			CLAMP_COORDINATES(x,y);
			if (extended)
				input->ExtendedMouseEvent(input, flags, x, y);
			else
				input->MouseEvent(input, flags, x, y);
		}
	}

	return TRUE;
}

static BOOL xf_event_ButtonPress(xfContext* xfc, XEvent* event, BOOL app)
{
	if (xfc->use_xinput)
		return TRUE;

	return xf_generic_ButtonPress(xfc, event->xbutton.x, event->xbutton.y,
			event->xbutton.button, event->xbutton.window, app);
}

BOOL xf_generic_ButtonRelease(xfContext* xfc, int x, int y, int button, Window window, BOOL app)
{
	int flags;
	BOOL wheel;
	BOOL extended;
	rdpInput* input;
	Window childWindow;


	flags = 0;
	wheel = FALSE;
	extended = FALSE;
	input = xfc->instance->input;

	switch (button)
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

		case 6:
		case 8:
		case 97:
			extended = TRUE;
			flags = PTR_XFLAGS_BUTTON1;
			break;

		case 7:
		case 9:
		case 112:
			extended = TRUE;
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
			/* make sure window exists */
			if (xf_rdpWindowFromWindow(xfc, window) == NULL)
			{
				return TRUE;
			}
			/* Translate to desktop coordinates */
			XTranslateCoordinates(xfc->display, window,
				RootWindowOfScreen(xfc->screen),
				x, y, &x, &y, &childWindow);
		}

		
		if ((xfc->settings->ScalingFactor != 1.0) || (xfc->offset_x) || (xfc->offset_y))
		{
			x = (int) ((x - xfc->offset_x) * (1.0 / xfc->settings->ScalingFactor));
			y = (int) ((y - xfc->offset_y) * (1.0 / xfc->settings->ScalingFactor));
		}

		CLAMP_COORDINATES(x,y);

		if (extended)
			input->ExtendedMouseEvent(input, flags, x, y);
		else
			input->MouseEvent(input, flags, x, y);
	}

	return TRUE;
}

static BOOL xf_event_ButtonRelease(xfContext* xfc, XEvent* event, BOOL app)
{
	if (xfc->use_xinput)
		return TRUE;

	return xf_generic_ButtonRelease(xfc, event->xbutton.x, event->xbutton.y,
			event->xbutton.button, event->xbutton.window, app);
}

static BOOL xf_event_KeyPress(xfContext* xfc, XEvent* event, BOOL app)
{
	KeySym keysym;
	char str[256];

	XLookupString((XKeyEvent*) event, str, sizeof(str), &keysym, NULL);

	xf_keyboard_key_press(xfc, event->xkey.keycode, keysym);

	return TRUE;
}

static BOOL xf_event_KeyRelease(xfContext* xfc, XEvent* event, BOOL app)
{
	XEvent nextEvent;

	if (XPending(xfc->display))
	{
		ZeroMemory(&nextEvent, sizeof(nextEvent));
		XPeekEvent(xfc->display, &nextEvent);

		if (nextEvent.type == KeyPress)
		{
			if (nextEvent.xkey.keycode == event->xkey.keycode)
				return TRUE;
		}
	}

	xf_keyboard_key_release(xfc, event->xkey.keycode);

	return TRUE;
}

static BOOL xf_event_FocusIn(xfContext* xfc, XEvent* event, BOOL app)
{
	if (event->xfocus.mode == NotifyGrab)
		return TRUE;

	xfc->focused = TRUE;

	if (xfc->mouse_active && (!app))
		XGrabKeyboard(xfc->display, xfc->window->handle, TRUE, GrabModeAsync, GrabModeAsync, CurrentTime);

	if (app)
	{
	       xf_rail_send_activate(xfc, event->xany.window, TRUE);
		
       	       rdpWindow* window;
               rdpRail* rail = ((rdpContext*) xfc)->rail;
               
               window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);
       
               /* Update the server with any window changes that occured while the window was not focused. */
               if (window != NULL)
                       xf_rail_adjust_position(xfc, window);
	}

	xf_keyboard_focus_in(xfc);

	if (!app)
		xf_cliprdr_check_owner(xfc);

	return TRUE;
}

static BOOL xf_event_FocusOut(xfContext* xfc, XEvent* event, BOOL app)
{
	if (event->xfocus.mode == NotifyUngrab)
		return TRUE;

	xfc->focused = FALSE;

	if (event->xfocus.mode == NotifyWhileGrabbed)
		XUngrabKeyboard(xfc->display, CurrentTime);

	xf_keyboard_clear(xfc);

	if (app)
		xf_rail_send_activate(xfc, event->xany.window, FALSE);

	return TRUE;
}

static BOOL xf_event_MappingNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	if (event->xmapping.request == MappingModifier)
	{
		if (xfc->modifierMap)
			XFreeModifiermap(xfc->modifierMap);

		xfc->modifierMap = XGetModifierMapping(xfc->display);
	}

	return TRUE;
}

static BOOL xf_event_ClientMessage(xfContext* xfc, XEvent* event, BOOL app)
{
	if ((event->xclient.message_type == xfc->WM_PROTOCOLS)
	    && ((Atom) event->xclient.data.l[0] == xfc->WM_DELETE_WINDOW))
	{
		if (app)
		{
			DEBUG_X11("RAIL window closed");
			rdpWindow* window;
			rdpRail* rail = ((rdpContext*) xfc)->rail;

			window = window_list_get_by_extra_id(rail->list, (void*) event->xclient.window);

			if (window != NULL)
			{
				xf_rail_send_client_system_command(xfc, window->windowId, SC_CLOSE);
			}

			return TRUE;
		}
		else
		{
			DEBUG_X11("Main window closed");
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL xf_event_EnterNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	if (!app)
	{
		xfc->mouse_active = TRUE;

		if (xfc->fullscreen)
			XSetInputFocus(xfc->display, xfc->window->handle, RevertToPointerRoot, CurrentTime);

		if (xfc->focused)
			XGrabKeyboard(xfc->display, xfc->window->handle, TRUE, GrabModeAsync, GrabModeAsync, CurrentTime);
	}
	else
	{
		/* keep track of which window has focus so that we can apply pointer updates */

		xfWindow* xfw;
		rdpWindow* window;
		rdpRail* rail = ((rdpContext*) xfc)->rail;
		window = window_list_get_by_extra_id(rail->list, (void*) event->xexpose.window);

		if (window != NULL)
		{
			xfw = (xfWindow*) window->extra;
			xfc->window = xfw;
		}
	}

	return TRUE;
}

static BOOL xf_event_LeaveNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	if (!app)
	{
		xfc->mouse_active = FALSE;
		XUngrabKeyboard(xfc->display, CurrentTime);
	}

	return TRUE;
}

static BOOL xf_event_ConfigureNotify(xfContext* xfc, XEvent* event, BOOL app)
{
        rdpWindow* window;
        rdpRail* rail = ((rdpContext*) xfc)->rail;


/*	This is for resizing the window by dragging the border

	if (xfc->width != event->xconfigure.width)
	{
		xfc->settings->ScalingFactor = (double) event->xconfigure.width / (double) xfc->originalWidth;
		xfc->currentWidth = event->xconfigure.width;
		xfc->currentHeight = event->xconfigure.width;

		xf_draw_screen_scaled(xfc);
	}
*/
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

                XTranslateCoordinates(xfc->display, xfw->handle, 
			RootWindowOfScreen(xfc->screen),
                        0, 0, &xfw->left, &xfw->top, &childWindow);




                xfw->width = event->xconfigure.width;
                xfw->height = event->xconfigure.height;
                xfw->right = xfw->left + xfw->width - 1;
                xfw->bottom = xfw->top + xfw->height - 1;

		DEBUG_X11_LMS("window=0x%X rc={l=%d t=%d r=%d b=%d} w=%u h=%u send_event=%d",
			(UINT32) xfw->handle, xfw->left, xfw->top, xfw->right, xfw->bottom,
			xfw->width, xfw->height, event->xconfigure.send_event);

		/*
		 * Additonal checks for not in a local move and not ignoring configure to send
		 * position update to server, also should the window not be focused then do not
		 * send to server yet(ie. resizing using window decoration).
		 * The server will be updated when the window gets refocused.
		 */
		if (app && xfw->decorations)
		{
			/* moving resizing using window decoration */
			xf_rail_adjust_position(xfc, window);
			window->windowOffsetX = xfw->left;
			window->visibleOffsetX = window->windowOffsetX;
			window->windowOffsetY = xfw->top;
			window->visibleOffsetY = window->windowOffsetY;
			window->windowWidth = xfw->width;
			window->windowHeight = xfw->height;
		}
		else
		{
			if (app && (!event->xconfigure.send_event || xfc->window->local_move.state == LMS_NOT_ACTIVE) 
				&& !xfw->rail_ignore_configure && xfc->focused)
				xf_rail_adjust_position(xfc, window);
		}

        }

        return True;
}

static BOOL xf_event_MapNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	RECTANGLE_16 rect;
	rdpWindow* window;
	rdpUpdate* update = xfc->instance->update;
	rdpRail* rail = ((rdpContext*) xfc)->rail;

	if (!app)
	{
		rect.left = 0;
		rect.top = 0;
		rect.right = xfc->width;
		rect.bottom = xfc->height;

		update->SuppressOutput((rdpContext*) xfc, 1, &rect);
	}
	else
	{
		window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);

		if (window != NULL)
		{
			/* local restore event */

			/* This is now handled as part of the PropertyNotify
			 * Doing this here would inhibit the ability to restore a maximized window
			 * that is minimized back to the maximized state
			 */

			//xf_rail_send_client_system_command(xfc, window->windowId, SC_RESTORE);
			xfWindow* xfw = (xfWindow*) window->extra;
			xfw->is_mapped = TRUE;
		}
	}

	return TRUE;
}

static BOOL xf_event_UnmapNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	rdpWindow* window;
	rdpUpdate* update = xfc->instance->update;
	rdpRail* rail = ((rdpContext*) xfc)->rail;

	xf_keyboard_release_all_keypress(xfc);

	if (!app)
	{
		update->SuppressOutput((rdpContext*) xfc, 0, NULL);
	}
	else
	{
		window = window_list_get_by_extra_id(rail->list, (void*) event->xany.window);

		if (window != NULL)
		{
			xfWindow* xfw = (xfWindow*) window->extra;
			xfw->is_mapped = FALSE;
		}
	}

	return TRUE;
}

static BOOL xf_event_SelectionNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	if (!app)
	{
		if (xf_cliprdr_process_selection_notify(xfc, event))
			return TRUE;
	}

	return TRUE;
}

static BOOL xf_event_SelectionRequest(xfContext* xfc, XEvent* event, BOOL app)
{
	if (!app)
	{
		if (xf_cliprdr_process_selection_request(xfc, event))
			return TRUE;
	}

	return TRUE;
}

static BOOL xf_event_SelectionClear(xfContext* xfc, XEvent* event, BOOL app)
{
	if (!app)
	{
		if (xf_cliprdr_process_selection_clear(xfc, event))
			return TRUE;
	}

	return TRUE;
}

static BOOL xf_event_PropertyNotify(xfContext* xfc, XEvent* event, BOOL app)
{
	/*
	 * This section handles sending the appropriate commands to the rail server
	 * when the window has been minimized, maximized, restored locally
	 * ie. not using the buttons on the rail window itself
	 */

	if (app)
	{
	        rdpWindow* window;
		
		window = xf_rdpWindowFromWindow(xfc, event->xproperty.window);

		if (window == NULL)
			return TRUE;
	
	        if ((((Atom) event->xproperty.atom == xfc->_NET_WM_STATE) && (event->xproperty.state != PropertyDelete)) ||
	            (((Atom) event->xproperty.atom == xfc->WM_STATE) && (event->xproperty.state != PropertyDelete)))
	        {
	        	int i;
	                BOOL status;
	                BOOL maxVert = FALSE;
	                BOOL maxHorz = FALSE;
	                BOOL minimized = FALSE;
	                unsigned long nitems;
	                unsigned long bytes;
	                unsigned char* prop;
	
	                if ((Atom) event->xproperty.atom == xfc->_NET_WM_STATE)
	                {
				status = xf_GetWindowProperty(xfc, event->xproperty.window,
						xfc->_NET_WM_STATE, 12, &nitems, &bytes, &prop);

				if (!status)
				{
					       DEBUG_X11_LMS("No return _NET_WM_STATE, window is not maximized");
				}

				for (i = 0; i < nitems; i++)
				{
					if ((Atom) ((UINT16**) prop)[i] == XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_VERT", False))
					{
						maxVert = TRUE;
					}
	
					if ((Atom) ((UINT16**) prop)[i] == XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
					{
						maxHorz = TRUE;
					}
				}
	
				XFree(prop);
	                }
	
	                if ((Atom) event->xproperty.atom == xfc->WM_STATE)
	                {
				status = xf_GetWindowProperty(xfc, event->xproperty.window, xfc->WM_STATE, 1, &nitems, &bytes, &prop);

				if (!status)
				{
					DEBUG_X11_LMS("No return WM_STATE, window is not minimized");
				}
				else
				{
					/* If the window is in the iconic state */
					if (((UINT32) *prop == 3))
						minimized = TRUE;
					else
						minimized = FALSE;

					XFree(prop);
				}
	                }
	

	                if (maxVert && maxHorz && !minimized && (xfc->window->rail_state != WINDOW_SHOW_MAXIMIZED))
	                {
	                	DEBUG_X11_LMS("Send SC_MAXIMIZE command to rail server.");
	                	xfc->window->rail_state = WINDOW_SHOW_MAXIMIZED;
	                	xf_rail_send_client_system_command(xfc, window->windowId, SC_MAXIMIZE);
	                }
	                else if (minimized && (xfc->window->rail_state != WINDOW_SHOW_MINIMIZED))
	                {
	                	DEBUG_X11_LMS("Send SC_MINIMIZE command to rail server.");
	                	xfc->window->rail_state = WINDOW_SHOW_MINIMIZED;
	                	xf_rail_send_client_system_command(xfc, window->windowId, SC_MINIMIZE);
	                }
	                else if (!minimized && !maxVert && !maxHorz && (xfc->window->rail_state != WINDOW_SHOW))
	                {
	                	DEBUG_X11_LMS("Send SC_RESTORE command to rail server");
	                	xfc->window->rail_state = WINDOW_SHOW;
	                	xf_rail_send_client_system_command(xfc, window->windowId, SC_RESTORE);
	                }
               }       
        }
	else
	{
		if (xf_cliprdr_process_property_notify(xfc, event))
			return TRUE;
	}

	return TRUE;
}

static BOOL xf_event_suppress_events(xfContext* xfc, rdpWindow* window, XEvent*event)
{
	if (!xfc->remote_app)
		return FALSE;

	switch (xfc->window->local_move.state)
	{
		case LMS_NOT_ACTIVE:
			/* No local move in progress, nothing to do */

			/* Prevent Configure from happening during indeterminant state of Horz or Vert Max only */

		        if ( (event->type == ConfigureNotify) && xfc->window->rail_ignore_configure)
                        {
                               DEBUG_X11_LMS("ConfigureNotify Event Ignored");
                               xfc->window->rail_ignore_configure = FALSE;
                               return TRUE;
                        }

			break;

		case LMS_STARTING:
			/* Local move initiated by RDP server, but we have not yet seen any updates from the X server */
			switch(event->type)
			{
				case ConfigureNotify:
					/* Starting to see move events from the X server. Local move is now in progress. */
					xfc->window->local_move.state = LMS_ACTIVE;

					/* Allow these events to be processed during move to keep our state up to date. */
					break;
				case ButtonPress:
				case ButtonRelease:
				case KeyPress:
				case KeyRelease:
				case UnmapNotify:
                	        	/*
                	        	 * A button release event means the X window server did not grab the
                	        	 * mouse before the user released it. In this case we must cancel the
                	        	 * local move. The event will be processed below as normal, below.
                	        	 */
	                        	break;
				case VisibilityNotify:
				case PropertyNotify:
				case Expose:
					/* Allow these events to pass */
					break;
				default:
					/* Eat any other events */
					return TRUE;
			}
			break;

		case LMS_ACTIVE:
			/* Local move is in progress */
			switch(event->type)
			{
				case ConfigureNotify:
				case VisibilityNotify:
				case PropertyNotify:
				case Expose:
				case GravityNotify:
					/* Keep us up to date on position */
					break;
				default:
					DEBUG_X11_LMS("Event Type to break LMS: %s", X11_EVENT_STRINGS[event->type]);
					/* Any other event terminates move */
					xf_rail_end_local_move(xfc, window);
					break;
			}
			break;

		case LMS_TERMINATING:
			/* Already sent RDP end move to server. Allow events to pass. */
			break;
	}	

	return FALSE;
}


BOOL xf_event_process(freerdp* instance, XEvent* event)
{
	BOOL status = TRUE;
	rdpWindow* window;
	xfContext* xfc = (xfContext*) instance->context;
	rdpRail* rail = ((rdpContext*) xfc)->rail;

	if (xfc->remote_app)
	{
		window = window_list_get_by_extra_id(rail->list, (void*) event->xexpose.window);

		if (window)
		{
			/* Update "current" window for cursor change orders */
			xfc->window = (xfWindow*) window->extra;

			if (xf_event_suppress_events(xfc, window, event))
				return TRUE;
		}
	}

	xf_event_execute_action_script(xfc, event);

	if (event->type != MotionNotify)
		DEBUG_X11("%s Event(%d): wnd=0x%04X", X11_EVENT_STRINGS[event->type], event->type, (UINT32) event->xany.window);

	switch (event->type)
	{
		case Expose:
			status = xf_event_Expose(xfc, event, xfc->remote_app);
			break;

		case VisibilityNotify:
			status = xf_event_VisibilityNotify(xfc, event, xfc->remote_app);
			break;

		case MotionNotify:
			status = xf_event_MotionNotify(xfc, event, xfc->remote_app);
			break;
		case ButtonPress:
			status = xf_event_ButtonPress(xfc, event, xfc->remote_app);
			break;

		case ButtonRelease:
			status = xf_event_ButtonRelease(xfc, event, xfc->remote_app);
			break;

		case KeyPress:
			status = xf_event_KeyPress(xfc, event, xfc->remote_app);
			break;

		case KeyRelease:
			status = xf_event_KeyRelease(xfc, event, xfc->remote_app);
			break;

		case FocusIn:
			status = xf_event_FocusIn(xfc, event, xfc->remote_app);
			break;

		case FocusOut:
			status = xf_event_FocusOut(xfc, event, xfc->remote_app);
			break;

		case EnterNotify:
			status = xf_event_EnterNotify(xfc, event, xfc->remote_app);
			break;

		case LeaveNotify:
			status = xf_event_LeaveNotify(xfc, event, xfc->remote_app);
			break;

		case NoExpose:
			break;

		case GraphicsExpose:
			break;

		case ConfigureNotify:
			status = xf_event_ConfigureNotify(xfc, event, xfc->remote_app);
			break;

		case MapNotify:
			status = xf_event_MapNotify(xfc, event, xfc->remote_app);
			break;

		case UnmapNotify:
			status = xf_event_UnmapNotify(xfc, event, xfc->remote_app);
			break;

		case ReparentNotify:
			break;

		case MappingNotify:
			status = xf_event_MappingNotify(xfc, event, xfc->remote_app);
			break;

		case ClientMessage:
			status = xf_event_ClientMessage(xfc, event, xfc->remote_app);
			break;

		case SelectionNotify:
			status = xf_event_SelectionNotify(xfc, event, xfc->remote_app);
			break;

		case SelectionRequest:
			status = xf_event_SelectionRequest(xfc, event, xfc->remote_app);
			break;

		case SelectionClear:
			status = xf_event_SelectionClear(xfc, event, xfc->remote_app);
			break;

		case PropertyNotify:
			status = xf_event_PropertyNotify(xfc, event, xfc->remote_app);
			break;

	}

	xf_input_handle_event(xfc, event);

	XSync(xfc->display, FALSE);

	return status;
}
