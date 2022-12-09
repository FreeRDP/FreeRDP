/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Windows
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012 HP Development Company, LLC
 * Copyright 2016 Thincast Technologies GmbH
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
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

#include <freerdp/config.h>

#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <winpr/assert.h>
#include <winpr/thread.h>
#include <winpr/crt.h>
#include <winpr/string.h>

#include <freerdp/rail.h>
#include <freerdp/log.h>

#ifdef WITH_XEXT
#include <X11/extensions/shape.h>
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#include "xf_input.h"
#endif

#include "xf_gfx.h"
#include "xf_rail.h"
#include "xf_input.h"
#include "xf_keyboard.h"

#define TAG CLIENT_TAG("x11")

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_X11(...) \
	do                 \
	{                  \
	} while (0)
#endif

#include "FreeRDP_Icon_256px.h"
#define xf_icon_prop FreeRDP_Icon_256px_prop

#include "xf_window.h"

/* Extended Window Manager Hints: http://standards.freedesktop.org/wm-spec/wm-spec-1.3.html */

/* bit definitions for MwmHints.flags */
#define MWM_HINTS_FUNCTIONS (1L << 0)
#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_HINTS_INPUT_MODE (1L << 2)
#define MWM_HINTS_STATUS (1L << 3)

/* bit definitions for MwmHints.functions */
#define MWM_FUNC_ALL (1L << 0)
#define MWM_FUNC_RESIZE (1L << 1)
#define MWM_FUNC_MOVE (1L << 2)
#define MWM_FUNC_MINIMIZE (1L << 3)
#define MWM_FUNC_MAXIMIZE (1L << 4)
#define MWM_FUNC_CLOSE (1L << 5)

/* bit definitions for MwmHints.decorations */
#define MWM_DECOR_ALL (1L << 0)
#define MWM_DECOR_BORDER (1L << 1)
#define MWM_DECOR_RESIZEH (1L << 2)
#define MWM_DECOR_TITLE (1L << 3)
#define MWM_DECOR_MENU (1L << 4)
#define MWM_DECOR_MINIMIZE (1L << 5)
#define MWM_DECOR_MAXIMIZE (1L << 6)

#define PROP_MOTIF_WM_HINTS_ELEMENTS 5

typedef struct
{
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long inputMode;
	unsigned long status;
} PropMotifWmHints;

static void xf_SetWindowTitleText(xfContext* xfc, Window window, const char* name)
{
	const size_t i = strnlen(name, MAX_PATH);
	XStoreName(xfc->display, window, name);
	Atom wm_Name = xfc->_NET_WM_NAME;
	Atom utf8Str = xfc->UTF8_STRING;
	XChangeProperty(xfc->display, window, wm_Name, utf8Str, 8, PropModeReplace,
	                (const unsigned char*)name, (int)i);
}

/**
 * Post an event from the client to the X server
 */
void xf_SendClientEvent(xfContext* xfc, Window window, Atom atom, unsigned int numArgs, ...)
{
	XEvent xevent = { 0 };
	unsigned int i;
	va_list argp;
	va_start(argp, numArgs);

	xevent.xclient.type = ClientMessage;
	xevent.xclient.serial = 0;
	xevent.xclient.send_event = False;
	xevent.xclient.display = xfc->display;
	xevent.xclient.window = window;
	xevent.xclient.message_type = atom;
	xevent.xclient.format = 32;

	for (i = 0; i < numArgs; i++)
	{
		xevent.xclient.data.l[i] = va_arg(argp, int);
	}

	DEBUG_X11("Send ClientMessage Event: wnd=0x%04lX", (unsigned long)xevent.xclient.window);
	XSendEvent(xfc->display, RootWindowOfScreen(xfc->screen), False,
	           SubstructureRedirectMask | SubstructureNotifyMask, &xevent);
	XSync(xfc->display, False);
	va_end(argp);
}

void xf_SetWindowMinimized(xfContext* xfc, xfWindow* window)
{
	XIconifyWindow(xfc->display, window->handle, xfc->screen_number);
}

void xf_SetWindowFullscreen(xfContext* xfc, xfWindow* window, BOOL fullscreen)
{
	UINT32 i;
	const rdpSettings* settings;
	int startX, startY;
	UINT32 width = window->width;
	UINT32 height = window->height;

	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	/* xfc->decorations is set by caller depending on settings and whether it is fullscreen or not
	 */
	window->decorations = xfc->decorations;
	/* show/hide decorations (e.g. title bar) as guided by xfc->decorations */
	xf_SetWindowDecorations(xfc, window->handle, window->decorations);
	DEBUG_X11(TAG, "X window decoration set to %d", (int)window->decorations);
	xf_floatbar_toggle_fullscreen(xfc->window->floatbar, fullscreen);

	if (fullscreen)
	{
		xfc->savedWidth = xfc->window->width;
		xfc->savedHeight = xfc->window->height;
		xfc->savedPosX = xfc->window->left;
		xfc->savedPosY = xfc->window->top;

		startX = (settings->DesktopPosX != UINT32_MAX) ? settings->DesktopPosX : 0;
		startY = (settings->DesktopPosY != UINT32_MAX) ? settings->DesktopPosY : 0;
	}
	else
	{
		width = xfc->savedWidth;
		height = xfc->savedHeight;
		startX = xfc->savedPosX;
		startY = xfc->savedPosY;
	}

	/* Determine the x,y starting location for the fullscreen window */
	if (fullscreen)
	{
		/* Initialize startX and startY with reasonable values */
		startX = settings->MonitorDefArray[0].x;
		startY = settings->MonitorDefArray[0].y;

		/* Search all monitors to find the lowest startX and startY values */
		for (i = 0; i < settings->MonitorCount; i++)
		{
			startX = MIN(startX, settings->MonitorDefArray[i].x);
			startY = MIN(startY, settings->MonitorDefArray[i].y);
		}

		/* Lastly apply any monitor shift(translation from remote to local coordinate system)
		 *  to startX and startY values
		 */
		startX += settings->MonitorLocalShiftX;
		startY += settings->MonitorLocalShiftY;
	}

	/*
	  It is safe to proceed with simply toogling _NET_WM_STATE_FULLSCREEN window state on the
	  following conditions:
	       - The window manager supports multiple monitor full screen
	       - The user requested to use a single monitor to render the remote desktop
	 */
	if (xfc->_NET_WM_FULLSCREEN_MONITORS != None || settings->MonitorCount == 1)
	{
		xf_ResizeDesktopWindow(xfc, window, width, height);

		if (fullscreen)
		{
			/* enter full screen: move the window before adding NET_WM_STATE_FULLSCREEN */
			XMoveWindow(xfc->display, window->handle, startX, startY);
		}

		/* Set the fullscreen state */
		xf_SendClientEvent(xfc, window->handle, xfc->_NET_WM_STATE, 4,
		                   fullscreen ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
		                   xfc->_NET_WM_STATE_FULLSCREEN, 0, 0);

		if (!fullscreen)
		{
			/* leave full screen: move the window after removing NET_WM_STATE_FULLSCREEN
			 * Resize the window again, the previous call to xf_SendClientEvent might have
			 * changed the window size (borders, ...)
			 */
			xf_ResizeDesktopWindow(xfc, window, width, height);
			XMoveWindow(xfc->display, window->handle, startX, startY);
		}

		/* Set monitor bounds */
		if (settings->MonitorCount > 1)
		{
			xf_SendClientEvent(xfc, window->handle, xfc->_NET_WM_FULLSCREEN_MONITORS, 5,
			                   xfc->fullscreenMonitors.top, xfc->fullscreenMonitors.bottom,
			                   xfc->fullscreenMonitors.left, xfc->fullscreenMonitors.right, 1);
		}
	}
	else
	{
		if (fullscreen)
		{
			xf_SetWindowDecorations(xfc, window->handle, FALSE);

			if (xfc->fullscreenMonitors.top)
			{
				xf_SendClientEvent(xfc, window->handle, xfc->_NET_WM_STATE, 4, _NET_WM_STATE_ADD,
				                   xfc->fullscreenMonitors.top, 0, 0);
			}
			else
			{
				XSetWindowAttributes xswa;
				xswa.override_redirect = True;
				XChangeWindowAttributes(xfc->display, window->handle, CWOverrideRedirect, &xswa);
				XRaiseWindow(xfc->display, window->handle);
				xswa.override_redirect = False;
				XChangeWindowAttributes(xfc->display, window->handle, CWOverrideRedirect, &xswa);
			}

			/* if window is in maximized state, save and remove */
			if (xfc->_NET_WM_STATE_MAXIMIZED_VERT != None)
			{
				BYTE state;
				unsigned long nitems;
				unsigned long bytes;
				BYTE* prop;

				if (xf_GetWindowProperty(xfc, window->handle, xfc->_NET_WM_STATE, 255, &nitems,
				                         &bytes, &prop))
				{
					state = 0;

					while (nitems-- > 0)
					{
						if (((Atom*)prop)[nitems] == xfc->_NET_WM_STATE_MAXIMIZED_VERT)
							state |= 0x01;

						if (((Atom*)prop)[nitems] == xfc->_NET_WM_STATE_MAXIMIZED_HORZ)
							state |= 0x02;
					}

					if (state)
					{
						xf_SendClientEvent(xfc, window->handle, xfc->_NET_WM_STATE, 4,
						                   _NET_WM_STATE_REMOVE, xfc->_NET_WM_STATE_MAXIMIZED_VERT,
						                   0, 0);
						xf_SendClientEvent(xfc, window->handle, xfc->_NET_WM_STATE, 4,
						                   _NET_WM_STATE_REMOVE, xfc->_NET_WM_STATE_MAXIMIZED_HORZ,
						                   0, 0);
						xfc->savedMaximizedState = state;
					}

					XFree(prop);
				}
			}

			width = xfc->vscreen.area.right - xfc->vscreen.area.left + 1;
			height = xfc->vscreen.area.bottom - xfc->vscreen.area.top + 1;
			DEBUG_X11("X window move and resize %dx%d@%dx%d", startX, startY, width, height);
			xf_ResizeDesktopWindow(xfc, window, width, height);
			XMoveWindow(xfc->display, window->handle, startX, startY);
		}
		else
		{
			xf_SetWindowDecorations(xfc, window->handle, window->decorations);
			xf_ResizeDesktopWindow(xfc, window, width, height);
			XMoveWindow(xfc->display, window->handle, startX, startY);

			if (xfc->fullscreenMonitors.top)
			{
				xf_SendClientEvent(xfc, window->handle, xfc->_NET_WM_STATE, 4, _NET_WM_STATE_REMOVE,
				                   xfc->fullscreenMonitors.top, 0, 0);
			}

			/* restore maximized state, if the window was maximized before setting fullscreen */
			if (xfc->savedMaximizedState & 0x01)
			{
				xf_SendClientEvent(xfc, window->handle, xfc->_NET_WM_STATE, 4, _NET_WM_STATE_ADD,
				                   xfc->_NET_WM_STATE_MAXIMIZED_VERT, 0, 0);
			}

			if (xfc->savedMaximizedState & 0x02)
			{
				xf_SendClientEvent(xfc, window->handle, xfc->_NET_WM_STATE, 4, _NET_WM_STATE_ADD,
				                   xfc->_NET_WM_STATE_MAXIMIZED_HORZ, 0, 0);
			}

			xfc->savedMaximizedState = 0;
		}
	}
}

/* http://tronche.com/gui/x/xlib/window-information/XGetWindowProperty.html */

BOOL xf_GetWindowProperty(xfContext* xfc, Window window, Atom property, int length,
                          unsigned long* nitems, unsigned long* bytes, BYTE** prop)
{
	int status = 0;
	Atom actual_type = None;
	int actual_format = 0;

	if (property == None)
		return FALSE;

	status = XGetWindowProperty(xfc->display, window, property, 0, length, False, AnyPropertyType,
	                            &actual_type, &actual_format, nitems, bytes, prop);

	if (status != Success)
		return FALSE;

	if (actual_type == None)
	{
		WLog_DBG(TAG, "Property %lu does not exist", (unsigned long)property);
		return FALSE;
	}

	return TRUE;
}

BOOL xf_GetCurrentDesktop(xfContext* xfc)
{
	BOOL status;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char* prop;
	status = xf_GetWindowProperty(xfc, DefaultRootWindow(xfc->display), xfc->_NET_CURRENT_DESKTOP,
	                              1, &nitems, &bytes, &prop);

	if (!status)
		return FALSE;

	xfc->current_desktop = (int)*prop;
	free(prop);
	return TRUE;
}

BOOL xf_GetWorkArea(xfContext* xfc)
{
	long* plong;
	BOOL status;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char* prop;
	status = xf_GetCurrentDesktop(xfc);

	if (!status)
		return FALSE;

	status = xf_GetWindowProperty(xfc, DefaultRootWindow(xfc->display), xfc->_NET_WORKAREA, 32 * 4,
	                              &nitems, &bytes, &prop);

	if (!status)
		return FALSE;

	if ((xfc->current_desktop * 4 + 3) >= (INT64)nitems)
	{
		free(prop);
		return FALSE;
	}

	plong = (long*)prop;
	xfc->workArea.x = plong[xfc->current_desktop * 4 + 0];
	xfc->workArea.y = plong[xfc->current_desktop * 4 + 1];
	xfc->workArea.width = plong[xfc->current_desktop * 4 + 2];
	xfc->workArea.height = plong[xfc->current_desktop * 4 + 3];
	free(prop);
	return TRUE;
}

void xf_SetWindowDecorations(xfContext* xfc, Window window, BOOL show)
{
	PropMotifWmHints hints;
	hints.decorations = (show) ? MWM_DECOR_ALL : 0;
	hints.functions = MWM_FUNC_ALL;
	hints.flags = MWM_HINTS_DECORATIONS | MWM_HINTS_FUNCTIONS;
	hints.inputMode = 0;
	hints.status = 0;
	XChangeProperty(xfc->display, window, xfc->_MOTIF_WM_HINTS, xfc->_MOTIF_WM_HINTS, 32,
	                PropModeReplace, (BYTE*)&hints, PROP_MOTIF_WM_HINTS_ELEMENTS);
}

void xf_SetWindowUnlisted(xfContext* xfc, Window window)
{
	Atom window_state[2];
	window_state[0] = xfc->_NET_WM_STATE_SKIP_PAGER;
	window_state[1] = xfc->_NET_WM_STATE_SKIP_TASKBAR;
	XChangeProperty(xfc->display, window, xfc->_NET_WM_STATE, XA_ATOM, 32, PropModeReplace,
	                (BYTE*)&window_state, 2);
}

static void xf_SetWindowPID(xfContext* xfc, Window window, pid_t pid)
{
	Atom am_wm_pid;

	if (!pid)
		pid = getpid();

	am_wm_pid = xfc->_NET_WM_PID;
	XChangeProperty(xfc->display, window, am_wm_pid, XA_CARDINAL, 32, PropModeReplace, (BYTE*)&pid,
	                1);
}

static const char* get_shm_id(void)
{
	static char shm_id[64];
	sprintf_s(shm_id, sizeof(shm_id), "/com.freerdp.xfreerdp.tsmf_%016X", GetCurrentProcessId());
	return shm_id;
}

Window xf_CreateDummyWindow(xfContext* xfc)
{
	return XCreateWindow(xfc->display, RootWindowOfScreen(xfc->screen), xfc->workArea.x,
	                     xfc->workArea.y, 1, 1, 0, xfc->depth, InputOutput, xfc->visual,
	                     xfc->attribs_mask, &xfc->attribs);
}

void xf_DestroyDummyWindow(xfContext* xfc, Window window)
{
	if (window)
		XDestroyWindow(xfc->display, window);
}

xfWindow* xf_CreateDesktopWindow(xfContext* xfc, char* name, int width, int height)
{
	XEvent xevent;
	int input_mask;
	xfWindow* window;
	Window parentWindow;
	XClassHint* classHints;
	rdpSettings* settings;
	window = (xfWindow*)calloc(1, sizeof(xfWindow));

	if (!window)
		return NULL;

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	parentWindow = (Window)settings->ParentWindowId;
	window->width = width;
	window->height = height;
	window->decorations = xfc->decorations;
	window->is_mapped = FALSE;
	window->is_transient = FALSE;

	WINPR_ASSERT(xfc->depth != 0);
	window->handle =
	    XCreateWindow(xfc->display, RootWindowOfScreen(xfc->screen), xfc->workArea.x,
	                  xfc->workArea.y, xfc->workArea.width, xfc->workArea.height, 0, xfc->depth,
	                  InputOutput, xfc->visual, xfc->attribs_mask, &xfc->attribs);
	window->shmid = shm_open(get_shm_id(), (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE));

	if (window->shmid < 0)
	{
		DEBUG_X11("xf_CreateDesktopWindow: failed to get access to shared memory - shmget()\n");
	}
	else
	{
		int rc = ftruncate(window->shmid, sizeof(window->handle));
		if (rc != 0)
		{
			DEBUG_X11("%s: ftruncate failed with %s [%d]", __FUNCTION__, strerror(rc), rc);
		}
		else
		{
			void* mem = mmap(0, sizeof(window->handle), PROT_READ | PROT_WRITE, MAP_SHARED,
			                 window->shmid, 0);

			if (mem == MAP_FAILED)
			{
				DEBUG_X11(
				    "xf_CreateDesktopWindow: failed to assign pointer to the memory address - "
				    "shmat()\n");
			}
			else
			{
				window->xfwin = mem;
				*window->xfwin = window->handle;
			}
		}
	}

	classHints = XAllocClassHint();

	if (classHints)
	{
		classHints->res_name = "xfreerdp";

		if (settings->WmClass)
			classHints->res_class = settings->WmClass;
		else
			classHints->res_class = "xfreerdp";

		XSetClassHint(xfc->display, window->handle, classHints);
		XFree(classHints);
	}

	xf_ResizeDesktopWindow(xfc, window, width, height);
	xf_SetWindowDecorations(xfc, window->handle, window->decorations);
	xf_SetWindowPID(xfc, window->handle, 0);
	input_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
	             VisibilityChangeMask | FocusChangeMask | StructureNotifyMask | PointerMotionMask |
	             ExposureMask | PropertyChangeMask;

	if (xfc->grab_keyboard)
		input_mask |= EnterWindowMask | LeaveWindowMask;

	XChangeProperty(xfc->display, window->handle, xfc->_NET_WM_ICON, XA_CARDINAL, 32,
	                PropModeReplace, (BYTE*)xf_icon_prop, ARRAYSIZE(xf_icon_prop));

	if (parentWindow)
		XReparentWindow(xfc->display, window->handle, parentWindow, 0, 0);

	XSelectInput(xfc->display, window->handle, input_mask);
	XClearWindow(xfc->display, window->handle);
	xf_SetWindowTitleText(xfc, window->handle, name);
	XMapWindow(xfc->display, window->handle);
	xf_input_init(xfc, window->handle);

	/*
	 * NOTE: This must be done here to handle reparenting the window,
	 * so that we don't miss the event and hang waiting for the next one
	 */
	do
	{
		XMaskEvent(xfc->display, VisibilityChangeMask, &xevent);
	} while (xevent.type != VisibilityNotify);

	/*
	 * The XCreateWindow call will start the window in the upper-left corner of our current
	 * monitor instead of the upper-left monitor for remote app mode (which uses all monitors).
	 * This extra call after the window is mapped will position the login window correctly
	 */
	if (settings->RemoteApplicationMode)
	{
		XMoveWindow(xfc->display, window->handle, 0, 0);
	}
	else if (settings->DesktopPosX != UINT32_MAX && settings->DesktopPosY != UINT32_MAX)
	{
		XMoveWindow(xfc->display, window->handle, settings->DesktopPosX, settings->DesktopPosY);
	}

	window->floatbar = xf_floatbar_new(xfc, window->handle, name,
	                                   freerdp_settings_get_uint32(settings, FreeRDP_Floatbar));

	if (xfc->_XWAYLAND_MAY_GRAB_KEYBOARD)
		xf_SendClientEvent(xfc, window->handle, xfc->_XWAYLAND_MAY_GRAB_KEYBOARD, 1, 1);

	return window;
}

void xf_ResizeDesktopWindow(xfContext* xfc, xfWindow* window, int width, int height)
{
	XSizeHints* size_hints;
	rdpSettings* settings = NULL;

	if (!xfc || !window)
		return;

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (!(size_hints = XAllocSizeHints()))
		return;

	size_hints->flags = PMinSize | PMaxSize | PWinGravity;
	size_hints->win_gravity = NorthWestGravity;
	size_hints->min_width = size_hints->min_height = 1;
	size_hints->max_width = size_hints->max_height = 16384;
	XResizeWindow(xfc->display, window->handle, width, height);
#ifdef WITH_XRENDER

	if (!settings->SmartSizing && !settings->DynamicResolutionUpdate)
#endif
	{
		if (!xfc->fullscreen)
		{
			/* min == max is an hint for the WM to indicate that the window should
			 * not be resizable */
			size_hints->min_width = size_hints->max_width = width;
			size_hints->min_height = size_hints->max_height = height;
		}
	}

	XSetWMNormalHints(xfc->display, window->handle, size_hints);
	XFree(size_hints);
}

void xf_DestroyDesktopWindow(xfContext* xfc, xfWindow* window)
{
	if (!window)
		return;

	if (xfc->window == window)
		xfc->window = NULL;

	xf_floatbar_free(window->floatbar);

	if (window->gc)
		XFreeGC(xfc->display, window->gc);

	if (window->handle)
	{
		XUnmapWindow(xfc->display, window->handle);
		XDestroyWindow(xfc->display, window->handle);
	}

	if (window->xfwin)
		munmap(0, sizeof(*window->xfwin));

	if (window->shmid >= 0)
		close(window->shmid);

	shm_unlink(get_shm_id());
	window->xfwin = (Window*)-1;
	window->shmid = -1;
	free(window);
}

void xf_SetWindowStyle(xfContext* xfc, xfAppWindow* appWindow, UINT32 style, UINT32 ex_style)
{
	Atom window_type;
	BOOL redirect = FALSE;

	if ((ex_style & WS_EX_NOACTIVATE) || (ex_style & WS_EX_TOOLWINDOW))
	{
		redirect = TRUE;
		appWindow->is_transient = TRUE;
		xf_SetWindowUnlisted(xfc, appWindow->handle);
		window_type = xfc->_NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
	}
	/*
	 * TOPMOST window that is not a tool window is treated like a regular window (i.e. task
	 * manager). Want to do this here, since the window may have type WS_POPUP
	 */
	else if (ex_style & WS_EX_TOPMOST)
	{
		window_type = xfc->_NET_WM_WINDOW_TYPE_NORMAL;
	}
	else if (style & WS_POPUP)
	{
		/* this includes dialogs, popups, etc, that need to be full-fledged windows */
		appWindow->is_transient = TRUE;
		window_type = xfc->_NET_WM_WINDOW_TYPE_DIALOG;
		xf_SetWindowUnlisted(xfc, appWindow->handle);
	}
	else
	{
		window_type = xfc->_NET_WM_WINDOW_TYPE_NORMAL;
	}

	{
		/*
		 * Tooltips and menu items should be unmanaged windows
		 * (called "override redirect" in X windows parlance)
		 * If they are managed, there are issues with window focus that
		 * cause the windows to behave improperly.  For example, a mouse
		 * press will dismiss a drop-down menu because the RDP server
		 * sees that as a focus out event from the window owning the
		 * dropdown.
		 */
		XSetWindowAttributes attrs;
		attrs.override_redirect = redirect ? True : False;
		XChangeWindowAttributes(xfc->display, appWindow->handle, CWOverrideRedirect, &attrs);
	}

	XChangeProperty(xfc->display, appWindow->handle, xfc->_NET_WM_WINDOW_TYPE, XA_ATOM, 32,
	                PropModeReplace, (BYTE*)&window_type, 1);
}

void xf_SetWindowText(xfContext* xfc, xfAppWindow* appWindow, const char* name)
{
	xf_SetWindowTitleText(xfc, appWindow->handle, name);
}

static void xf_FixWindowCoordinates(xfContext* xfc, int* x, int* y, int* width, int* height)
{
	int vscreen_width;
	int vscreen_height;
	vscreen_width = xfc->vscreen.area.right - xfc->vscreen.area.left + 1;
	vscreen_height = xfc->vscreen.area.bottom - xfc->vscreen.area.top + 1;

	if (*x < xfc->vscreen.area.left)
	{
		*width += *x;
		*x = xfc->vscreen.area.left;
	}

	if (*y < xfc->vscreen.area.top)
	{
		*height += *y;
		*y = xfc->vscreen.area.top;
	}

	if (*width > vscreen_width)
	{
		*width = vscreen_width;
	}

	if (*height > vscreen_height)
	{
		*height = vscreen_height;
	}

	if (*width < 1)
	{
		*width = 1;
	}

	if (*height < 1)
	{
		*height = 1;
	}
}

int xf_AppWindowInit(xfContext* xfc, xfAppWindow* appWindow)
{
	if (!xfc || !appWindow)
		return -1;

	xf_SetWindowDecorations(xfc, appWindow->handle, appWindow->decorations);
	xf_SetWindowStyle(xfc, appWindow, appWindow->dwStyle, appWindow->dwExStyle);
	xf_SetWindowPID(xfc, appWindow->handle, 0);
	xf_ShowWindow(xfc, appWindow, WINDOW_SHOW);
	XClearWindow(xfc->display, appWindow->handle);
	XMapWindow(xfc->display, appWindow->handle);
	/* Move doesn't seem to work until window is mapped. */
	xf_MoveWindow(xfc, appWindow, appWindow->x, appWindow->y, appWindow->width, appWindow->height);
	xf_SetWindowText(xfc, appWindow, appWindow->title);
	return 1;
}

BOOL xf_AppWindowCreate(xfContext* xfc, xfAppWindow* appWindow)
{
	XGCValues gcv = { 0 };
	int input_mask;
	XWMHints* InputModeHint;
	XClassHint* class_hints;
	const rdpSettings* settings;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(appWindow);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	xf_FixWindowCoordinates(xfc, &appWindow->x, &appWindow->y, &appWindow->width,
	                        &appWindow->height);
	appWindow->shmid = -1;
	appWindow->decorations = FALSE;
	appWindow->fullscreen = FALSE;
	appWindow->local_move.state = LMS_NOT_ACTIVE;
	appWindow->is_mapped = FALSE;
	appWindow->is_transient = FALSE;
	appWindow->rail_state = 0;
	appWindow->maxVert = FALSE;
	appWindow->maxHorz = FALSE;
	appWindow->minimized = FALSE;
	appWindow->rail_ignore_configure = FALSE;

	WINPR_ASSERT(xfc->depth != 0);
	appWindow->handle =
	    XCreateWindow(xfc->display, RootWindowOfScreen(xfc->screen), appWindow->x, appWindow->y,
	                  appWindow->width, appWindow->height, 0, xfc->depth, InputOutput, xfc->visual,
	                  xfc->attribs_mask, &xfc->attribs);

	if (!appWindow->handle)
		return FALSE;

	appWindow->gc = XCreateGC(xfc->display, appWindow->handle, GCGraphicsExposures, &gcv);

	if (!xf_AppWindowResize(xfc, appWindow))
		return FALSE;

	class_hints = XAllocClassHint();

	if (class_hints)
	{
		char* class = NULL;

		if (settings->WmClass)
		{
			class_hints->res_class = settings->WmClass;
		}
		else
		{
			class = malloc(sizeof("RAIL:00000000"));
			sprintf_s(class, sizeof("RAIL:00000000"), "RAIL:%08" PRIX64 "", appWindow->windowId);
			class_hints->res_class = class;
		}

		class_hints->res_name = "RAIL";
		XSetClassHint(xfc->display, appWindow->handle, class_hints);
		XFree(class_hints);
		free(class);
	}

	/* Set the input mode hint for the WM */
	InputModeHint = XAllocWMHints();
	InputModeHint->flags = (1L << 0);
	InputModeHint->input = True;
	XSetWMHints(xfc->display, appWindow->handle, InputModeHint);
	XFree(InputModeHint);
	XSetWMProtocols(xfc->display, appWindow->handle, &(xfc->WM_DELETE_WINDOW), 1);
	input_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
	             EnterWindowMask | LeaveWindowMask | PointerMotionMask | Button1MotionMask |
	             Button2MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask |
	             ButtonMotionMask | KeymapStateMask | ExposureMask | VisibilityChangeMask |
	             StructureNotifyMask | SubstructureNotifyMask | SubstructureRedirectMask |
	             FocusChangeMask | PropertyChangeMask | ColormapChangeMask | OwnerGrabButtonMask;
	XSelectInput(xfc->display, appWindow->handle, input_mask);

	if (xfc->_XWAYLAND_MAY_GRAB_KEYBOARD)
		xf_SendClientEvent(xfc, appWindow->handle, xfc->_XWAYLAND_MAY_GRAB_KEYBOARD, 1, 1);

	return TRUE;
}

void xf_SetWindowMinMaxInfo(xfContext* xfc, xfAppWindow* appWindow, int maxWidth, int maxHeight,
                            int maxPosX, int maxPosY, int minTrackWidth, int minTrackHeight,
                            int maxTrackWidth, int maxTrackHeight)
{
	XSizeHints* size_hints;
	size_hints = XAllocSizeHints();

	if (size_hints)
	{
		size_hints->flags = PMinSize | PMaxSize | PResizeInc;
		size_hints->min_width = minTrackWidth;
		size_hints->min_height = minTrackHeight;
		size_hints->max_width = maxTrackWidth;
		size_hints->max_height = maxTrackHeight;
		/* to speedup window drawing we need to select optimal value for sizing step. */
		size_hints->width_inc = size_hints->height_inc = 1;
		XSetWMNormalHints(xfc->display, appWindow->handle, size_hints);
		XFree(size_hints);
	}
}

void xf_StartLocalMoveSize(xfContext* xfc, xfAppWindow* appWindow, int direction, int x, int y)
{
	if (appWindow->local_move.state != LMS_NOT_ACTIVE)
		return;

	/*
	 * Save original mouse location relative to root.  This will be needed
	 * to end local move to RDP server and/or X server
	 */
	appWindow->local_move.root_x = x;
	appWindow->local_move.root_y = y;
	appWindow->local_move.state = LMS_STARTING;
	appWindow->local_move.direction = direction;

	xf_ungrab(xfc);

	xf_SendClientEvent(
	    xfc, appWindow->handle,
	    xfc->_NET_WM_MOVERESIZE, /* request X window manager to initiate a local move */
	    5,                       /* 5 arguments to follow */
	    x,                       /* x relative to root window */
	    y,                       /* y relative to root window */
	    direction,               /* extended ICCM direction flag */
	    1,                       /* simulated mouse button 1 */
	    1);                      /* 1 == application request per extended ICCM */
}

void xf_EndLocalMoveSize(xfContext* xfc, xfAppWindow* appWindow)
{
	if (appWindow->local_move.state == LMS_NOT_ACTIVE)
		return;

	if (appWindow->local_move.state == LMS_STARTING)
	{
		/*
		 * The move never was property started. This can happen due to race
		 * conditions between the mouse button up and the communications to the
		 * RDP server for local moves. We must cancel the X window manager move.
		 * Per ICCM, the X client can ask to cancel an active move.
		 */
		xf_SendClientEvent(
		    xfc, appWindow->handle,
		    xfc->_NET_WM_MOVERESIZE,      /* request X window manager to abort a local move */
		    5,                            /* 5 arguments to follow */
		    appWindow->local_move.root_x, /* x relative to root window */
		    appWindow->local_move.root_y, /* y relative to root window */
		    _NET_WM_MOVERESIZE_CANCEL,    /* extended ICCM direction flag */
		    1,                            /* simulated mouse button 1 */
		    1);                           /* 1 == application request per extended ICCM */
	}

	appWindow->local_move.state = LMS_NOT_ACTIVE;
}

void xf_MoveWindow(xfContext* xfc, xfAppWindow* appWindow, int x, int y, int width, int height)
{
	BOOL resize = FALSE;

	if ((width * height) < 1)
		return;

	if ((appWindow->width != width) || (appWindow->height != height))
		resize = TRUE;

	if (appWindow->local_move.state == LMS_STARTING || appWindow->local_move.state == LMS_ACTIVE)
		return;

	appWindow->x = x;
	appWindow->y = y;
	appWindow->width = width;
	appWindow->height = height;

	if (resize)
		XMoveResizeWindow(xfc->display, appWindow->handle, x, y, width, height);
	else
		XMoveWindow(xfc->display, appWindow->handle, x, y);

	xf_UpdateWindowArea(xfc, appWindow, 0, 0, width, height);
}

void xf_ShowWindow(xfContext* xfc, xfAppWindow* appWindow, BYTE state)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(appWindow);

	switch (state)
	{
		case WINDOW_HIDE:
			XWithdrawWindow(xfc->display, appWindow->handle, xfc->screen_number);
			break;

		case WINDOW_SHOW_MINIMIZED:
			appWindow->minimized = TRUE;
			XIconifyWindow(xfc->display, appWindow->handle, xfc->screen_number);
			break;

		case WINDOW_SHOW_MAXIMIZED:
			/* Set the window as maximized */
			appWindow->maxHorz = TRUE;
			appWindow->maxVert = TRUE;
			xf_SendClientEvent(xfc, appWindow->handle, xfc->_NET_WM_STATE, 4, _NET_WM_STATE_ADD,
			                   xfc->_NET_WM_STATE_MAXIMIZED_VERT, xfc->_NET_WM_STATE_MAXIMIZED_HORZ,
			                   0);

			/*
			 * This is a workaround for the case where the window is maximized locally before the
			 * rail server is told to maximize the window, this appears to be a race condition where
			 * the local window with incomplete data and once the window is actually maximized on
			 * the server
			 * - an update of the new areas may not happen. So, we simply to do a full update of the
			 * entire window once the rail server notifies us that the window is now maximized.
			 */
			if (appWindow->rail_state == WINDOW_SHOW_MAXIMIZED)
			{
				xf_UpdateWindowArea(xfc, appWindow, 0, 0, appWindow->windowWidth,
				                    appWindow->windowHeight);
			}

			break;

		case WINDOW_SHOW:
			/* Ensure the window is not maximized */
			xf_SendClientEvent(xfc, appWindow->handle, xfc->_NET_WM_STATE, 4, _NET_WM_STATE_REMOVE,
			                   xfc->_NET_WM_STATE_MAXIMIZED_VERT, xfc->_NET_WM_STATE_MAXIMIZED_HORZ,
			                   0);

			/*
			 * Ignore configure requests until both the Maximized properties have been processed
			 * to prevent condition where WM overrides size of request due to one or both of these
			 * properties still being set - which causes a position adjustment to be sent back to
			 * the server thus causing the window to not return to its original size
			 */
			if (appWindow->rail_state == WINDOW_SHOW_MAXIMIZED)
				appWindow->rail_ignore_configure = TRUE;

			if (appWindow->is_transient)
				xf_SetWindowUnlisted(xfc, appWindow->handle);

			XMapWindow(xfc->display, appWindow->handle);
			break;
	}

	/* Save the current rail state of this window */
	appWindow->rail_state = state;
	XFlush(xfc->display);
}

void xf_SetWindowRects(xfContext* xfc, xfAppWindow* appWindow, RECTANGLE_16* rects, int nrects)
{
	int i;
	XRectangle* xrects;

	if (nrects < 1)
		return;

#ifdef WITH_XEXT
	xrects = (XRectangle*)calloc(nrects, sizeof(XRectangle));

	for (i = 0; i < nrects; i++)
	{
		xrects[i].x = rects[i].left;
		xrects[i].y = rects[i].top;
		xrects[i].width = rects[i].right - rects[i].left;
		xrects[i].height = rects[i].bottom - rects[i].top;
	}

	XShapeCombineRectangles(xfc->display, appWindow->handle, ShapeBounding, 0, 0, xrects, nrects,
	                        ShapeSet, 0);
	free(xrects);
#endif
}

void xf_SetWindowVisibilityRects(xfContext* xfc, xfAppWindow* appWindow, UINT32 rectsOffsetX,
                                 UINT32 rectsOffsetY, RECTANGLE_16* rects, int nrects)
{
	int i;
	XRectangle* xrects;

	if (nrects < 1)
		return;

#ifdef WITH_XEXT
	xrects = (XRectangle*)calloc(nrects, sizeof(XRectangle));

	for (i = 0; i < nrects; i++)
	{
		xrects[i].x = rects[i].left;
		xrects[i].y = rects[i].top;
		xrects[i].width = rects[i].right - rects[i].left;
		xrects[i].height = rects[i].bottom - rects[i].top;
	}

	XShapeCombineRectangles(xfc->display, appWindow->handle, ShapeBounding, rectsOffsetX,
	                        rectsOffsetY, xrects, nrects, ShapeSet, 0);
	free(xrects);
#endif
}

void xf_UpdateWindowArea(xfContext* xfc, xfAppWindow* appWindow, int x, int y, int width,
                         int height)
{
	int ax, ay;
	const rdpSettings* settings;

	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (appWindow == NULL)
		return;

	if (appWindow->surfaceId < UINT16_MAX)
		return;

	ax = x + appWindow->windowOffsetX;
	ay = y + appWindow->windowOffsetY;

	if (ax + width > appWindow->windowOffsetX + appWindow->width)
		width = (appWindow->windowOffsetX + appWindow->width - 1) - ax;

	if (ay + height > appWindow->windowOffsetY + appWindow->height)
		height = (appWindow->windowOffsetY + appWindow->height - 1) - ay;

	xf_lock_x11(xfc);

	if (settings->SoftwareGdi)
	{
		XPutImage(xfc->display, appWindow->pixmap, appWindow->gc, xfc->image, ax, ay, x, y, width,
		          height);
	}

	XCopyArea(xfc->display, appWindow->pixmap, appWindow->handle, appWindow->gc, x, y, width,
	          height, x, y);
	XFlush(xfc->display);
	xf_unlock_x11(xfc);
}

static void xf_AppWindowDestroyImage(xfAppWindow* appWindow)
{
	WINPR_ASSERT(appWindow);
	if (appWindow->image)
	{
		appWindow->image->data = NULL;
		XDestroyImage(appWindow->image);
		appWindow->image = NULL;
	}
}

void xf_DestroyWindow(xfContext* xfc, xfAppWindow* appWindow)
{
	if (!appWindow)
		return;

	if (xfc->appWindow == appWindow)
		xfc->appWindow = NULL;

	if (appWindow->gc)
		XFreeGC(xfc->display, appWindow->gc);

	if (appWindow->pixmap)
		XFreePixmap(xfc->display, appWindow->pixmap);

	xf_AppWindowDestroyImage(appWindow);

	if (appWindow->handle)
	{
		XUnmapWindow(xfc->display, appWindow->handle);
		XDestroyWindow(xfc->display, appWindow->handle);
	}

	if (appWindow->xfwin)
		munmap(0, sizeof(*appWindow->xfwin));

	if (appWindow->shmid >= 0)
		close(appWindow->shmid);

	shm_unlink(get_shm_id());
	appWindow->xfwin = (Window*)-1;
	appWindow->shmid = -1;
	free(appWindow->title);
	free(appWindow->windowRects);
	free(appWindow->visibilityRects);
	free(appWindow);
}

xfAppWindow* xf_AppWindowFromX11Window(xfContext* xfc, Window wnd)
{
	size_t index;
	size_t count;
	ULONG_PTR* pKeys = NULL;

	WINPR_ASSERT(xfc);
	if (!xfc->railWindows)
		return NULL;

	count = HashTable_GetKeys(xfc->railWindows, &pKeys);

	for (index = 0; index < count; index++)
	{
		xfAppWindow* appWindow = xf_rail_get_window(xfc, *(UINT64*)pKeys[index]);

		if (!appWindow)
		{
			free(pKeys);
			return NULL;
		}

		if (appWindow->handle == wnd)
		{
			free(pKeys);
			return appWindow;
		}
	}

	free(pKeys);
	return NULL;
}

UINT xf_AppUpdateWindowFromSurface(xfContext* xfc, gdiGfxSurface* surface)
{
	XImage* image = NULL;
	UINT rc = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);

	xfAppWindow* appWindow = xf_rail_get_window(xfc, surface->windowId);
	if (!appWindow)
	{
		WLog_VRB(TAG, "[%s] Failed to find a window for id=0x%08" PRIx64, __func__,
		         surface->windowId);
		return CHANNEL_RC_OK;
	}

	const BOOL swGdi = freerdp_settings_get_bool(xfc->common.context.settings, FreeRDP_SoftwareGdi);
	UINT32 nrects = 0;
	const RECTANGLE_16* rects = region16_rects(&surface->invalidRegion, &nrects);

	xf_lock_x11(xfc);
	if (swGdi)
	{
		if (appWindow->surfaceId != surface->surfaceId)
		{
			xf_AppWindowDestroyImage(appWindow);
			appWindow->surfaceId = surface->surfaceId;
		}
		if (appWindow->width != (INT64)surface->width)
			xf_AppWindowDestroyImage(appWindow);
		if (appWindow->height != (INT64)surface->height)
			xf_AppWindowDestroyImage(appWindow);

		if (!appWindow->image)
		{
			WINPR_ASSERT(xfc->depth != 0);
			appWindow->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
			                                (char*)surface->data, surface->width, surface->height,
			                                xfc->scanline_pad, surface->scanline);
			if (!appWindow->image)
			{
				WLog_WARN(TAG,
				          "[%s] Failed create a XImage[%" PRIu32 "x%" PRIu32 ", scanline=%" PRIu32
				          ", bpp=%" PRIu32 "] for window id=0x%08" PRIx64,
				          __func__, surface->width, surface->height, surface->scanline, xfc->depth,
				          surface->windowId);
				goto fail;
			}
			appWindow->image->byte_order = LSBFirst;
			appWindow->image->bitmap_bit_order = LSBFirst;
		}

		image = appWindow->image;
	}
	else
	{
		xfGfxSurface* xfSurface = (xfGfxSurface*)surface;
		image = xfSurface->image;
	}

	for (UINT32 x = 0; x < nrects; x++)
	{
		const RECTANGLE_16* rect = &rects[x];
		const UINT32 width = rect->right - rect->left;
		const UINT32 height = rect->bottom - rect->top;

		XPutImage(xfc->display, appWindow->pixmap, appWindow->gc, image, rect->left, rect->top,
		          rect->left, rect->top, width, height);

		XCopyArea(xfc->display, appWindow->pixmap, appWindow->handle, appWindow->gc, rect->left,
		          rect->top, width, height, rect->left, rect->top);
	}

	rc = CHANNEL_RC_OK;
fail:
	XFlush(xfc->display);
	xf_unlock_x11(xfc);
	return rc;
}

BOOL xf_AppWindowResize(xfContext* xfc, xfAppWindow* appWindow)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(appWindow);

	if (appWindow->pixmap != 0)
		XFreePixmap(xfc->display, appWindow->pixmap);

	WINPR_ASSERT(xfc->depth != 0);
	appWindow->pixmap =
	    XCreatePixmap(xfc->display, xfc->drawable, appWindow->width, appWindow->height, xfc->depth);
	xf_AppWindowDestroyImage(appWindow);

	return appWindow->pixmap != 0;
}
