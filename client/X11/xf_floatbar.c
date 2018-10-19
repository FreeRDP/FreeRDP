/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Windows
 *
 * Licensed under the Apache License, Version 2.0 (the "License");n
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
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <X11/cursorfont.h>

#include "xf_floatbar.h"
#include "resource/close.xbm"
#include "resource/lock.xbm"
#include "resource/unlock.xbm"
#include "resource/minimize.xbm"
#include "resource/restore.xbm"

#define TAG CLIENT_TAG("x11")

#define FLOATBAR_HEIGHT				26
#define FLOATBAR_DEFAULT_WIDTH		576
#define FLOATBAR_MIN_WIDTH			200
#define FLOATBAR_BORDER				24
#define FLOATBAR_BUTTON_WIDTH		24
#define FLOATBAR_COLOR_BACKGROUND 	"RGB:31/6c/a9"
#define FLOATBAR_COLOR_BORDER 		"RGB:75/9a/c8"
#define FLOATBAR_COLOR_FOREGROUND 	"RGB:FF/FF/FF"

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_X11(...) do { } while (0)
#endif

#define XF_FLOATBAR_MODE_NONE 			0
#define XF_FLOATBAR_MODE_DRAGGING 		1
#define XF_FLOATBAR_MODE_RESIZE_LEFT 	2
#define XF_FLOATBAR_MODE_RESIZE_RIGHT 	3


#define XF_FLOATBAR_BUTTON_CLOSE 	1
#define XF_FLOATBAR_BUTTON_RESTORE 	2
#define XF_FLOATBAR_BUTTON_MINIMIZE 3
#define XF_FLOATBAR_BUTTON_LOCKED 	4

typedef struct xf_floatbar_button xfFloatbarButton;

struct xf_floatbar
{
	int x;
	int y;
	int width;
	int height;
	int mode;
	int last_motion_x_root;
	int last_motion_y_root;
	bool locked;
	xfFloatbarButton* buttons[4];
	Window handle;
};

struct xf_floatbar_button
{
	int x;
	int y;
	int type;
	bool focus;
	bool clicked;
	OnClick onclick;
	Window handle;
};

static void xf_floatbar_button_onclick_close(xfContext* xfc)
{
	ExitProcess(EXIT_SUCCESS);
}

static void xf_floatbar_button_onclick_minimize(xfContext* xfc)
{
	xf_SetWindowMinimized(xfc, xfc->window);
}

static void xf_floatbar_button_onclick_restore(xfContext* xfc)
{
	xf_toggle_fullscreen(xfc);
}

static void xf_floatbar_button_onclick_locked(xfContext* xfc)
{
	xfFloatbar* floatbar;
	floatbar = xfc->window->floatbar;
	floatbar->locked = (floatbar->locked) ? FALSE : TRUE;
}

void xf_floatbar_set_root_y(xfContext* xfc, int y)
{
	xfFloatbar* floatbar;
	floatbar = xfc->window->floatbar;
	floatbar->last_motion_y_root = y;
}

void xf_floatbar_hide_and_show(xfContext* xfc)
{
	xfFloatbar* floatbar;
	floatbar = xfc->window->floatbar;

	if (!floatbar->locked)
	{
		if ((floatbar->mode == 0) && (floatbar->last_motion_y_root > 10) &&
		    (floatbar->y > (FLOATBAR_HEIGHT * -1)))
		{
			floatbar->y = floatbar->y - 1;
			XMoveWindow(xfc->display, floatbar->handle, floatbar->x, floatbar->y);
		}
		else if (floatbar->y < 0 && (floatbar->last_motion_y_root < 10))
		{
			floatbar->y = floatbar->y + 1;
			XMoveWindow(xfc->display, floatbar->handle, floatbar->x, floatbar->y);
		}
	}
}

void xf_floatbar_toggle_visibility(xfContext* xfc, bool visible)
{
	xfFloatbar* floatbar;
	int i, size;
	floatbar = xfc->window->floatbar;

	if (visible)
	{
		XMapWindow(xfc->display, floatbar->handle);
		size = ARRAYSIZE(floatbar->buttons);

		for (i = 0; i < size; i++)
		{
			XMapWindow(xfc->display, floatbar->buttons[i]->handle);
		}
	}
	else
	{
		XUnmapSubwindows(xfc->display, floatbar->handle);
		XUnmapWindow(xfc->display, floatbar->handle);
	}
}

static xfFloatbarButton* xf_floatbar_new_button(xfContext* xfc, xfFloatbar* floatbar, int type)
{
	xfFloatbarButton* button;
	button = (xfFloatbarButton*) calloc(1, sizeof(xfFloatbarButton));
	button->type = type;

	switch (type)
	{
		case XF_FLOATBAR_BUTTON_CLOSE:
			button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * type;
			button->onclick = xf_floatbar_button_onclick_close;
			break;

		case XF_FLOATBAR_BUTTON_RESTORE:
			button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * type;
			button->onclick = xf_floatbar_button_onclick_restore;
			break;

		case XF_FLOATBAR_BUTTON_MINIMIZE:
			button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * type;
			button->onclick = xf_floatbar_button_onclick_minimize;
			break;

		case XF_FLOATBAR_BUTTON_LOCKED:
			button->x = FLOATBAR_BORDER;
			button->onclick = xf_floatbar_button_onclick_locked;
			break;

		default:
			break;
	}

	button->y = 0;
	button->focus = FALSE;
	button->handle = XCreateWindow(xfc->display, floatbar->handle, button->x, 0, FLOATBAR_BUTTON_WIDTH,
	                               FLOATBAR_BUTTON_WIDTH, 0, CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
	XSelectInput(xfc->display, button->handle, ExposureMask | ButtonPressMask | ButtonReleaseMask |
	             FocusChangeMask | LeaveWindowMask | EnterWindowMask | StructureNotifyMask);
	return button;
}

xfFloatbar* xf_floatbar_new(xfContext* xfc, Window window)
{
	xfFloatbar* floatbar;
	XWindowAttributes attr;
	int i, width;
	if (!xfc)
		return NULL;

	floatbar = (xfFloatbar*) calloc(1, sizeof(xfFloatbar));
	floatbar->locked = TRUE;
	XGetWindowAttributes(xfc->display, window, &attr);

	for (i = 0; i < xfc->vscreen.nmonitors; i++)
	{
		if (attr.x >= xfc->vscreen.monitors[i].area.left && attr.x <= xfc->vscreen.monitors[i].area.right)
		{
			width = xfc->vscreen.monitors[i].area.right - xfc->vscreen.monitors[i].area.left;
			floatbar->x = width / 2 + xfc->vscreen.monitors[i].area.left - FLOATBAR_DEFAULT_WIDTH / 2;
		}
	}

	floatbar->y = 0;
	floatbar->handle = XCreateWindow(xfc->display, window, floatbar->x, 0, FLOATBAR_DEFAULT_WIDTH,
	                                 FLOATBAR_HEIGHT, 0,
	                                 CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
	floatbar->width = FLOATBAR_DEFAULT_WIDTH;
	floatbar->height = FLOATBAR_HEIGHT;
	floatbar->mode = XF_FLOATBAR_MODE_NONE;
	floatbar->buttons[0] = xf_floatbar_new_button(xfc, floatbar, XF_FLOATBAR_BUTTON_CLOSE);
	floatbar->buttons[1] = xf_floatbar_new_button(xfc, floatbar, XF_FLOATBAR_BUTTON_RESTORE);
	floatbar->buttons[2] = xf_floatbar_new_button(xfc, floatbar, XF_FLOATBAR_BUTTON_MINIMIZE);
	floatbar->buttons[3] = xf_floatbar_new_button(xfc, floatbar, XF_FLOATBAR_BUTTON_LOCKED);
	XSelectInput(xfc->display, floatbar->handle, ExposureMask | ButtonPressMask | ButtonReleaseMask |
	             PointerMotionMask | FocusChangeMask | LeaveWindowMask | EnterWindowMask | StructureNotifyMask |
	             PropertyChangeMask);
	return floatbar;
}

static unsigned long xf_floatbar_get_color(xfContext* xfc, char* rgb_value)
{
	Colormap cmap;
	XColor color;
	cmap = DefaultColormap(xfc->display, XDefaultScreen(xfc->display));
	XParseColor(xfc->display, cmap, rgb_value, &color);
	XAllocColor(xfc->display, cmap, &color);
	XFreeColormap(xfc->display, cmap);
	return color.pixel;
}

static void xf_floatbar_event_expose(xfContext* xfc, XEvent* event)
{
	GC gc, shape_gc;
	Pixmap pmap;
	XPoint shape[5], border[5];
	xfFloatbar* floatbar;
	int len;
	floatbar = xfc->window->floatbar;
	/* create the pixmap that we'll use for shaping the window */
	pmap = XCreatePixmap(xfc->display, floatbar->handle, floatbar->width, floatbar->height, 1);
	gc = XCreateGC(xfc->display, floatbar->handle, 0, 0);
	shape_gc = XCreateGC(xfc->display, pmap, 0, 0);
	/* points for drawing the floatbar */
	shape[0].x = 0;
	shape[0].y = 0;
	shape[1].x = floatbar->width;
	shape[1].y = 0;
	shape[2].x = shape[1].x - FLOATBAR_BORDER;
	shape[2].y = FLOATBAR_HEIGHT;
	shape[3].x = shape[0].x + FLOATBAR_BORDER;
	shape[3].y = FLOATBAR_HEIGHT;
	shape[4].x = shape[0].x;
	shape[4].y = shape[0].y;
	/* points for drawing the border of the floatbar */
	border[0].x = shape[0].x;
	border[0].y = shape[0].y - 1;
	border[1].x = shape[1].x - 1;
	border[1].y = shape[1].y - 1;
	border[2].x = shape[2].x;
	border[2].y = shape[2].y - 1;
	border[3].x = shape[3].x - 1;
	border[3].y = shape[3].y - 1;
	border[4].x = border[0].x;
	border[4].y = border[0].y;
	/* Fill all pixels with 0 */
	XSetForeground(xfc->display, shape_gc, 0);
	XFillRectangle(xfc->display, pmap, shape_gc, 0, 0, floatbar->width,
	               floatbar->height);
	/* Fill all pixels which should be shown with 1 */
	XSetForeground(xfc->display, shape_gc, 1);
	XFillPolygon(xfc->display, pmap, shape_gc, shape, 5, 0, CoordModeOrigin);
	XShapeCombineMask(xfc->display, floatbar->handle, ShapeBounding, 0, 0, pmap, ShapeSet);
	/* draw the float bar */
	XSetForeground(xfc->display, gc, xf_floatbar_get_color(xfc, FLOATBAR_COLOR_BACKGROUND));
	XFillPolygon(xfc->display, floatbar->handle, gc, shape, 4, 0, CoordModeOrigin);
	/* draw an border for the floatbar */
	XSetForeground(xfc->display, gc, xf_floatbar_get_color(xfc, FLOATBAR_COLOR_BORDER));
	XDrawLines(xfc->display, floatbar->handle, gc, border, 5, CoordModeOrigin);
	/* draw the host name connected to */
	len = strlen(xfc->context.settings->ServerHostname);
	XSetForeground(xfc->display, gc, xf_floatbar_get_color(xfc, FLOATBAR_COLOR_FOREGROUND));
	XDrawString(xfc->display, floatbar->handle, gc, floatbar->width / 2 - len * 2, 15,
	            xfc->context.settings->ServerHostname, len);
	XFreeGC(xfc->display, gc);
	XFreeGC(xfc->display, shape_gc);
}

static xfFloatbarButton* xf_floatbar_get_button(xfContext* xfc, XEvent* event)
{
	xfFloatbar* floatbar;
	int i, size;
	size = ARRAYSIZE(floatbar->buttons);
	floatbar = xfc->window->floatbar;

	for (i = 0; i < size; i++)
	{
		if (floatbar->buttons[i]->handle == event->xany.window)
		{
			return floatbar->buttons[i];
		}
	}

	return NULL;
}

static void xf_floatbar_button_update_positon(xfContext* xfc, XEvent* event)
{
	xfFloatbar* floatbar;
	xfFloatbarButton* button;
	int i, size;
	floatbar = xfc->window->floatbar;
	size = ARRAYSIZE(floatbar->buttons);

	for (i = 0; i < size; i++)
	{
		button = floatbar->buttons[i];

		switch (button->type)
		{
			case XF_FLOATBAR_BUTTON_CLOSE:
				button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			case XF_FLOATBAR_BUTTON_RESTORE:
				button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			case XF_FLOATBAR_BUTTON_MINIMIZE:
				button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			default:
				break;
		}

		XMoveWindow(xfc->display, button->handle, button->x, button->y);
		xf_floatbar_event_expose(xfc, event);
	}
}

static void xf_floatbar_button_event_expose(xfContext* xfc, XEvent* event)
{
	xfFloatbar* floatbar;
	xfFloatbarButton* button;
	static unsigned char* bits;
	GC gc;
	Pixmap pattern;
	button = xf_floatbar_get_button(xfc, event);

	if (!button)
		return;

	gc = XCreateGC(xfc->display, button->handle, 0, 0);
	floatbar = xfc->window->floatbar;

	switch (button->type)
	{
		case XF_FLOATBAR_BUTTON_CLOSE:
			bits = close_bits;
			break;

		case XF_FLOATBAR_BUTTON_RESTORE:
			bits = restore_bits;
			break;

		case XF_FLOATBAR_BUTTON_MINIMIZE:
			bits = minimize_bits;
			break;

		case XF_FLOATBAR_BUTTON_LOCKED:
			if (floatbar->locked)
				bits = lock_bits;
			else
				bits = unlock_bits;

			break;

		default:
			break;
	}

	pattern = XCreateBitmapFromData(xfc->display, button->handle, (const char*)bits,
	                                FLOATBAR_BUTTON_WIDTH, FLOATBAR_BUTTON_WIDTH);

	if (!(button->focus))
		XSetForeground(xfc->display, gc, xf_floatbar_get_color(xfc, FLOATBAR_COLOR_BACKGROUND));
	else
		XSetForeground(xfc->display, gc, xf_floatbar_get_color(xfc, FLOATBAR_COLOR_BORDER));

	XSetBackground(xfc->display, gc, xf_floatbar_get_color(xfc, FLOATBAR_COLOR_FOREGROUND));
	XCopyPlane(xfc->display, pattern, button->handle, gc, 0, 0, FLOATBAR_BUTTON_WIDTH,
	           FLOATBAR_BUTTON_WIDTH, 0, 0, 1);
	XFreePixmap(xfc->display, pattern);
	XFreeGC(xfc->display, gc);
}

static void xf_floatbar_button_event_buttonpress(xfContext* xfc, XEvent* event)
{
	xfFloatbarButton* button;
	button = xf_floatbar_get_button(xfc, event);

	if (button)
		button->clicked = TRUE;
}

static void xf_floatbar_button_event_buttonrelease(xfContext* xfc, XEvent* event)
{
	xfFloatbarButton* button;
	button = xf_floatbar_get_button(xfc, event);

	if (button)
	{
		if (button->clicked)
			button->onclick(xfc);
	}
}

static void xf_floatbar_event_buttonpress(xfContext* xfc, XEvent* event)
{
	xfFloatbar* floatbar;
	floatbar = xfc->window->floatbar;

	switch (event->xbutton.button)
	{
		case Button1:
			if (event->xmotion.x <= FLOATBAR_BORDER)
				floatbar->mode = XF_FLOATBAR_MODE_RESIZE_LEFT;
			else if	(event->xmotion.x >= (floatbar->width - FLOATBAR_BORDER))
				floatbar->mode = XF_FLOATBAR_MODE_RESIZE_RIGHT;
			else
				floatbar->mode = XF_FLOATBAR_MODE_DRAGGING;

			break;

		default:
			break;
	}
}

static void xf_floatbar_event_buttonrelease(xfContext* xfc, XEvent* event)
{
	switch (event->xbutton.button)
	{
		case Button1:
			xfc->window->floatbar->mode = XF_FLOATBAR_MODE_NONE;
			break;

		default:
			break;
	}
}

static void xf_floatbar_resize(xfContext* xfc, XEvent* event)
{
	xfFloatbar* floatbar;
	floatbar = xfc->window->floatbar;
	int x, width, movement;
	/* calculate movement which happened on the root window */
	movement = event->xmotion.x_root - floatbar->last_motion_x_root;

	/* set x and width depending if movement happens on the left or right  */
	if (floatbar->mode == XF_FLOATBAR_MODE_RESIZE_LEFT)
	{
		x = floatbar->x + movement;
		width = floatbar->width + movement * -1;
	}
	else
	{
		x = floatbar->x;
		width = floatbar->width + movement;
	}

	/* only resize and move window if still above minimum width */
	if (FLOATBAR_MIN_WIDTH < width)
	{
		XMoveResizeWindow(xfc->display, floatbar->handle, x, 0, width, floatbar->height);
		floatbar->x = x;
		floatbar->width = width;
	}
}

static void xf_floatbar_dragging(xfContext* xfc, XEvent* event)
{
	xfFloatbar* floatbar;
	floatbar = xfc->window->floatbar;
	int x, movement;
	/* calculate movement and new x position */
	movement = event->xmotion.x_root - floatbar->last_motion_x_root;
	x = floatbar->x + movement;

	/* do nothing if floatbar would be moved out of the window */
	if (x < 0 || (x + floatbar->width) > xfc->window->width)
		return;

	/* move window to new x position */
	XMoveWindow(xfc->display, floatbar->handle, x, 0);
	/* update struct values for the next event */
	floatbar->last_motion_x_root = floatbar->last_motion_x_root + movement;
	floatbar->x = x;
}

static void xf_floatbar_event_motionnotify(xfContext* xfc, XEvent* event)
{
	int mode;
	xfFloatbar* floatbar;
	Cursor cursor;
	mode = xfc->window->floatbar->mode;
	floatbar = xfc->window->floatbar;
	cursor = XCreateFontCursor(xfc->display, XC_arrow);

	if ((event->xmotion.state & Button1Mask) && (mode > XF_FLOATBAR_MODE_DRAGGING))
	{
		xf_floatbar_resize(xfc, event);
	}
	else if ((event->xmotion.state & Button1Mask) && (mode == XF_FLOATBAR_MODE_DRAGGING))
	{
		xf_floatbar_dragging(xfc, event);
	}
	else
	{
		if (event->xmotion.x <= FLOATBAR_BORDER ||
		    event->xmotion.x >= xfc->window->floatbar->width - FLOATBAR_BORDER)
			cursor = XCreateFontCursor(xfc->display, XC_sb_h_double_arrow);
	}

	XDefineCursor(xfc->display, xfc->window->handle, cursor);
	XFreeCursor(xfc->display, cursor);
	xfc->window->floatbar->last_motion_x_root = event->xmotion.x_root;
}

static void xf_floatbar_button_event_focusin(xfContext* xfc, XEvent* event)
{
	xfFloatbarButton* button;
	button = xf_floatbar_get_button(xfc, event);

	if (button)
	{
		button->focus = TRUE;
		xf_floatbar_button_event_expose(xfc, event);
	}
}

static void xf_floatbar_button_event_focusout(xfContext* xfc, XEvent* event)
{
	xfFloatbarButton* button;
	button = xf_floatbar_get_button(xfc, event);

	if (button)
	{
		button->focus = FALSE;
		button->clicked = FALSE;
		xf_floatbar_button_event_expose(xfc, event);
	}
}

static void xf_floatbar_event_focusout(xfContext* xfc, XEvent* event)
{
	Cursor cursor;
	cursor = XCreateFontCursor(xfc->display, XC_arrow);
	XDefineCursor(xfc->display, xfc->window->handle, cursor);
	XFreeCursor(xfc->display, cursor);
}

BOOL xf_floatbar_check_event(xfContext* xfc, XEvent* event)
{
	xfFloatbar* floatbar;
	xfFloatbarButton* button;
	size_t i, size;

	if (!xfc || !event || !xfc->window)
		return FALSE;

	floatbar = xfc->window->floatbar;

	if (event->xany.window == floatbar->handle)
		return TRUE;

	size = ARRAYSIZE(floatbar->buttons);

	for (i = 0; i < size; i++)
	{
		button = floatbar->buttons[i];

		if (event->xany.window == button->handle)
			return TRUE;
	}

	return FALSE;
}

BOOL xf_floatbar_event_process(xfContext* xfc, XEvent* event)
{
	xfFloatbar* floatbar;

	if (!xfc || !xfc->window || !event)
		return FALSE;

	floatbar = xfc->window->floatbar;

	switch (event->type)
	{
		case Expose:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_expose(xfc, event);
			else
				xf_floatbar_button_event_expose(xfc, event);

			break;

		case MotionNotify:
			xf_floatbar_event_motionnotify(xfc, event);
			break;

		case ButtonPress:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_buttonpress(xfc, event);
			else
				xf_floatbar_button_event_buttonpress(xfc, event);

			break;

		case ButtonRelease:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_buttonrelease(xfc, event);
			else
				xf_floatbar_button_event_buttonrelease(xfc, event);

			break;

		case EnterNotify:
		case FocusIn:
			if (event->xany.window != floatbar->handle)
				xf_floatbar_button_event_focusin(xfc, event);

			break;

		case LeaveNotify:
		case FocusOut:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_focusout(xfc, event);
			else
				xf_floatbar_button_event_focusout(xfc, event);

			break;

		case ConfigureNotify:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_button_update_positon(xfc, event);

			break;

		case PropertyNotify:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_button_update_positon(xfc, event);

			break;

		default:
			break;
	}

	return floatbar->handle == event->xany.window;
}

static void xf_floatbar_button_free(xfContext* xfc, xfFloatbarButton* button)
{
	if (!button)
		return;

	if (button->handle)
	{
		XUnmapWindow(xfc->display, button->handle);
		XDestroyWindow(xfc->display, button->handle);
	}

	free(button);
}

void xf_floatbar_free(xfContext* xfc, xfWindow* window, xfFloatbar* floatbar)
{
	size_t i, size;
	size = ARRAYSIZE(floatbar->buttons);

	if (!floatbar)
		return;

	if (window->floatbar == floatbar)
		window->floatbar = NULL;

	for (i = 0; i < size; i++)
	{
		xf_floatbar_button_free(xfc, floatbar->buttons[i]);
		floatbar->buttons[i] = NULL;
	}

	if (floatbar->handle)
	{
		XUnmapWindow(xfc->display, floatbar->handle);
		XDestroyWindow(xfc->display, floatbar->handle);
	}

	free(floatbar);
}
