/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Windows
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

#include <X11/Xutil.h>

#include "xf_window.h"

/* Extended Window Manager Hints: http://standards.freedesktop.org/wm-spec/wm-spec-1.3.html */

#ifndef XA_CARDINAL
#define XA_CARDINAL			6
#endif

#define MWM_HINTS_DECORATIONS		(1L << 1)
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

void desktop_fullscreen(xfInfo* xfi, xfWindow* window, boolean fullscreen)
{
	if (fullscreen)
	{
		if (window->decorations)
			window_show_decorations(xfi, window, False);

		XSetInputFocus(xfi->display, window->handle, RevertToParent, CurrentTime);
		window->fullscreen = True;
	}
}

/* http://tronche.com/gui/x/xlib/window-information/XGetWindowProperty.html */

boolean window_GetProperty(xfInfo* xfi, Window window, char* name, int length,
		unsigned long* nitems, unsigned long* bytes, uint8** prop)
{
	int status;
	Atom property;
	Atom actual_type;
	int actual_format;

	property = XInternAtom(xfi->display, name, True);

	if (property == None)
		return False;

	status = XGetWindowProperty(xfi->display, window,
			property, 0, length, False, AnyPropertyType,
			&actual_type, &actual_format, nitems, bytes, prop);

	if (status != Success)
		return False;

	return True;
}

boolean window_GetCurrentDesktop(xfInfo* xfi)
{
	boolean status;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char* prop;

	status = window_GetProperty(xfi, DefaultRootWindow(xfi->display),
			"_NET_CURRENT_DESKTOP", 1, &nitems, &bytes, &prop);

	if (status != True)
		return False;

	xfi->current_desktop = (int) *prop;
	xfree(prop);

	return True;
}

boolean window_GetWorkArea(xfInfo* xfi)
{
	long* plong;
	boolean status;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char* prop;

	status = window_GetProperty(xfi, DefaultRootWindow(xfi->display),
			"_NET_WORKAREA", 32 * 4, &nitems, &bytes, &prop);

	if (status != True)
		return False;

	window_GetCurrentDesktop(xfi);

	plong = (long*) prop;

	xfi->workArea.x = plong[xfi->current_desktop * 4 + 0];
	xfi->workArea.y = plong[xfi->current_desktop * 4 + 1];
	xfi->workArea.width = plong[xfi->current_desktop * 4 + 2];
	xfi->workArea.height = plong[xfi->current_desktop * 4 + 3];
	xfree(prop);

	return True;
}

void window_show_decorations(xfInfo* xfi, xfWindow* window, boolean show)
{
	Atom atom;
	PropMotifWmHints hints;

	hints.decorations = 0;
	hints.flags = MWM_HINTS_DECORATIONS;

	atom = XInternAtom(xfi->display, "_MOTIF_WM_HINTS", True);

	if (!atom)
	{
		printf("window_show_decorations: failed to obtain atom _MOTIF_WM_HINTS\n");
		return;
	}
	else
	{
		if (show != True)
		{
			XChangeProperty(xfi->display, window->handle, atom, atom, 32,
				PropModeReplace, (uint8*) &hints, PROP_MOTIF_WM_HINTS_ELEMENTS);
		}
	}

	window->decorations = show;
}

xfWindow* desktop_create(xfInfo* xfi, char* name)
{
	xfWindow* window;

	window = (xfWindow*) xzalloc(sizeof(xfWindow));

	if (window != NULL)
	{
		int input_mask;
		XSizeHints* size_hints;
		XClassHint* class_hints;

		window->decorations = True;
		window->fullscreen = True;

		window->handle = XCreateWindow(xfi->display, RootWindowOfScreen(xfi->screen),
			xfi->workArea.x, xfi->workArea.y, xfi->width, xfi->height, 0, xfi->depth, InputOutput, xfi->visual,
			CWBackPixel | CWBackingStore | CWOverrideRedirect | CWColormap |
			CWBorderPixel, &xfi->attribs);

		class_hints = XAllocClassHint();

		if (class_hints != NULL)
		{
			if (name != NULL)
				class_hints->res_name = name;

			class_hints->res_class = "freerdp";
			XSetClassHint(xfi->display, window->handle, class_hints);
			XFree(class_hints);
		}

		size_hints = XAllocSizeHints();

		if (size_hints)
		{
			size_hints->flags = PMinSize | PMaxSize;
			size_hints->min_width = size_hints->max_width = xfi->width;
			size_hints->min_height = size_hints->max_height = xfi->height;
			XSetWMNormalHints(xfi->display, window->handle, size_hints);
			XFree(size_hints);
		}

		input_mask =
			KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
			VisibilityChangeMask | FocusChangeMask | StructureNotifyMask |
			PointerMotionMask | ExposureMask;

		XSelectInput(xfi->display, window->handle, input_mask);
		XMapWindow(xfi->display, window->handle);
	}

	return window;
}

char rail_window_class[] = "RAIL:00000000";

xfWindow* xf_CreateWindow(xfInfo* xfi, int x, int y, int width, int height, uint32 id)
{
	xfWindow* window;

	window = (xfWindow*) xzalloc(sizeof(xfWindow));

	window->width = width;
	window->height = height;

	if (window != NULL)
	{
		XGCValues gcv;
		int input_mask;
		XSizeHints* size_hints;
		XClassHint* class_hints;

		window->decorations = False;
		window->fullscreen = False;

		window->handle = XCreateWindow(xfi->display, RootWindowOfScreen(xfi->screen),
			x, y, window->width, window->height, 0, xfi->depth, InputOutput, xfi->visual,
			CWBackPixel | CWBackingStore | CWOverrideRedirect | CWColormap |
			CWBorderPixel, &xfi->attribs);

		window_show_decorations(xfi, window, window->decorations);

		class_hints = XAllocClassHint();

		if (class_hints != NULL)
		{
			char* class;
			class = xmalloc(sizeof(rail_window_class));
			snprintf(class, sizeof(rail_window_class), "RAIL:%08X", id);
			class_hints->res_name = "RAIL";
			class_hints->res_class = class;
			XSetClassHint(xfi->display, window->handle, class_hints);
			XFree(class_hints);
			xfree(class);
		}

		size_hints = XAllocSizeHints();

		if (size_hints)
		{
			size_hints->flags = PMinSize | PMaxSize;
			size_hints->min_width = size_hints->max_width = window->width;
			size_hints->min_height = size_hints->max_height = window->height;
			XSetWMNormalHints(xfi->display, window->handle, size_hints);
			XFree(size_hints);
		}

		input_mask =
			KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
			VisibilityChangeMask | FocusChangeMask | StructureNotifyMask |
			PointerMotionMask | ExposureMask;

		XSelectInput(xfi->display, window->handle, input_mask);
		XMapWindow(xfi->display, window->handle);

		memset(&gcv, 0, sizeof(gcv));
		window->gc = XCreateGC(xfi->display, window->handle, GCGraphicsExposures, &gcv);
		window->surface = XCreatePixmap(xfi->display, window->handle, window->width, window->height, xfi->depth);

		xf_MoveWindow(xfi, window, x, y, width, height);
	}

	return window;
}

void xf_MoveWindow(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height)
{
	Pixmap surface;
	XSizeHints* size_hints;

	if ((width * height) < 1)
		return;

	size_hints = XAllocSizeHints();

	if (size_hints)
	{
		size_hints->flags = PMinSize | PMaxSize;
		size_hints->min_width = size_hints->max_width = width;
		size_hints->min_height = size_hints->max_height = height;
		XSetWMNormalHints(xfi->display, window->handle, size_hints);
		XFree(size_hints);
	}

	if (window->width == width && window->height == height)
		XMoveWindow(xfi->display, window->handle, x, y);
	else if (window->x == x && window->y == y)
		XResizeWindow(xfi->display, window->handle, width, height);
	else
		XMoveResizeWindow(xfi->display, window->handle, x, y, width, height);

	surface = XCreatePixmap(xfi->display, window->handle, width, height, xfi->depth);
	XCopyArea(xfi->display, surface, window->surface, window->gc, 0, 0, window->width, window->height, 0, 0);
	XFreePixmap(xfi->display, window->surface);
	window->surface = surface;

	window->x = x;
	window->y = y;
	window->width = width;
	window->height = height;
}

void xf_ShowWindow(xfInfo* xfi, xfWindow* window, uint8 state)
{
	//printf("xf_ShowWindow:%d\n", state);

	switch (state)
	{
		case WINDOW_HIDE:
			//XIconifyWindow(xfi->display, window->handle, xfi->screen_number);
			break;

		case WINDOW_SHOW_MINIMIZED:
			XIconifyWindow(xfi->display, window->handle, xfi->screen_number);
			break;

		case WINDOW_SHOW_MAXIMIZED:
			XRaiseWindow(xfi->display, window->handle);
			break;

		case WINDOW_SHOW:
			XRaiseWindow(xfi->display, window->handle);
			break;
	}

	XFlush(xfi->display);
}

void xf_SetWindowIcon(xfInfo* xfi, xfWindow* window, rdpIcon* icon)
{
	Atom atom;
	int x, y;
	int pixels;
	int propsize;
	long* propdata;
	long* dstp;
	uint32* srcp;

	if (icon->big != True)
		return;

	pixels = icon->entry->width * icon->entry->height;
	propsize = 2 + pixels;
	propdata = xmalloc(propsize * sizeof(long));

	propdata[0] = icon->entry->width;
	propdata[1] = icon->entry->height;
	dstp = &(propdata[2]);
	srcp = (uint32*) icon->extra;

	for (y = 0; y < icon->entry->height; y++)
	{
		for (x = 0; x < icon->entry->width; x++)
		{
			*dstp++ = *srcp++;
		}
	}

	atom = XInternAtom(xfi->display, "_NET_WM_ICON", False);

	if (!atom)
	{
		printf("xf_SetWindowIcon: failed to obtain atom _NET_WM_ICON\n");
		return;
	}
	else
	{
		XChangeProperty(xfi->display, window->handle, atom, XA_CARDINAL, 32,
			PropModeReplace, (uint8*) propdata, propsize);

		XFlush(xfi->display);
	}
}

void xf_DestroyWindow(xfInfo* xfi, xfWindow* window)
{
	XFreeGC(xfi->display, window->gc);
	XFreePixmap(xfi->display, window->surface);
	XUnmapWindow(xfi->display, window->handle);
	XDestroyWindow(xfi->display, window->handle);
	xfree(window);
}
