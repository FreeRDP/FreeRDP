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

#include <freerdp/log.h>
#include <freerdp/locale/keyboard.h>

#include "xf_rail.h"
#include "xf_window.h"
#include "xf_cliprdr.h"
#include "xf_disp.h"
#include "xf_input.h"
#include "xf_gfx.h"

#include "xf_event.h"
#include "xf_input.h"

#define TAG CLIENT_TAG("x11")

#define CLAMP_COORDINATES(x, y) \
	if (x < 0)                  \
		x = 0;                  \
	if (y < 0)                  \
	y = 0

static const char* x11_event_string(int event)
{
	switch (event)
	{
		case KeyPress:
			return "KeyPress";

		case KeyRelease:
			return "KeyRelease";

		case ButtonPress:
			return "ButtonPress";

		case ButtonRelease:
			return "ButtonRelease";

		case MotionNotify:
			return "MotionNotify";

		case EnterNotify:
			return "EnterNotify";

		case LeaveNotify:
			return "LeaveNotify";

		case FocusIn:
			return "FocusIn";

		case FocusOut:
			return "FocusOut";

		case KeymapNotify:
			return "KeymapNotify";

		case Expose:
			return "Expose";

		case GraphicsExpose:
			return "GraphicsExpose";

		case NoExpose:
			return "NoExpose";

		case VisibilityNotify:
			return "VisibilityNotify";

		case CreateNotify:
			return "CreateNotify";

		case DestroyNotify:
			return "DestroyNotify";

		case UnmapNotify:
			return "UnmapNotify";

		case MapNotify:
			return "MapNotify";

		case MapRequest:
			return "MapRequest";

		case ReparentNotify:
			return "ReparentNotify";

		case ConfigureNotify:
			return "ConfigureNotify";

		case ConfigureRequest:
			return "ConfigureRequest";

		case GravityNotify:
			return "GravityNotify";

		case ResizeRequest:
			return "ResizeRequest";

		case CirculateNotify:
			return "CirculateNotify";

		case CirculateRequest:
			return "CirculateRequest";

		case PropertyNotify:
			return "PropertyNotify";

		case SelectionClear:
			return "SelectionClear";

		case SelectionRequest:
			return "SelectionRequest";

		case SelectionNotify:
			return "SelectionNotify";

		case ColormapNotify:
			return "ColormapNotify";

		case ClientMessage:
			return "ClientMessage";

		case MappingNotify:
			return "MappingNotify";

		case GenericEvent:
			return "GenericEvent";

		default:
			return "UNKNOWN";
	};
}

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_X11(...) \
	do                 \
	{                  \
	} while (0)
#endif

BOOL xf_event_action_script_init(xfContext* xfc)
{
	char* xevent;
	FILE* actionScript;
	char buffer[1024] = { 0 };
	char command[1024] = { 0 };
	xfc->xevents = ArrayList_New(TRUE);

	if (!xfc->xevents)
		return FALSE;

	ArrayList_Object(xfc->xevents)->fnObjectFree = free;
	sprintf_s(command, sizeof(command), "%s xevent", xfc->context.settings->ActionScript);
	actionScript = popen(command, "r");

	if (!actionScript)
		return FALSE;

	while (fgets(buffer, sizeof(buffer), actionScript))
	{
		strtok(buffer, "\n");
		xevent = _strdup(buffer);

		if (!xevent || ArrayList_Add(xfc->xevents, xevent) < 0)
		{
			pclose(actionScript);
			ArrayList_Free(xfc->xevents);
			xfc->xevents = NULL;
			return FALSE;
		}
	}

	pclose(actionScript);
	return TRUE;
}

void xf_event_action_script_free(xfContext* xfc)
{
	if (xfc->xevents)
	{
		ArrayList_Free(xfc->xevents);
		xfc->xevents = NULL;
	}
}

static BOOL xf_event_execute_action_script(xfContext* xfc, const XEvent* event)
{
	int index;
	int count;
	char* name;
	FILE* actionScript;
	BOOL match = FALSE;
	const char* xeventName;
	char buffer[1024] = { 0 };
	char command[1024] = { 0 };

	if (!xfc->actionScriptExists || !xfc->xevents || !xfc->window)
		return FALSE;

	if (event->type > LASTEvent)
		return FALSE;

	xeventName = x11_event_string(event->type);
	count = ArrayList_Count(xfc->xevents);

	for (index = 0; index < count; index++)
	{
		name = (char*)ArrayList_GetItem(xfc->xevents, index);

		if (_stricmp(name, xeventName) == 0)
		{
			match = TRUE;
			break;
		}
	}

	if (!match)
		return FALSE;

	sprintf_s(command, sizeof(command), "%s xevent %s %lu", xfc->context.settings->ActionScript,
	          xeventName, (unsigned long)xfc->window->handle);
	actionScript = popen(command, "r");

	if (!actionScript)
		return FALSE;

	while (fgets(buffer, sizeof(buffer), actionScript))
	{
		strtok(buffer, "\n");
	}

	pclose(actionScript);
	return TRUE;
}

void xf_event_adjust_coordinates(xfContext* xfc, int* x, int* y)
{
	rdpSettings* settings;

	if (!xfc || !xfc->context.settings || !y || !x)
		return;

	settings = xfc->context.settings;

	if (!xfc->remote_app)
	{
#ifdef WITH_XRENDER

		if (xf_picture_transform_required(xfc))
		{
			double xScalingFactor = settings->DesktopWidth / (double)xfc->scaledWidth;
			double yScalingFactor = settings->DesktopHeight / (double)xfc->scaledHeight;
			*x = (int)((*x - xfc->offset_x) * xScalingFactor);
			*y = (int)((*y - xfc->offset_y) * yScalingFactor);
		}

#endif
	}

	CLAMP_COORDINATES(*x, *y);
}
static BOOL xf_event_Expose(xfContext* xfc, const XExposeEvent* event, BOOL app)
{
	int x, y;
	int w, h;
	rdpSettings* settings = xfc->context.settings;

	if (!app && (settings->SmartSizing || settings->MultiTouchGestures))
	{
		x = 0;
		y = 0;
		w = settings->DesktopWidth;
		h = settings->DesktopHeight;
	}
	else
	{
		x = event->x;
		y = event->y;
		w = event->width;
		h = event->height;
	}

	if (!app)
	{
		if (xfc->context.gdi->gfx)
		{
			xf_OutputExpose(xfc, x, y, w, h);
			return TRUE;
		}
		xf_draw_screen(xfc, x, y, w, h);
	}
	else
	{
		xfAppWindow* appWindow;
		appWindow = xf_AppWindowFromX11Window(xfc, event->window);

		if (appWindow)
		{
			xf_UpdateWindowArea(xfc, appWindow, x, y, w, h);
		}
	}

	return TRUE;
}

static BOOL xf_event_VisibilityNotify(xfContext* xfc, const XVisibilityEvent* event, BOOL app)
{
	WINPR_UNUSED(app);
	xfc->unobscured = event->state == VisibilityUnobscured;
	return TRUE;
}

BOOL xf_generic_MotionNotify(xfContext* xfc, int x, int y, int state, Window window, BOOL app)
{
	rdpInput* input;
	Window childWindow;
	input = xfc->context.input;

	if (!xfc->context.settings->MouseMotion)
	{
		if ((state & (Button1Mask | Button2Mask | Button3Mask)) == 0)
			return TRUE;
	}

	if (app)
	{
		/* make sure window exists */
		if (!xf_AppWindowFromX11Window(xfc, window))
			return TRUE;

		/* Translate to desktop coordinates */
		XTranslateCoordinates(xfc->display, window, RootWindowOfScreen(xfc->screen), x, y, &x, &y,
		                      &childWindow);
	}

	xf_event_adjust_coordinates(xfc, &x, &y);
	freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, x, y);

	if (xfc->fullscreen && !app)
	{
		XSetInputFocus(xfc->display, xfc->window->handle, RevertToPointerRoot, CurrentTime);
	}

	return TRUE;
}
static BOOL xf_event_MotionNotify(xfContext* xfc, const XMotionEvent* event, BOOL app)
{
	if (xfc->use_xinput)
		return TRUE;

	if (xfc->window)
		xf_floatbar_set_root_y(xfc->window->floatbar, event->y);

	return xf_generic_MotionNotify(xfc, event->x, event->y, event->state, event->window, app);
}

BOOL xf_generic_ButtonEvent(xfContext* xfc, int x, int y, int button, Window window, BOOL app,
                            BOOL down)
{
	UINT16 flags = 0;
	rdpInput* input;
	Window childWindow;
	size_t i;

	for (i = 0; i < ARRAYSIZE(xfc->button_map); i++)
	{
		const button_map* cur = &xfc->button_map[i];

		if (cur->button == button)
		{
			flags = cur->flags;
			break;
		}
	}

	input = xfc->context.input;

	if (flags != 0)
	{
		if (flags & (PTR_FLAGS_WHEEL | PTR_FLAGS_HWHEEL))
		{
			if (down)
				freerdp_input_send_mouse_event(input, flags, 0, 0);
		}
		else
		{
			BOOL extended = FALSE;

			if (flags & (PTR_XFLAGS_BUTTON1 | PTR_XFLAGS_BUTTON2))
			{
				extended = TRUE;

				if (down)
					flags |= PTR_XFLAGS_DOWN;
			}
			else if (flags & (PTR_FLAGS_BUTTON1 | PTR_FLAGS_BUTTON2 | PTR_FLAGS_BUTTON3))
			{
				if (down)
					flags |= PTR_FLAGS_DOWN;
			}

			if (app)
			{
				/* make sure window exists */
				if (!xf_AppWindowFromX11Window(xfc, window))
					return TRUE;

				/* Translate to desktop coordinates */
				XTranslateCoordinates(xfc->display, window, RootWindowOfScreen(xfc->screen), x, y,
				                      &x, &y, &childWindow);
			}

			xf_event_adjust_coordinates(xfc, &x, &y);

			if (extended)
				freerdp_input_send_extended_mouse_event(input, flags, x, y);
			else
				freerdp_input_send_mouse_event(input, flags, x, y);
		}
	}

	return TRUE;
}
static BOOL xf_event_ButtonPress(xfContext* xfc, const XButtonEvent* event, BOOL app)
{
	if (xfc->use_xinput)
		return TRUE;

	return xf_generic_ButtonEvent(xfc, event->x, event->y, event->button, event->window, app, TRUE);
}

static BOOL xf_event_ButtonRelease(xfContext* xfc, const XButtonEvent* event, BOOL app)
{
	if (xfc->use_xinput)
		return TRUE;

	return xf_generic_ButtonEvent(xfc, event->x, event->y, event->button, event->window, app,
	                              FALSE);
}

static BOOL xf_event_KeyPress(xfContext* xfc, const XKeyEvent* event, BOOL app)
{
	KeySym keysym;
	char str[256];
	WINPR_UNUSED(app);
	XLookupString((XKeyEvent*)event, str, sizeof(str), &keysym, NULL);
	xf_keyboard_key_press(xfc, event->keycode, keysym);
	return TRUE;
}

static BOOL xf_event_KeyRelease(xfContext* xfc, const XKeyEvent* event, BOOL app)
{
	KeySym keysym;
	char str[256];
	WINPR_UNUSED(app);
	XLookupString((XKeyEvent*)event, str, sizeof(str), &keysym, NULL);
	xf_keyboard_key_release(xfc, event->keycode, keysym);
	return TRUE;
}

static BOOL xf_event_FocusIn(xfContext* xfc, const XFocusInEvent* event, BOOL app)
{
	if (event->mode == NotifyGrab)
		return TRUE;

	xfc->focused = TRUE;

	if (xfc->mouse_active && !app)
	{
		if (!xfc->window)
			return FALSE;

		XGrabKeyboard(xfc->display, xfc->window->handle, TRUE, GrabModeAsync, GrabModeAsync,
		              CurrentTime);
	}

	if (app)
	{
		xfAppWindow* appWindow;
		xf_rail_send_activate(xfc, event->window, TRUE);
		appWindow = xf_AppWindowFromX11Window(xfc, event->window);

		/* Update the server with any window changes that occurred while the window was not focused.
		 */
		if (appWindow)
		{
			xf_rail_adjust_position(xfc, appWindow);
		}
	}

	xf_keyboard_focus_in(xfc);
	return TRUE;
}

static BOOL xf_event_FocusOut(xfContext* xfc, const XFocusOutEvent* event, BOOL app)
{
	if (event->mode == NotifyUngrab)
		return TRUE;

	xfc->focused = FALSE;

	if (event->mode == NotifyWhileGrabbed)
		XUngrabKeyboard(xfc->display, CurrentTime);

	xf_keyboard_release_all_keypress(xfc);
	xf_keyboard_clear(xfc);

	if (app)
		xf_rail_send_activate(xfc, event->window, FALSE);

	return TRUE;
}

static BOOL xf_event_MappingNotify(xfContext* xfc, const XMappingEvent* event, BOOL app)
{
	WINPR_UNUSED(app);

	if (event->request == MappingModifier)
	{
		if (xfc->modifierMap)
			XFreeModifiermap(xfc->modifierMap);

		xfc->modifierMap = XGetModifierMapping(xfc->display);
	}

	return TRUE;
}

static BOOL xf_event_ClientMessage(xfContext* xfc, const XClientMessageEvent* event, BOOL app)
{
	if ((event->message_type == xfc->WM_PROTOCOLS) &&
	    ((Atom)event->data.l[0] == xfc->WM_DELETE_WINDOW))
	{
		if (app)
		{
			xfAppWindow* appWindow;
			appWindow = xf_AppWindowFromX11Window(xfc, event->window);

			if (appWindow)
			{
				xf_rail_send_client_system_command(xfc, appWindow->windowId, SC_CLOSE);
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

static BOOL xf_event_EnterNotify(xfContext* xfc, const XEnterWindowEvent* event, BOOL app)
{
	if (!app)
	{
		if (!xfc->window)
			return FALSE;

		xfc->mouse_active = TRUE;

		if (xfc->fullscreen)
			XSetInputFocus(xfc->display, xfc->window->handle, RevertToPointerRoot, CurrentTime);

		if (xfc->focused)
			XGrabKeyboard(xfc->display, xfc->window->handle, TRUE, GrabModeAsync, GrabModeAsync,
			              CurrentTime);
	}
	else
	{
		xfAppWindow* appWindow;
		appWindow = xf_AppWindowFromX11Window(xfc, event->window);

		/* keep track of which window has focus so that we can apply pointer updates */

		if (appWindow)
		{
			xfc->appWindow = appWindow;
		}
	}

	return TRUE;
}

static BOOL xf_event_LeaveNotify(xfContext* xfc, const XLeaveWindowEvent* event, BOOL app)
{
	WINPR_UNUSED(event);

	if (!app)
	{
		xfc->mouse_active = FALSE;
		XUngrabKeyboard(xfc->display, CurrentTime);
	}

	return TRUE;
}

static BOOL xf_event_ConfigureNotify(xfContext* xfc, const XConfigureEvent* event, BOOL app)
{
	Window childWindow;
	xfAppWindow* appWindow;
	rdpSettings* settings;
	settings = xfc->context.settings;

	if (!app)
	{
		if (!xfc->window)
			return FALSE;

		if (xfc->window->left != event->x)
			xfc->window->left = event->x;

		if (xfc->window->top != event->y)
			xfc->window->top = event->y;

		if (xfc->window->width != event->width || xfc->window->height != event->height)
		{
			xfc->window->width = event->width;
			xfc->window->height = event->height;
#ifdef WITH_XRENDER
			xfc->offset_x = 0;
			xfc->offset_y = 0;

			if (xfc->context.settings->SmartSizing || xfc->context.settings->MultiTouchGestures)
			{
				xfc->scaledWidth = xfc->window->width;
				xfc->scaledHeight = xfc->window->height;
				xf_draw_screen(xfc, 0, 0, settings->DesktopWidth, settings->DesktopHeight);
			}
			else
			{
				xfc->scaledWidth = settings->DesktopWidth;
				xfc->scaledHeight = settings->DesktopHeight;
			}

#endif
		}

		if (settings->DynamicResolutionUpdate)
		{
			int alignedWidth, alignedHeight;
			alignedWidth = (xfc->window->width / 2) * 2;
			alignedHeight = (xfc->window->height / 2) * 2;
			/* ask the server to resize using the display channel */
			xf_disp_handle_configureNotify(xfc, alignedWidth, alignedHeight);
		}

		return TRUE;
	}

	appWindow = xf_AppWindowFromX11Window(xfc, event->window);

	if (appWindow)
	{
		/*
		 * ConfigureNotify coordinates are expressed relative to the window parent.
		 * Translate these to root window coordinates.
		 */
		XTranslateCoordinates(xfc->display, appWindow->handle, RootWindowOfScreen(xfc->screen), 0,
		                      0, &appWindow->x, &appWindow->y, &childWindow);
		appWindow->width = event->width;
		appWindow->height = event->height;

		/*
		 * Additional checks for not in a local move and not ignoring configure to send
		 * position update to server, also should the window not be focused then do not
		 * send to server yet (i.e. resizing using window decoration).
		 * The server will be updated when the window gets refocused.
		 */
		if (appWindow->decorations)
		{
			/* moving resizing using window decoration */
			xf_rail_adjust_position(xfc, appWindow);
		}
		else
		{
			if ((!event->send_event || appWindow->local_move.state == LMS_NOT_ACTIVE) &&
			    !appWindow->rail_ignore_configure && xfc->focused)
				xf_rail_adjust_position(xfc, appWindow);
		}
	}

	return TRUE;
}

static BOOL xf_event_MapNotify(xfContext* xfc, const XMapEvent* event, BOOL app)
{
	xfAppWindow* appWindow;

	if (!app)
		gdi_send_suppress_output(xfc->context.gdi, FALSE);
	else
	{
		appWindow = xf_AppWindowFromX11Window(xfc, event->window);

		if (appWindow)
		{
			/* local restore event */
			/* This is now handled as part of the PropertyNotify
			 * Doing this here would inhibit the ability to restore a maximized window
			 * that is minimized back to the maximized state
			 */
			xf_rail_send_client_system_command(xfc, appWindow->windowId, SC_RESTORE);
			appWindow->is_mapped = TRUE;
		}
	}

	return TRUE;
}

static BOOL xf_event_UnmapNotify(xfContext* xfc, const XUnmapEvent* event, BOOL app)
{
	xfAppWindow* appWindow;
	xf_keyboard_release_all_keypress(xfc);

	if (!app)
		gdi_send_suppress_output(xfc->context.gdi, TRUE);
	else
	{
		appWindow = xf_AppWindowFromX11Window(xfc, event->window);

		if (appWindow)
		{
			appWindow->is_mapped = FALSE;
		}
	}

	return TRUE;
}

static BOOL xf_event_PropertyNotify(xfContext* xfc, const XPropertyEvent* event, BOOL app)
{
	/*
	 * This section handles sending the appropriate commands to the rail server
	 * when the window has been minimized, maximized, restored locally
	 * ie. not using the buttons on the rail window itself
	 */
	if ((((Atom)event->atom == xfc->_NET_WM_STATE) && (event->state != PropertyDelete)) ||
	    (((Atom)event->atom == xfc->WM_STATE) && (event->state != PropertyDelete)))
	{
		unsigned long i;
		BOOL status;
		BOOL maxVert = FALSE;
		BOOL maxHorz = FALSE;
		BOOL minimized = FALSE;
		BOOL minimizedChanged = FALSE;
		unsigned long nitems;
		unsigned long bytes;
		unsigned char* prop;
		xfAppWindow* appWindow = NULL;

		if (app)
		{
			appWindow = xf_AppWindowFromX11Window(xfc, event->window);

			if (!appWindow)
				return TRUE;
		}

		if ((Atom)event->atom == xfc->_NET_WM_STATE)
		{
			status = xf_GetWindowProperty(xfc, event->window, xfc->_NET_WM_STATE, 12, &nitems,
			                              &bytes, &prop);

			if (status)
			{
				for (i = 0; i < nitems; i++)
				{
					if ((Atom)((UINT16**)prop)[i] ==
					    XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_VERT", False))
					{
						maxVert = TRUE;
					}

					if ((Atom)((UINT16**)prop)[i] ==
					    XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
					{
						maxHorz = TRUE;
					}
				}

				XFree(prop);
			}
		}

		if ((Atom)event->atom == xfc->WM_STATE)
		{
			status =
			    xf_GetWindowProperty(xfc, event->window, xfc->WM_STATE, 1, &nitems, &bytes, &prop);

			if (status)
			{
				/* If the window is in the iconic state */
				if (((UINT32)*prop == 3))
					minimized = TRUE;
				else
					minimized = FALSE;

				minimizedChanged = TRUE;
				XFree(prop);
			}
		}

		if (app)
		{
			if (maxVert && maxHorz && !minimized &&
			    (appWindow->rail_state != WINDOW_SHOW_MAXIMIZED))
			{
				appWindow->rail_state = WINDOW_SHOW_MAXIMIZED;
				xf_rail_send_client_system_command(xfc, appWindow->windowId, SC_MAXIMIZE);
			}
			else if (minimized && (appWindow->rail_state != WINDOW_SHOW_MINIMIZED))
			{
				appWindow->rail_state = WINDOW_SHOW_MINIMIZED;
				xf_rail_send_client_system_command(xfc, appWindow->windowId, SC_MINIMIZE);
			}
			else if (!minimized && !maxVert && !maxHorz && (appWindow->rail_state != WINDOW_SHOW) &&
			         (appWindow->rail_state != WINDOW_HIDE))
			{
				appWindow->rail_state = WINDOW_SHOW;
				xf_rail_send_client_system_command(xfc, appWindow->windowId, SC_RESTORE);
			}
		}
		else if (minimizedChanged)
			gdi_send_suppress_output(xfc->context.gdi, minimized);
	}

	return TRUE;
}

static BOOL xf_event_suppress_events(xfContext* xfc, xfAppWindow* appWindow, const XEvent* event)
{
	if (!xfc->remote_app)
		return FALSE;

	switch (appWindow->local_move.state)
	{
		case LMS_NOT_ACTIVE:

			/* No local move in progress, nothing to do */

			/* Prevent Configure from happening during indeterminant state of Horz or Vert Max only
			 */
			if ((event->type == ConfigureNotify) && appWindow->rail_ignore_configure)
			{
				appWindow->rail_ignore_configure = FALSE;
				return TRUE;
			}

			break;

		case LMS_STARTING:

			/* Local move initiated by RDP server, but we have not yet seen any updates from the X
			 * server */
			switch (event->type)
			{
				case ConfigureNotify:
					/* Starting to see move events from the X server. Local move is now in progress.
					 */
					appWindow->local_move.state = LMS_ACTIVE;
					/* Allow these events to be processed during move to keep our state up to date.
					 */
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
			switch (event->type)
			{
				case ConfigureNotify:
				case VisibilityNotify:
				case PropertyNotify:
				case Expose:
				case GravityNotify:
					/* Keep us up to date on position */
					break;

				default:
					/* Any other event terminates move */
					xf_rail_end_local_move(xfc, appWindow);
					break;
			}

			break;

		case LMS_TERMINATING:
			/* Already sent RDP end move to server. Allow events to pass. */
			break;
	}

	return FALSE;
}

BOOL xf_event_process(freerdp* instance, const XEvent* event)
{
	BOOL status = TRUE;
	xfAppWindow* appWindow;
	xfContext* xfc = (xfContext*)instance->context;
	rdpSettings* settings = xfc->context.settings;

	if (xfc->remote_app)
	{
		appWindow = xf_AppWindowFromX11Window(xfc, event->xany.window);

		if (appWindow)
		{
			/* Update "current" window for cursor change orders */
			xfc->appWindow = appWindow;

			if (xf_event_suppress_events(xfc, appWindow, event))
				return TRUE;
		}
	}

	if (xfc->window)
	{
		if (xf_floatbar_check_event(xfc->window->floatbar, event))
		{
			xf_floatbar_event_process(xfc->window->floatbar, event);
			return TRUE;
		}
	}

	xf_event_execute_action_script(xfc, event);

	if (event->type != MotionNotify)
	{
		DEBUG_X11("%s Event(%d): wnd=0x%08lX", x11_event_string(event->type), event->type,
		          (unsigned long)event->xany.window);
	}

	switch (event->type)
	{
		case Expose:
			status = xf_event_Expose(xfc, &event->xexpose, xfc->remote_app);
			break;

		case VisibilityNotify:
			status = xf_event_VisibilityNotify(xfc, &event->xvisibility, xfc->remote_app);
			break;

		case MotionNotify:
			status = xf_event_MotionNotify(xfc, &event->xmotion, xfc->remote_app);
			break;

		case ButtonPress:
			status = xf_event_ButtonPress(xfc, &event->xbutton, xfc->remote_app);
			break;

		case ButtonRelease:
			status = xf_event_ButtonRelease(xfc, &event->xbutton, xfc->remote_app);
			break;

		case KeyPress:
			status = xf_event_KeyPress(xfc, &event->xkey, xfc->remote_app);
			break;

		case KeyRelease:
			status = xf_event_KeyRelease(xfc, &event->xkey, xfc->remote_app);
			break;

		case FocusIn:
			status = xf_event_FocusIn(xfc, &event->xfocus, xfc->remote_app);
			break;

		case FocusOut:
			status = xf_event_FocusOut(xfc, &event->xfocus, xfc->remote_app);
			break;

		case EnterNotify:
			status = xf_event_EnterNotify(xfc, &event->xcrossing, xfc->remote_app);
			break;

		case LeaveNotify:
			status = xf_event_LeaveNotify(xfc, &event->xcrossing, xfc->remote_app);
			break;

		case NoExpose:
			break;

		case GraphicsExpose:
			break;

		case ConfigureNotify:
			status = xf_event_ConfigureNotify(xfc, &event->xconfigure, xfc->remote_app);
			break;

		case MapNotify:
			status = xf_event_MapNotify(xfc, &event->xmap, xfc->remote_app);
			break;

		case UnmapNotify:
			status = xf_event_UnmapNotify(xfc, &event->xunmap, xfc->remote_app);
			break;

		case ReparentNotify:
			break;

		case MappingNotify:
			status = xf_event_MappingNotify(xfc, &event->xmapping, xfc->remote_app);
			break;

		case ClientMessage:
			status = xf_event_ClientMessage(xfc, &event->xclient, xfc->remote_app);
			break;

		case PropertyNotify:
			status = xf_event_PropertyNotify(xfc, &event->xproperty, xfc->remote_app);
			break;

		default:
			if (settings->SupportDisplayControl)
				xf_disp_handle_xevent(xfc, event);

			break;
	}

	xf_cliprdr_handle_xevent(xfc, event);
	xf_input_handle_event(xfc, event);
	XSync(xfc->display, FALSE);
	return status;
}
