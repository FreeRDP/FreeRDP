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
#include <X11/Xatom.h>

#ifdef WITH_XEXT
#include <X11/extensions/shape.h>
#endif

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

void xf_RefrenceWindow(xfInfo* xfi, xfWindow* window);
void xf_DerefrenceWindow(xfInfo* xfi, xfWindow* window);

void xf_SendClientMessage(xfInfo* xfi, xfWindow* window, Atom atom, long msg, long d1, long d2, long d3)
{
	XEvent xevent;

	xevent.xclient.type = ClientMessage;
	xevent.xclient.message_type = atom;
	xevent.xclient.window = window->handle;
	xevent.xclient.format = 32;
	xevent.xclient.data.l[0] = CurrentTime;
	xevent.xclient.data.l[1] = msg;
	xevent.xclient.data.l[2] = d1;
	xevent.xclient.data.l[3] = d2;
	xevent.xclient.data.l[4] = d3;

	XSendEvent(xfi->display, window->handle, False, NoEventMask, &xevent);
	XSync(xfi->display, False);
}

void xf_SetWindowFullscreen(xfInfo* xfi, xfWindow* window, boolean fullscreen)
{
	if (fullscreen)
	{
		if (window->decorations)
			xf_SetWindowDecorations(xfi, window, False);

		printf("width:%d height:%d\n", window->width, window->height);

                XMoveResizeWindow(xfi->display, window->handle, 0, 0, window->width, window->height);
                XMapRaised(xfi->display, window->handle);
                //XGrabPointer(xfi->display, window->handle, True, 0, GrabModeAsync, GrabModeAsync, window->handle, 0L, CurrentTime);
                //XGrabKeyboard(xfi->display, window->handle, False, GrabModeAsync, GrabModeAsync, CurrentTime);

		//XSetInputFocus(xfi->display, window->handle, RevertToParent, CurrentTime);
		window->fullscreen = True;
	}
}

/* http://tronche.com/gui/x/xlib/window-information/XGetWindowProperty.html */

boolean xf_GetWindowProperty(xfInfo* xfi, Window window, Atom property, int length,
		unsigned long* nitems, unsigned long* bytes, uint8** prop)
{
	int status;
	Atom actual_type;
	int actual_format;

	if (property == None)
		return False;

	status = XGetWindowProperty(xfi->display, window,
			property, 0, length, False, AnyPropertyType,
			&actual_type, &actual_format, nitems, bytes, prop);

	if (status != Success)
		return False;

	return True;
}

boolean xf_GetCurrentDesktop(xfInfo* xfi)
{
	boolean status;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char* prop;

	status = xf_GetWindowProperty(xfi, DefaultRootWindow(xfi->display),
			xfi->_NET_CURRENT_DESKTOP, 1, &nitems, &bytes, &prop);

	if (status != True)
		return False;

	xfi->current_desktop = (int) *prop;
	xfree(prop);

	return True;
}

boolean xf_GetWorkArea(xfInfo* xfi)
{
	long* plong;
	boolean status;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char* prop;

	status = xf_GetWindowProperty(xfi, DefaultRootWindow(xfi->display),
			xfi->_NET_WORKAREA, 32 * 4, &nitems, &bytes, &prop);

	if (status != True)
		return False;

	xf_GetCurrentDesktop(xfi);

	plong = (long*) prop;

	xfi->workArea.x = plong[xfi->current_desktop * 4 + 0];
	xfi->workArea.y = plong[xfi->current_desktop * 4 + 1];
	xfi->workArea.width = plong[xfi->current_desktop * 4 + 2];
	xfi->workArea.height = plong[xfi->current_desktop * 4 + 3];
	xfree(prop);

	return True;
}

void xf_SetWindowDecorations(xfInfo* xfi, xfWindow* window, boolean show)
{
	PropMotifWmHints hints;

	hints.decorations = show;
	hints.flags = MWM_HINTS_DECORATIONS;

	XChangeProperty(xfi->display, window->handle, xfi->_MOTIF_WM_HINTS, xfi->_MOTIF_WM_HINTS, 32,
		PropModeReplace, (uint8*) &hints, PROP_MOTIF_WM_HINTS_ELEMENTS);

	window->decorations = show;
}

void xf_SetWindowStyle(xfInfo* xfi, xfWindow* window, uint32 style, uint32 ex_style)
{
	Atom window_type;

	window_type = xfi->_NET_WM_WINDOW_TYPE_NORMAL;

	if (((style & WS_POPUP) !=0) ||
		((style & WS_DLGFRAME) != 0) ||
		((ex_style & WS_EX_DLGMODALFRAME) != 0)
		)
	{
		window_type = xfi->_NET_WM_WINDOW_TYPE_DIALOG;
	}
	else if ((ex_style & WS_EX_TOOLWINDOW) != 0)
	{
		window_type = xfi->_NET_WM_WINDOW_TYPE_UTILITY;
	}

	XChangeProperty(xfi->display, window->handle, xfi->_NET_WM_WINDOW_TYPE,
		xfi->_NET_WM_WINDOW_TYPE, 32, PropModeReplace, (unsigned char*)&window_type, 1);
}

void xf_SetWindowChildState(xfInfo* xfi, xfWindow* window)
{
	Atom window_state[2];

	if (window->parent != NULL)
	{
		window_state[0] = xfi->_NET_WM_STATE_SKIP_PAGER;
		window_state[1] = xfi->_NET_WM_STATE_SKIP_TASKBAR;

		XChangeProperty(xfi->display, window->handle, xfi->_NET_WM_STATE,
			xfi->_NET_WM_STATE, 32, PropModeReplace, (unsigned char*)&window_state, 2);
	}
}


xfWindow* xf_CreateDesktopWindow(xfInfo* xfi, char* name, int width, int height)
{
	xfWindow* window;

	window = (xfWindow*) xzalloc(sizeof(xfWindow));

	if (window != NULL)
	{
		int input_mask;
		XSizeHints* size_hints;
		XClassHint* class_hints;

		window->width = width;
		window->height = height;
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

void xf_FixWindowCoordinates(int* x, int* y, int* width, int* height)
{
	if (*x < 0)
	{
		*width += *x;
		*x = 0;
	}
	if (*y < 0)
	{
		*height += *y;
		*y = 0;
	}
}

char rail_window_class[] = "RAIL:00000000";

xfWindow* xf_CreateWindow(xfInfo* xfi, xfWindow* parent, int x, int y, int width, int height, uint32 id)
{
	xfWindow* window;

	window = (xfWindow*) xzalloc(sizeof(xfWindow));

	if ((width * height) < 1)
		return NULL;

	xf_FixWindowCoordinates(&x, &y, &width, &height);

	window->left = x;
	window->top = y;
	window->right = x + width - 1;
	window->bottom = y + height - 1;
	window->width = width;
	window->height = height;

	if (window != NULL)
	{
		XGCValues gcv;
		int input_mask;
		XClassHint* class_hints;
		Window parent_handle;
		int lx;
		int ly;

		window->ref_count = 0;
		window->decorations = False;
		window->fullscreen = False;
		window->parent = parent;

		lx = x;
		ly = y;
		parent_handle = RootWindowOfScreen(xfi->screen);
	    if (window->parent != NULL)
	    {
	    	lx = x - window->parent->left;
	    	ly = y - window->parent->top;
	    	parent_handle = parent->handle;
	    }

		window->handle = XCreateWindow(xfi->display, parent_handle,
			lx, ly, window->width, window->height, 0, xfi->depth, InputOutput, xfi->visual,
			CWBackPixel | CWBackingStore | CWOverrideRedirect | CWColormap |
			CWBorderPixel, &xfi->attribs);

		xf_RefrenceWindow(xfi, window);
		xf_RefrenceWindow(xfi, window->parent);

		xf_SetWindowDecorations(xfi, window, window->decorations);
		//xf_SetWindowChildState(xfi, window);

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

		input_mask =
			KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
			VisibilityChangeMask | FocusChangeMask | StructureNotifyMask |
			PointerMotionMask | ExposureMask;

		XSelectInput(xfi->display, window->handle, input_mask);
		XMapWindow(xfi->display, window->handle);

		memset(&gcv, 0, sizeof(gcv));
		window->gc = XCreateGC(xfi->display, window->handle, GCGraphicsExposures, &gcv);
		window->surface = XCreatePixmap(xfi->display, window->handle, window->width, window->height, xfi->depth);

		printf("xf_CreateWindow: h=0x%X p=0x%X x=%d y=%d w=%d h=%d\n", (uint32)window->handle,
				(window->parent != NULL) ? (uint32)window->parent->handle : 0,
				x, y, width, height);

		xf_MoveWindow(xfi, window, x, y, width, height);
	}

	return window;
}

void xf_MoveWindow(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height)
{
	Pixmap surface;
	int    lx = x;
	int    ly = y;

	if ((width * height) < 1)
		return;

	printf("xf_MoveWindow: BEFORE correctness h=0x%X x=%d y=%d w=%d h=%d\n", (uint32)window->handle,
			x, y, width, height);

    xf_FixWindowCoordinates(&x, &y, &width, &height);

    if (window->parent != NULL)
    {
    	lx = x - window->parent->left;
    	ly = y - window->parent->top;
    }

    printf("xf_MoveWindow: AFTER correctness h=0x%X x=%d y=%d lx=%d ly=%d w=%d h=%d \n", (uint32)window->handle,
			x, y, lx, ly, width, height);

	if (window->width == width && window->height == height)
		XMoveWindow(xfi->display, window->handle, lx, ly);
	else if (window->left == x && window->top == y)
		XResizeWindow(xfi->display, window->handle, width, height);
	else
		XMoveResizeWindow(xfi->display, window->handle, lx, ly, width, height);

	surface = XCreatePixmap(xfi->display, window->handle, width, height, xfi->depth);
	XCopyArea(xfi->display, surface, window->surface, window->gc, 0, 0, window->width, window->height, 0, 0);
	XFreePixmap(xfi->display, window->surface);
	window->surface = surface;

	window->left = x;
	window->top = y;
	window->right = x + width - 1;
	window->bottom = y + height - 1;
	window->width = width;
	window->height = height;
}

void xf_ShowWindow(xfInfo* xfi, xfWindow* window, uint8 state)
{
	switch (state)
	{
		case WINDOW_HIDE:
			XWithdrawWindow(xfi->display, window->handle, xfi->screen_number);
			break;

		case WINDOW_SHOW_MINIMIZED:
			XIconifyWindow(xfi->display, window->handle, xfi->screen_number);
			break;

		case WINDOW_SHOW_MAXIMIZED:
			XRaiseWindow(xfi->display, window->handle);
			break;

		case WINDOW_SHOW:
			XMapWindow(xfi->display, window->handle);
			break;
	}

	XFlush(xfi->display);
}

void xf_SetWindowIcon(xfInfo* xfi, xfWindow* window, rdpIcon* icon)
{
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

	XChangeProperty(xfi->display, window->handle, xfi->_NET_WM_ICON, XA_CARDINAL, 32,
		PropModeReplace, (uint8*) propdata, propsize);

	XFlush(xfi->display);
}

void xf_SetWindowVisibilityRects(xfInfo* xfi, xfWindow* window, RECTANGLE_16* rects, int nrects)
{
	int i;
	XRectangle* xrects;

	xrects = xmalloc(sizeof(XRectangle) * nrects);

	for (i = 0; i < nrects; i++)
	{
		xrects[i].x = rects[i].left;
		xrects[i].y = rects[i].top;
		xrects[i].width = rects[i].right - rects[i].left;
		xrects[i].height = rects[i].bottom - rects[i].top;
	}

#ifdef WITH_XEXT
	XShapeCombineRectangles(xfi->display, window->handle, ShapeBounding, 0, 0, xrects, nrects, ShapeSet, 0);
#endif

	xfree(xrects);
}

void xf_RefrenceWindow(xfInfo* xfi, xfWindow* window)
{
	if (window == NULL) return;
	window->ref_count++;
}

void xf_DerefrenceWindow(xfInfo* xfi, xfWindow* window)
{
	if (window == NULL) return;

	window->ref_count--;
	if (window->ref_count == 0)
	{
		printf("xf_DerefrenceWindow: destroying h=0x%X p=0x%X\n", (uint32)window->handle,
				(window->parent != NULL) ? (uint32)window->parent->handle : 0);

		if (window->gc)
			XFreeGC(xfi->display, window->gc);
		if (window->surface)
			XFreePixmap(xfi->display, window->surface);
		if (window->handle)
		{
			XUnmapWindow(xfi->display, window->handle);
			XDestroyWindow(xfi->display, window->handle);
		}
		xfree(window);
	}
}

void xf_DestroyWindow(xfInfo* xfi, xfWindow* window)
{
	xfWindow* parent = window->parent;

	printf("xf_DestroyWindow: h=0x%X p=0x%X\n", (uint32)window->handle,
			(window->parent != NULL) ? (uint32)window->parent->handle : 0);

	xf_DerefrenceWindow(xfi, window);
	xf_DerefrenceWindow(xfi, parent);
}
