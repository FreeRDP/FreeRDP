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

void window_fullscreen(xfInfo* xfi, xfWindow* window, boolean fullscreen)
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

	printf("x:%d y:%d w:%d h:%d\n", xfi->workArea.x, xfi->workArea.y, xfi->workArea.width, xfi->workArea.height);

	return True;
}

void window_move(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height)
{
	XWindowChanges changes;

	changes.x = x;
	changes.y = y;
	changes.width = width;
	changes.height = height;

	XConfigureWindow(xfi->display, window->handle, CWX | CWY | CWWidth | CWHeight, &changes);
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

xfWindow* window_create(xfInfo* xfi, char* name)
{
	xfWindow* window;

	window = (xfWindow*) xzalloc(sizeof(xfWindow));

	if (window != NULL)
	{
		int input_mask;
		XSizeHints* size_hints;
		XClassHint* class_hints;

		window->decorations = True;
		window->fullscreen = False;

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

xfWindow* xf_CreateWindow(xfInfo* xfi, int x, int y, int width, int height, char* name)
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
			class_hints->res_name = "rail";
			class_hints->res_class = "freerdp";
			XSetClassHint(xfi->display, window->handle, class_hints);
			XFree(class_hints);
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

		XStoreName(xfi->display, window->handle, name);

		window_move(xfi, window, x, y, width, height);
	}

	return window;
}

void xf_DestroyWindow(xfInfo* xfi, xfWindow* window)
{
	XFreeGC(xfi->display, window->gc);
	XFreePixmap(xfi->display, window->surface);
	XUnmapWindow(xfi->display, window->handle);
	XDestroyWindow(xfi->display, window->handle);
	xfree(window);
}
