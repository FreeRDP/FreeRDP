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

#include <freerdp/rail.h>
#include <freerdp/utils/rail.h>


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

                XMoveResizeWindow(xfi->display, window->handle, 0, 0, window->width, window->height);
                XMapRaised(xfi->display, window->handle);

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

void xf_SetWindowUnlisted(xfInfo* xfi, xfWindow* window)
{
	Atom window_state[2];

	window_state[0] = xfi->_NET_WM_STATE_SKIP_PAGER;
	window_state[1] = xfi->_NET_WM_STATE_SKIP_TASKBAR;

	XChangeProperty(xfi->display, window->handle, xfi->_NET_WM_STATE,
		XA_ATOM, 32, PropModeReplace, (uint8*) &window_state, 2);
}

void xf_SetWindowStyle(xfInfo* xfi, xfWindow* window, uint32 style, uint32 ex_style)
{
	Atom window_type;

	window_type = xfi->_NET_WM_WINDOW_TYPE_NORMAL;

	if ((style & WS_POPUP) || (style & WS_DLGFRAME) || (ex_style & WS_EX_DLGMODALFRAME))
	{
		window_type = xfi->_NET_WM_WINDOW_TYPE_DIALOG;
	}

	if (ex_style & WS_EX_TOOLWINDOW)
	{
		xf_SetWindowUnlisted(xfi, window);
		window_type = xfi->_NET_WM_WINDOW_TYPE_UTILITY;
	}

	XChangeProperty(xfi->display, window->handle, xfi->_NET_WM_WINDOW_TYPE,
		XA_ATOM, 32, PropModeReplace, (uint8*) &window_type, 1);
}

xfWindow* xf_CreateDesktopWindow(xfInfo* xfi, char* name, int width, int height)
{
	xfWindow* window;

	window = (xfWindow*) xzalloc(sizeof(xfWindow));

	if (window != NULL)
	{
		int input_mask;
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
			class_hints->res_name = "xfreerdp";
			class_hints->res_class = "xfreerdp";
			XSetClassHint(xfi->display, window->handle, class_hints);
			XFree(class_hints);
		}

		xf_ResizeDesktopWindow(xfi, window, width, height);

		input_mask =
			KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
			VisibilityChangeMask | FocusChangeMask | StructureNotifyMask |
			PointerMotionMask | ExposureMask | EnterWindowMask | LeaveWindowMask |
			PropertyChangeMask;

		XSelectInput(xfi->display, window->handle, input_mask);
		XMapWindow(xfi->display, window->handle);
	}

	XStoreName(xfi->display, window->handle, name);

	return window;
}

void xf_ResizeDesktopWindow(xfInfo* xfi, xfWindow* window, int width, int height)
{
	XSizeHints* size_hints;

	size_hints = XAllocSizeHints();

	if (size_hints)
	{
		size_hints->flags = PMinSize | PMaxSize;
		size_hints->min_width = size_hints->max_width = xfi->width;
		size_hints->min_height = size_hints->max_height = xfi->height;
		XSetWMNormalHints(xfi->display, window->handle, size_hints);
		XFree(size_hints);
	}
}

void xf_FixWindowCoordinates(xfInfo* xfi, int* x, int* y, int* width, int* height)
{
	int vscreen_width;
	int vscreen_height;

	vscreen_width = xfi->vscreen.area.right - xfi->vscreen.area.left + 1;
	vscreen_height = xfi->vscreen.area.bottom - xfi->vscreen.area.top + 1;

	if (*x < xfi->vscreen.area.left)
	{
		*width += *x;
		*x = xfi->vscreen.area.left;
	}
	if (*y < xfi->vscreen.area.top)
	{
		*height += *y;
		*y = xfi->vscreen.area.top;
	}
	if (*width > vscreen_width)
	{
		*width = vscreen_width;
	}
	if (*height > vscreen_height)
	{
		*height = vscreen_height;
	}
}

char rail_window_class[] = "RAIL:00000000";

xfWindow* xf_CreateWindow(xfInfo* xfi, rdpWindow* wnd, int x, int y, int width, int height, uint32 id)
{
	xfWindow* window;

	window = (xfWindow*) xzalloc(sizeof(xfWindow));

	if ((width * height) < 1)
		return NULL;

	xf_FixWindowCoordinates(xfi, &x, &y, &width, &height);

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

		window->decorations = False;
		window->fullscreen = False;
		window->window = wnd;
		window->localMoveSize = False;

		window->handle = XCreateWindow(xfi->display, RootWindowOfScreen(xfi->screen),
			x, y, window->width, window->height, 0, xfi->depth, InputOutput, xfi->visual,
			CWBackPixel | CWBackingStore | CWOverrideRedirect | CWColormap |
			CWBorderPixel, &xfi->attribs);

		xf_SetWindowDecorations(xfi, window, window->decorations);

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

		XSetWMProtocols(xfi->display, window->handle, &(xfi->WM_DELETE_WINDOW), 1);

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

void xf_SetWindowMinMaxInfo(xfInfo* xfi, xfWindow* window,
		int maxWidth, int maxHeight, int maxPosX, int maxPosY,
		int minTrackWidth, int minTrackHeight, int maxTrackWidth, int maxTrackHeight)
{
	XSizeHints* size_hints;

	size_hints = XAllocSizeHints();

	if (size_hints)
	{
		size_hints->flags = PMinSize | PMaxSize | PResizeInc;

		size_hints->min_width  = minTrackWidth;
		size_hints->min_height = minTrackHeight;

		size_hints->max_width  = maxTrackWidth;
		size_hints->max_height = maxTrackHeight;

		/* to speedup window drawing we need to select optimal value for sizing step. */
		size_hints->width_inc = size_hints->height_inc = 1;

		XSetWMNormalHints(xfi->display, window->handle, size_hints);
		XFree(size_hints);
	}
}


void xf_SendMoveResizeEvent(xfInfo* xfi, xfWindow* window, int direction, int x_root, int y_root)
{
	// TODO:
	// - how to receive movesize canceling event?
	// - how to produce correct RAIL movesize finish?
	// - how to receive move/size window coordinates in process of local move/size?

	XEvent event;

	event.xclient.type = ClientMessage;
	event.xclient.window = window->handle;
	event.xclient.message_type = xfi->_NET_WM_MOVERESIZE;
	event.xclient.serial = 0;
	event.xclient.display = xfi->display;
	event.xclient.send_event = True;
	event.xclient.format = 32;
	event.xclient.data.l[0] = x_root;
	event.xclient.data.l[1] = y_root;
	event.xclient.data.l[2] = direction;
	event.xclient.data.l[3] = 1; /* button 1 */
	event.xclient.data.l[4] = 0;

	XUngrabPointer(xfi->display, CurrentTime);
	XSendEvent(xfi->display, RootWindowOfScreen(xfi->screen), False, SubstructureNotifyMask, &event);
}

void xf_StartLocalMoveSize(xfInfo* xfi, xfWindow* window, uint16 moveSizeType, int posX, int posY)
{
	window->localMoveSize = True;
}

void xf_StopLocalMoveSize(xfInfo* xfi, xfWindow* window, uint16 moveSizeType, int posX, int posY)
{
	window->localMoveSize = False;
}

void xf_MoveWindow(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height)
{
	boolean resize = False;

	if ((width * height) < 1)
		return;

	if ((window->width != width) || (window->height != height))
		resize = True;

	if (resize)
		XMoveResizeWindow(xfi->display, window->handle, x, y, width, height);
	else
		XMoveWindow(xfi->display, window->handle, x, y);

	window->left = x;
	window->top = y;
	window->right = x + width - 1;
	window->bottom = y + height - 1;
	window->width = width;
	window->height = height;

	DEBUG_X11_LMS("xf_MoveWindow: window=0x%X rc={l=%d t=%d r=%d b=%d} w=%d h=%d",
			(uint32) window->handle, window->left, window->top, window->right, window->bottom,
			window->width, window->height);

	if (resize)
	{
		XFreePixmap(xfi->display, window->surface);
		window->surface = XCreatePixmap(xfi->display, window->handle, width, height, xfi->depth);
		xf_UpdateWindowArea(xfi, window, 0, 0, width, height);
	}
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

void xf_SetWindowRects(xfInfo* xfi, xfWindow* window, RECTANGLE_16* rects, int nrects)
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
	//XShapeCombineRectangles(xfi->display, window->handle, ShapeBounding, 0, 0, xrects, nrects, ShapeSet, 0);
#endif

	xfree(xrects);
}

void xf_UpdateWindowArea(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height)
{
	int ax, ay;
	rdpWindow* wnd;

	wnd = window->window;
	ax = x + wnd->windowOffsetX;
	ay = y + wnd->windowOffsetY;

	if (ax + width > wnd->windowOffsetX + wnd->windowWidth)
		width = (wnd->windowOffsetX + wnd->windowWidth - 1) - ax;

	if (ay + height > wnd->windowOffsetY + wnd->windowHeight)
		height = (wnd->windowOffsetY + wnd->windowHeight - 1) - ay;

	if (xfi->sw_gdi)
	{
		XPutImage(xfi->display, xfi->primary, window->gc, xfi->image,
			ax, ay, ax, ay, width, height);
	}

	XCopyArea(xfi->display, xfi->primary, window->handle, window->gc,
			ax, ay, width, height, x, y);

	XFlush(xfi->display);
}

boolean xf_IsWindowBorder(xfInfo* xfi, xfWindow* xfw, int x, int y)
{
	rdpWindow* wnd;
	boolean clientArea = False;
	boolean windowArea = False;

	wnd = xfw->window;

	if (((x > wnd->clientOffsetX) && (x < wnd->clientOffsetX + wnd->clientAreaWidth)) &&
		((y > wnd->clientOffsetY) && (y < wnd->clientOffsetY + wnd->clientAreaHeight)))
		clientArea = True;

	if (((x > wnd->windowOffsetX) && (x < wnd->windowOffsetX + wnd->windowWidth)) &&
		((y > wnd->windowOffsetY) && (y < wnd->windowOffsetY + wnd->windowHeight)))
		windowArea = True;

	return (windowArea && !(clientArea));
}

void xf_DestroyWindow(xfInfo* xfi, xfWindow* window)
{
	if (window == NULL)
		return;

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
