/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Windows
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012 HP Development Company, LLC
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

#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <winpr/thread.h>
#include <winpr/crt.h>

#include <freerdp/rail.h>
#include <freerdp/utils/rail.h>

#ifdef WITH_XEXT
#include <X11/extensions/shape.h>
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#include "xf_input.h"
#endif

#include "xf_input.h"

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

#include "FreeRDP_Icon_256px.h"
#define xf_icon_prop FreeRDP_Icon_256px_prop

#include "xf_window.h"

/* Extended Window Manager Hints: http://standards.freedesktop.org/wm-spec/wm-spec-1.3.html */

/* bit definitions for MwmHints.flags */
#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

/* bit definitions for MwmHints.functions */
#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

/* bit definitions for MwmHints.decorations */
#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

#define PROP_MOTIF_WM_HINTS_ELEMENTS	5

struct _PropMotifWmHints
{
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long inputMode;
	unsigned long status;
};
typedef struct _PropMotifWmHints PropMotifWmHints;

/**
 * Post an event from the client to the X server
 */
void xf_SendClientEvent(xfContext *xfc, xfWindow *window, Atom atom, unsigned int numArgs, ...)
{
	XEvent xevent;
	unsigned int i;
	va_list argp;
	va_start(argp, numArgs);
	xevent.xclient.type = ClientMessage;
	xevent.xclient.serial = 0;
	xevent.xclient.send_event = False;
	xevent.xclient.display = xfc->display;
	xevent.xclient.window = window->handle;
	xevent.xclient.message_type = atom;
	xevent.xclient.format = 32;
	for(i=0; i<numArgs; i++)
	{
		xevent.xclient.data.l[i] = va_arg(argp, int);
	}
	DEBUG_X11("Send ClientMessage Event: wnd=0x%04X", (unsigned int) xevent.xclient.window);
	XSendEvent(xfc->display, RootWindowOfScreen(xfc->screen), False,
			   SubstructureRedirectMask | SubstructureNotifyMask, &xevent);
	XSync(xfc->display, False);
	va_end(argp);
}

void xf_SetWindowFullscreen(xfContext *xfc, xfWindow *window, BOOL fullscreen)
{
	if(fullscreen)
	{
		rdpSettings *settings = xfc->instance->settings;
		xf_SetWindowDecorations(xfc, window, FALSE);
		XMoveResizeWindow(xfc->display, window->handle, settings->DesktopPosX, settings->DesktopPosY, window->width, window->height);
		XMapRaised(xfc->display, window->handle);
		window->fullscreen = TRUE;
	}
}

/* http://tronche.com/gui/x/xlib/window-information/XGetWindowProperty.html */

BOOL xf_GetWindowProperty(xfContext *xfc, Window window, Atom property, int length,
						  unsigned long *nitems, unsigned long *bytes, BYTE **prop)
{
	int status;
	Atom actual_type;
	int actual_format;
	if(property == None)
		return FALSE;
	status = XGetWindowProperty(xfc->display, window,
								property, 0, length, FALSE, AnyPropertyType,
								&actual_type, &actual_format, nitems, bytes, prop);
	if(status != Success)
		return FALSE;
	if(actual_type == None)
	{
		DEBUG_WARN("Property %lu does not exist", property);
		return FALSE;
	}
	return TRUE;
}

BOOL xf_GetCurrentDesktop(xfContext *xfc)
{
	BOOL status;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char *prop;
	status = xf_GetWindowProperty(xfc, DefaultRootWindow(xfc->display),
								  xfc->_NET_CURRENT_DESKTOP, 1, &nitems, &bytes, &prop);
	if(!status)
		return FALSE;
	xfc->current_desktop = (int) *prop;
	free(prop);
	return TRUE;
}

BOOL xf_GetWorkArea(xfContext *xfc)
{
	long *plong;
	BOOL status;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char *prop;
	status = xf_GetCurrentDesktop(xfc);
	if(status != TRUE)
		return FALSE;
	status = xf_GetWindowProperty(xfc, DefaultRootWindow(xfc->display),
								  xfc->_NET_WORKAREA, 32 * 4, &nitems, &bytes, &prop);
	if(status != TRUE)
		return FALSE;
	if((xfc->current_desktop * 4 + 3) >= nitems)
	{
		free(prop);
		return FALSE;
	}
	plong = (long *) prop;
	xfc->workArea.x = plong[xfc->current_desktop * 4 + 0];
	xfc->workArea.y = plong[xfc->current_desktop * 4 + 1];
	xfc->workArea.width = plong[xfc->current_desktop * 4 + 2];
	xfc->workArea.height = plong[xfc->current_desktop * 4 + 3];
	free(prop);
	return TRUE;
}

void xf_SetWindowDecorations(xfContext *xfc, xfWindow *window, BOOL show)
{
	PropMotifWmHints hints;
	hints.decorations = (show) ? MWM_DECOR_ALL : 0;
	hints.functions = MWM_FUNC_ALL ;
	hints.flags = MWM_HINTS_DECORATIONS | MWM_HINTS_FUNCTIONS;
	hints.inputMode = 0;
	hints.status = 0;
	XChangeProperty(xfc->display, window->handle, xfc->_MOTIF_WM_HINTS, xfc->_MOTIF_WM_HINTS, 32,
					PropModeReplace, (BYTE *) &hints, PROP_MOTIF_WM_HINTS_ELEMENTS);
}

void xf_SetWindowUnlisted(xfContext *xfc, xfWindow *window)
{
	Atom window_state[2];
	window_state[0] = xfc->_NET_WM_STATE_SKIP_PAGER;
	window_state[1] = xfc->_NET_WM_STATE_SKIP_TASKBAR;
	XChangeProperty(xfc->display, window->handle, xfc->_NET_WM_STATE,
					XA_ATOM, 32, PropModeReplace, (BYTE *) &window_state, 2);
}

void xf_SetWindowStyle(xfContext *xfc, xfWindow *window, UINT32 style, UINT32 ex_style)
{
	Atom window_type;
	if(/*(ex_style & WS_EX_TOPMOST) ||*/ (ex_style & WS_EX_TOOLWINDOW))
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
		attrs.override_redirect = True;
		XChangeWindowAttributes(xfc->display, window->handle, CWOverrideRedirect, &attrs);
		window->is_transient = TRUE;
		xf_SetWindowUnlisted(xfc, window);
		window_type = xfc->_NET_WM_WINDOW_TYPE_POPUP;
	}
	/*
	 * TOPMOST window that is not a toolwindow is treated like a regular window(ie. task manager).
	 * Want to do this here, since the window may have type WS_POPUP
	 */
	else
		if(ex_style & WS_EX_TOPMOST)
		{
			window_type = xfc->_NET_WM_WINDOW_TYPE_NORMAL;
		}
		else
			if(style & WS_POPUP)
			{
				/* this includes dialogs, popups, etc, that need to be full-fledged windows */
				window->is_transient = TRUE;
				window_type = xfc->_NET_WM_WINDOW_TYPE_DIALOG;
				xf_SetWindowUnlisted(xfc, window);
			}
			else
			{
				window_type = xfc->_NET_WM_WINDOW_TYPE_NORMAL;
			}
	XChangeProperty(xfc->display, window->handle, xfc->_NET_WM_WINDOW_TYPE,
					XA_ATOM, 32, PropModeReplace, (BYTE *) &window_type, 1);
}

void xf_SetWindowText(xfContext *xfc, xfWindow *window, char *name)
{
	XStoreName(xfc->display, window->handle, name);
}

static void xf_SetWindowPID(xfContext *xfc, xfWindow *window, pid_t pid)
{
	Atom am_wm_pid;
	if(!pid)
		pid = getpid();
	am_wm_pid = XInternAtom(xfc->display, "_NET_WM_PID", False);
	XChangeProperty(xfc->display, window->handle, am_wm_pid, XA_CARDINAL,
					32, PropModeReplace, (unsigned char *)&pid, 1);
}

static const char *get_shm_id()
{
	static char shm_id[64];
	snprintf(shm_id, sizeof(shm_id), "com.freerdp.xfreerpd.tsmf_%016X", GetCurrentProcessId());
	return shm_id;
}

xfWindow* xf_CreateDesktopWindow(xfContext *xfc, char *name, int width, int height, BOOL decorations)
{
	xfWindow* window;
	XEvent xevent;
	rdpSettings* settings;
	window = (xfWindow*) calloc(1, sizeof(xfWindow));

	settings = xfc->instance->settings;

	if (window)
	{
		int input_mask;
		XClassHint *class_hints;
		window->width = width;
		window->height = height;
		window->fullscreen = FALSE;
		window->decorations = decorations;
		window->local_move.state = LMS_NOT_ACTIVE;
		window->is_mapped = FALSE;
		window->is_transient = FALSE;
		window->handle = XCreateWindow(xfc->display, RootWindowOfScreen(xfc->screen),
									   xfc->workArea.x, xfc->workArea.y, xfc->workArea.width, xfc->workArea.height, 0, xfc->depth, InputOutput, xfc->visual,
									   CWBackPixel | CWBackingStore | CWOverrideRedirect | CWColormap |
									   CWBorderPixel | CWWinGravity | CWBitGravity, &xfc->attribs);

		window->shmid = shm_open(get_shm_id(), O_CREAT | O_EXCL | O_RDWR, S_IREAD | S_IWRITE);

		if (window->shmid < 0)
		{
			DEBUG_X11("xf_CreateDesktopWindow: failed to get access to shared memory - shmget()\n");
		}
		else
		{
			void* mem;

			ftruncate(window->shmid, sizeof(window->handle));

			mem = mmap(0, sizeof(window->handle), PROT_READ | PROT_WRITE, MAP_SHARED, window->shmid, 0);

			if (mem == ((int*) -1))
			{
				DEBUG_X11("xf_CreateDesktopWindow: failed to assign pointer to the memory address - shmat()\n");
			}
			else
			{
				window->xfwin = mem;
				*window->xfwin = window->handle;
			}
		}

		class_hints = XAllocClassHint();

		if (class_hints)
		{
			class_hints->res_name = "xfreerdp";

			if (xfc->instance->settings->WmClass)
				class_hints->res_class = xfc->instance->settings->WmClass;
			else
				class_hints->res_class = "xfreerdp";

			XSetClassHint(xfc->display, window->handle, class_hints);
			XFree(class_hints);
		}

		xf_ResizeDesktopWindow(xfc, window, width, height);
		xf_SetWindowDecorations(xfc, window, decorations);
		xf_SetWindowPID(xfc, window, 0);

		input_mask =
			KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
			VisibilityChangeMask | FocusChangeMask | StructureNotifyMask |
			PointerMotionMask | ExposureMask | PropertyChangeMask;

		if (xfc->grab_keyboard)
			input_mask |= EnterWindowMask | LeaveWindowMask;

		XChangeProperty(xfc->display, window->handle, xfc->_NET_WM_ICON, XA_CARDINAL, 32,
						PropModeReplace, (BYTE *) xf_icon_prop, ARRAYSIZE(xf_icon_prop));

		if (xfc->settings->ParentWindowId)
			XReparentWindow(xfc->display, window->handle, (Window) xfc->settings->ParentWindowId, 0, 0);

		XSelectInput(xfc->display, window->handle, input_mask);
		XClearWindow(xfc->display, window->handle);
		XMapWindow(xfc->display, window->handle);
		xf_input_init(xfc, window->handle);
		/*
		 * NOTE: This must be done here to handle reparenting the window,
		 * so that we don't miss the event and hang waiting for the next one
		 */
		do
		{
			XMaskEvent(xfc->display, VisibilityChangeMask, &xevent);
		}
		while(xevent.type != VisibilityNotify);
		/*
		 * The XCreateWindow call will start the window in the upper-left corner of our current
		 * monitor instead of the upper-left monitor for remote app mode(which uses all monitors).
		 * This extra call after the window is mapped will position the login window correctly
		 */
		if (xfc->instance->settings->RemoteApplicationMode)
		{
			XMoveWindow(xfc->display, window->handle, 0, 0);
		}
		else if(settings->DesktopPosX || settings->DesktopPosY)
		{
			XMoveWindow(xfc->display, window->handle, settings->DesktopPosX, settings->DesktopPosY);
		}
	}
	xf_SetWindowText(xfc, window, name);
	return window;
}

void xf_ResizeDesktopWindow(xfContext *xfc, xfWindow *window, int width, int height)
{
	XSizeHints *size_hints;
	size_hints = XAllocSizeHints();
	if(size_hints)
	{
		size_hints->flags = PMinSize | PMaxSize;
		size_hints->min_width = size_hints->max_width = xfc->width;
		size_hints->min_height = size_hints->max_height = xfc->height;
		XSetWMNormalHints(xfc->display, window->handle, size_hints);
		XResizeWindow(xfc->display, window->handle, xfc->width, xfc->height);
		XFree(size_hints);
	}
}

void xf_FixWindowCoordinates(xfContext *xfc, int *x, int *y, int *width, int *height)
{
	int vscreen_width;
	int vscreen_height;
	vscreen_width = xfc->vscreen.area.right - xfc->vscreen.area.left + 1;
	vscreen_height = xfc->vscreen.area.bottom - xfc->vscreen.area.top + 1;
	if(*width < 1)
	{
		*width = 1;
	}
	if(*height < 1)
	{
		*height = 1;
	}
	if(*x < xfc->vscreen.area.left)
	{
		*width += *x;
		*x = xfc->vscreen.area.left;
	}
	if(*y < xfc->vscreen.area.top)
	{
		*height += *y;
		*y = xfc->vscreen.area.top;
	}
	if(*width > vscreen_width)
	{
		*width = vscreen_width;
	}
	if(*height > vscreen_height)
	{
		*height = vscreen_height;
	}
}

char rail_window_class[] = "RAIL:00000000";

xfWindow *xf_CreateWindow(xfContext *xfc, rdpWindow *wnd, int x, int y, int width, int height, UINT32 id)
{
	XGCValues gcv;
	int input_mask;
	xfWindow *window;
	XWMHints *InputModeHint;
	XClassHint *class_hints;
	window = (xfWindow *) malloc(sizeof(xfWindow));
	ZeroMemory(window, sizeof(xfWindow));
	xf_FixWindowCoordinates(xfc, &x, &y, &width, &height);
	window->left = x;
	window->top = y;
	window->right = x + width - 1;
	window->bottom = y + height - 1;
	window->width = width;
	window->height = height;
	/*
	 * WS_EX_DECORATIONS is used by XRDP and instructs
	 * the client to use local window decorations
	 */
	window->decorations = (wnd->extendedStyle & WS_EX_DECORATIONS) ? TRUE : FALSE;
	window->fullscreen = FALSE;
	window->window = wnd;
	window->local_move.state = LMS_NOT_ACTIVE;
	window->is_mapped = FALSE;
	window->is_transient = FALSE;
	window->rail_state = 0;
	window->rail_ignore_configure = FALSE;
	window->handle = XCreateWindow(xfc->display, RootWindowOfScreen(xfc->screen),
								   x, y, window->width, window->height, 0, xfc->depth, InputOutput, xfc->visual,
								   0, &xfc->attribs);
	DEBUG_X11_LMS("Create window=0x%X rc={l=%d t=%d r=%d b=%d} w=%d h=%d  rdp=0x%X",
				  (UINT32) window->handle, window->left, window->top, window->right, window->bottom,
				  window->width, window->height, wnd->windowId);
	ZeroMemory(&gcv, sizeof(gcv));
	window->gc = XCreateGC(xfc->display, window->handle, GCGraphicsExposures, &gcv);
	class_hints = XAllocClassHint();
	if(class_hints)
	{
		char *class = NULL;
		if(xfc->instance->settings->WmClass != NULL)
		{
			class_hints->res_class = xfc->instance->settings->WmClass;
		}
		else
		{
			class = malloc(sizeof(rail_window_class));
			snprintf(class, sizeof(rail_window_class), "RAIL:%08X", id);
			class_hints->res_class = class;
		}
		class_hints->res_name = "RAIL";
		XSetClassHint(xfc->display, window->handle, class_hints);
		XFree(class_hints);
		if(class)
			free(class);
	}
	/* Set the input mode hint for the WM */
	InputModeHint = XAllocWMHints();
	InputModeHint->flags = (1L << 0);
	InputModeHint->input = True;
	XSetWMHints(xfc->display, window->handle, InputModeHint);
	XFree(InputModeHint);
	XSetWMProtocols(xfc->display, window->handle, &(xfc->WM_DELETE_WINDOW), 1);
	input_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
				 ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
				 PointerMotionMask | Button1MotionMask | Button2MotionMask |
				 Button3MotionMask | Button4MotionMask | Button5MotionMask |
				 ButtonMotionMask | KeymapStateMask | ExposureMask |
				 VisibilityChangeMask | StructureNotifyMask | SubstructureNotifyMask |
				 SubstructureRedirectMask | FocusChangeMask | PropertyChangeMask |
				 ColormapChangeMask | OwnerGrabButtonMask;
	XSelectInput(xfc->display, window->handle, input_mask);
	xf_SetWindowDecorations(xfc, window, window->decorations);
	xf_SetWindowStyle(xfc, window, wnd->style, wnd->extendedStyle);
	xf_SetWindowPID(xfc, window, 0);
	xf_ShowWindow(xfc, window, WINDOW_SHOW);
	XClearWindow(xfc->display, window->handle);
	XMapWindow(xfc->display, window->handle);
	/* Move doesn't seem to work until window is mapped. */
	xf_MoveWindow(xfc, window, x, y, width, height);
	return window;
}

void xf_SetWindowMinMaxInfo(xfContext *xfc, xfWindow *window,
							int maxWidth, int maxHeight, int maxPosX, int maxPosY,
							int minTrackWidth, int minTrackHeight, int maxTrackWidth, int maxTrackHeight)
{
	XSizeHints *size_hints;
	size_hints = XAllocSizeHints();
	if(size_hints)
	{
		size_hints->flags = PMinSize | PMaxSize | PResizeInc;
		size_hints->min_width  = minTrackWidth;
		size_hints->min_height = minTrackHeight;
		size_hints->max_width  = maxTrackWidth;
		size_hints->max_height = maxTrackHeight;
		/* to speedup window drawing we need to select optimal value for sizing step. */
		size_hints->width_inc = size_hints->height_inc = 1;
		XSetWMNormalHints(xfc->display, window->handle, size_hints);
		XFree(size_hints);
	}
}

void xf_StartLocalMoveSize(xfContext *xfc, xfWindow *window, int direction, int x, int y)
{
	if(window->local_move.state != LMS_NOT_ACTIVE)
		return;
	DEBUG_X11_LMS("direction=%d window=0x%X rc={l=%d t=%d r=%d b=%d} w=%d h=%d   "
				  "RDP=0x%X rc={l=%d t=%d} w=%d h=%d  mouse_x=%d mouse_y=%d",
				  direction, (UINT32) window->handle,
				  window->left, window->top, window->right, window->bottom,
				  window->width, window->height, window->window->windowId,
				  window->window->windowOffsetX, window->window->windowOffsetY,
				  window->window->windowWidth, window->window->windowHeight, x, y);
	/*
	* Save original mouse location relative to root.  This will be needed
	* to end local move to RDP server and/or X server
	*/
	window->local_move.root_x = x;
	window->local_move.root_y = y;
	window->local_move.state = LMS_STARTING;
	window->local_move.direction = direction;
	XUngrabPointer(xfc->display, CurrentTime);
	xf_SendClientEvent(xfc, window,
					   xfc->_NET_WM_MOVERESIZE, /* request X window manager to initiate a local move */
					   5, /* 5 arguments to follow */
					   x, /* x relative to root window */
					   y, /* y relative to root window */
					   direction, /* extended ICCM direction flag */
					   1, /* simulated mouse button 1 */
					   1); /* 1 == application request per extended ICCM */
}

void xf_EndLocalMoveSize(xfContext *xfc, xfWindow *window)
{
	DEBUG_X11_LMS("state=%d window=0x%X rc={l=%d t=%d r=%d b=%d} w=%d h=%d  "
				  "RDP=0x%X rc={l=%d t=%d} w=%d h=%d",
				  window->local_move.state,
				  (UINT32) window->handle, window->left, window->top, window->right, window->bottom,
				  window->width, window->height, window->window->windowId,
				  window->window->windowOffsetX, window->window->windowOffsetY,
				  window->window->windowWidth, window->window->windowHeight);
	if(window->local_move.state == LMS_NOT_ACTIVE)
		return;
	if(window->local_move.state == LMS_STARTING)
	{
		/*
		 * The move never was property started. This can happen due to race
		 * conditions between the mouse button up and the communications to the
		 * RDP server for local moves. We must cancel the X window manager move.
		 * Per ICCM, the X client can ask to cancel an active move.
		 */
		xf_SendClientEvent(xfc, window,
						   xfc->_NET_WM_MOVERESIZE, /* request X window manager to abort a local move */
						   5, /* 5 arguments to follow */
						   window->local_move.root_x, /* x relative to root window */
						   window->local_move.root_y, /* y relative to root window */
						   _NET_WM_MOVERESIZE_CANCEL, /* extended ICCM direction flag */
						   1, /* simulated mouse button 1 */
						   1); /* 1 == application request per extended ICCM */
	}
	window->local_move.state = LMS_NOT_ACTIVE;
}

void xf_MoveWindow(xfContext *xfc, xfWindow *window, int x, int y, int width, int height)
{
	BOOL resize = FALSE;
	if((width * height) < 1)
		return;
	if((window->width != width) || (window->height != height))
		resize = TRUE;
	if(window->local_move.state == LMS_STARTING ||
			window->local_move.state == LMS_ACTIVE)
		return;
	DEBUG_X11_LMS("window=0x%X rc={l=%d t=%d r=%d b=%d} w=%u h=%u  "
				  "new rc={l=%d t=%d r=%d b=%d} w=%u h=%u"
				  "  RDP=0x%X rc={l=%d t=%d} w=%d h=%d",
				  (UINT32) window->handle, window->left, window->top,
				  window->right, window->bottom, window->width, window->height,
				  x, y, x + width -1, y + height -1, width, height,
				  window->window->windowId,
				  window->window->windowOffsetX, window->window->windowOffsetY,
				  window->window->windowWidth, window->window->windowHeight);
	window->left = x;
	window->top = y;
	window->right = x + width - 1;
	window->bottom = y + height - 1;
	window->width = width;
	window->height = height;
	if(resize)
		XMoveResizeWindow(xfc->display, window->handle, x, y, width, height);
	else
		XMoveWindow(xfc->display, window->handle, x, y);
	xf_UpdateWindowArea(xfc, window, 0, 0, width, height);
}

void xf_ShowWindow(xfContext *xfc, xfWindow *window, BYTE state)
{
	switch(state)
	{
		case WINDOW_HIDE:
			XWithdrawWindow(xfc->display, window->handle, xfc->screen_number);
			break;
		case WINDOW_SHOW_MINIMIZED:
			XIconifyWindow(xfc->display, window->handle, xfc->screen_number);
			break;
		case WINDOW_SHOW_MAXIMIZED:
			/* Set the window as maximized */
			xf_SendClientEvent(xfc, window, xfc->_NET_WM_STATE, 4, 1,
							   XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_VERT", False),
							   XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False), 0);
			/*
			 * This is a workaround for the case where the window is maximized locally before the rail server is told to maximize
			 * the window, this appears to be a race condition where the local window with incomplete data and once the window is
			 * actually maximized on the server - an update of the new areas may not happen. So, we simply to do a full update of
			 * the entire window once the rail server notifies us that the window is now maximized.
			 */
			if(window->rail_state == WINDOW_SHOW_MAXIMIZED)
				xf_UpdateWindowArea(xfc, window, 0, 0, window->window->windowWidth, window->window->windowHeight);
			break;
		case WINDOW_SHOW:
			/* Ensure the window is not maximized */
			xf_SendClientEvent(xfc, window, xfc->_NET_WM_STATE, 4, 0,
							   XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_VERT", False),
							   XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False), 0);
			/*
			 * Ignore configure requests until both the Maximized properties have been processed
			 * to prevent condition where WM overrides size of request due to one or both of these properties
			 * still being set - which causes a position adjustment to be sent back to the server
			 * thus causing the window to not return to its original size
			 */
			if(window->rail_state == WINDOW_SHOW_MAXIMIZED)
				window->rail_ignore_configure = TRUE;
			if(window->is_transient)
				xf_SetWindowUnlisted(xfc, window);
			break;
	}
	/* Save the current rail state of this window */
	window->rail_state = state;
	XFlush(xfc->display);
}

void xf_SetWindowIcon(xfContext *xfc, xfWindow *window, rdpIcon *icon)
{
	int x, y;
	int pixels;
	int propsize;
	long *propdata;
	long *dstp;
	UINT32 *srcp;
	if(!icon->big)
		return;
	pixels = icon->entry->width * icon->entry->height;
	propsize = 2 + pixels;
	propdata = malloc(propsize * sizeof(long));
	propdata[0] = icon->entry->width;
	propdata[1] = icon->entry->height;
	dstp = &(propdata[2]);
	srcp = (UINT32 *) icon->extra;
	for(y = 0; y < icon->entry->height; y++)
	{
		for(x = 0; x < icon->entry->width; x++)
		{
			*dstp++ = *srcp++;
		}
	}
	XChangeProperty(xfc->display, window->handle, xfc->_NET_WM_ICON, XA_CARDINAL, 32,
					PropModeReplace, (BYTE *) propdata, propsize);
	XFlush(xfc->display);
	free(propdata);
}

void xf_SetWindowRects(xfContext *xfc, xfWindow *window, RECTANGLE_16 *rects, int nrects)
{
	int i;
	XRectangle *xrects;
	if(nrects == 0)
		return;
	xrects = malloc(sizeof(XRectangle) * nrects);
	for(i = 0; i < nrects; i++)
	{
		xrects[i].x = rects[i].left;
		xrects[i].y = rects[i].top;
		xrects[i].width = rects[i].right - rects[i].left;
		xrects[i].height = rects[i].bottom - rects[i].top;
	}
#ifdef WITH_XEXT
	/*
	 * This is currently unsupported with the new logic to handle window placement with VisibleOffset variables
	 *
	 * Marc: enabling it works, and is required for round corners.
	 */
	XShapeCombineRectangles(xfc->display, window->handle, ShapeBounding, 0, 0, xrects, nrects, ShapeSet, 0);
#endif
	free(xrects);
}

void xf_SetWindowVisibilityRects(xfContext *xfc, xfWindow *window, RECTANGLE_16 *rects, int nrects)
{
	int i;
	XRectangle *xrects;
	if(nrects == 0)
		return;
	xrects = malloc(sizeof(XRectangle) * nrects);
	for(i = 0; i < nrects; i++)
	{
		xrects[i].x = rects[i].left;
		xrects[i].y = rects[i].top;
		xrects[i].width = rects[i].right - rects[i].left;
		xrects[i].height = rects[i].bottom - rects[i].top;
	}
#ifdef WITH_XEXT
	/*
	 * This is currently unsupported with the new logic to handle window placement with VisibleOffset variables
	 *
	 * Marc: enabling it works, and is required for round corners.
	 */
	XShapeCombineRectangles(xfc->display, window->handle, ShapeBounding, 0, 0, xrects, nrects, ShapeSet, 0);
#endif
	free(xrects);
}

void xf_UpdateWindowArea(xfContext *xfc, xfWindow *window, int x, int y, int width, int height)
{
	int ax, ay;
	rdpWindow *wnd;
	wnd = window->window;
	/* RemoteApp mode uses visibleOffset instead of windowOffset */
	if(!xfc->remote_app)
	{
		ax = x + wnd->windowOffsetX;
		ay = y + wnd->windowOffsetY;
		if(ax + width > wnd->windowOffsetX + wnd->windowWidth)
			width = (wnd->windowOffsetX + wnd->windowWidth - 1) - ax;
		if(ay + height > wnd->windowOffsetY + wnd->windowHeight)
			height = (wnd->windowOffsetY + wnd->windowHeight - 1) - ay;
	}
	else
	{
		ax = x + wnd->visibleOffsetX;
		ay = y + wnd->visibleOffsetY;
		if(ax + width > wnd->visibleOffsetX + wnd->windowWidth)
			width = (wnd->visibleOffsetX + wnd->windowWidth - 1) - ax;
		if(ay + height > wnd->visibleOffsetY + wnd->windowHeight)
			height = (wnd->visibleOffsetY + wnd->windowHeight - 1) - ay;
	}
	WaitForSingleObject(xfc->mutex, INFINITE);
	if(xfc->settings->SoftwareGdi)
	{
		XPutImage(xfc->display, xfc->primary, window->gc, xfc->image,
				  ax, ay, ax, ay, width, height);
	}
	XCopyArea(xfc->display, xfc->primary, window->handle, window->gc,
			  ax, ay, width, height, x, y);
	XFlush(xfc->display);
	ReleaseMutex(xfc->mutex);
}

BOOL xf_IsWindowBorder(xfContext *xfc, xfWindow *xfw, int x, int y)
{
	rdpWindow *wnd;
	BOOL clientArea = FALSE;
	BOOL windowArea = FALSE;
	wnd = xfw->window;
	if(((x > wnd->clientOffsetX) && (x < wnd->clientOffsetX + wnd->clientAreaWidth)) &&
			((y > wnd->clientOffsetY) && (y < wnd->clientOffsetY + wnd->clientAreaHeight)))
		clientArea = TRUE;
	if(((x > wnd->windowOffsetX) && (x < wnd->windowOffsetX + wnd->windowWidth)) &&
			((y > wnd->windowOffsetY) && (y < wnd->windowOffsetY + wnd->windowHeight)))
		windowArea = TRUE;
	return (windowArea && !(clientArea));
}

void xf_DestroyWindow(xfContext *xfc, xfWindow *window)
{
	if (!window)
		return;

	if (xfc->window == window)
		xfc->window = NULL;

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

	window->xfwin = (Window*) -1;
	window->shmid = -1;

	free(window);
}

rdpWindow *xf_rdpWindowFromWindow(xfContext *xfc, Window wnd)
{
	rdpRail *rail;
	if(xfc)
	{
		if(wnd)
		{
			rail = ((rdpContext *) xfc)->rail;
			if(rail)
				return window_list_get_by_extra_id(rail->list, (void *)(long) wnd);
		}
	}
	return NULL;
}
