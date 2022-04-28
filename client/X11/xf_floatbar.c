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
#include <X11/cursorfont.h>

#include <winpr/assert.h>

#include "xf_floatbar.h"
#include "resource/close.xbm"
#include "resource/lock.xbm"
#include "resource/unlock.xbm"
#include "resource/minimize.xbm"
#include "resource/restore.xbm"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

#define FLOATBAR_HEIGHT 26
#define FLOATBAR_DEFAULT_WIDTH 576
#define FLOATBAR_MIN_WIDTH 200
#define FLOATBAR_BORDER 24
#define FLOATBAR_BUTTON_WIDTH 24
#define FLOATBAR_COLOR_BACKGROUND "RGB:31/6c/a9"
#define FLOATBAR_COLOR_BORDER "RGB:75/9a/c8"
#define FLOATBAR_COLOR_FOREGROUND "RGB:FF/FF/FF"

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_X11(...) \
	do                 \
	{                  \
	} while (0)
#endif

#define XF_FLOATBAR_MODE_NONE 0
#define XF_FLOATBAR_MODE_DRAGGING 1
#define XF_FLOATBAR_MODE_RESIZE_LEFT 2
#define XF_FLOATBAR_MODE_RESIZE_RIGHT 3

#define XF_FLOATBAR_BUTTON_CLOSE 1
#define XF_FLOATBAR_BUTTON_RESTORE 2
#define XF_FLOATBAR_BUTTON_MINIMIZE 3
#define XF_FLOATBAR_BUTTON_LOCKED 4

typedef BOOL (*OnClick)(xfFloatbar*);

typedef struct
{
	int x;
	int y;
	int type;
	bool focus;
	bool clicked;
	OnClick onclick;
	Window handle;
} xfFloatbarButton;

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
	BOOL hasCursor;
	xfContext* xfc;
	DWORD flags;
	BOOL created;
	Window root_window;
	char* title;
};

static xfFloatbarButton* xf_floatbar_new_button(xfFloatbar* floatbar, int type);

static BOOL xf_floatbar_button_onclick_close(xfFloatbar* floatbar)
{
	if (!floatbar)
		return FALSE;

	return freerdp_abort_connect_context(&floatbar->xfc->common.context);
}

static BOOL xf_floatbar_button_onclick_minimize(xfFloatbar* floatbar)
{
	xfContext* xfc;

	if (!floatbar || !floatbar->xfc)
		return FALSE;

	xfc = floatbar->xfc;
	xf_SetWindowMinimized(xfc, xfc->window);
	return TRUE;
}

static BOOL xf_floatbar_button_onclick_restore(xfFloatbar* floatbar)
{
	if (!floatbar)
		return FALSE;

	xf_toggle_fullscreen(floatbar->xfc);
	return TRUE;
}

static BOOL xf_floatbar_button_onclick_locked(xfFloatbar* floatbar)
{
	if (!floatbar)
		return FALSE;

	floatbar->locked = (floatbar->locked) ? FALSE : TRUE;
	return xf_floatbar_hide_and_show(floatbar);
}

BOOL xf_floatbar_set_root_y(xfFloatbar* floatbar, int y)
{
	if (!floatbar)
		return FALSE;

	floatbar->last_motion_y_root = y;
	return TRUE;
}

BOOL xf_floatbar_hide_and_show(xfFloatbar* floatbar)
{
	xfContext* xfc;

	if (!floatbar || !floatbar->xfc)
		return FALSE;

	if (!floatbar->created)
		return TRUE;

	xfc = floatbar->xfc;

	if (!floatbar->locked)
	{
		if ((floatbar->mode == XF_FLOATBAR_MODE_NONE) && (floatbar->last_motion_y_root > 10) &&
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

	return TRUE;
}

static BOOL create_floatbar(xfFloatbar* floatbar)
{
	xfContext* xfc;
	Status status;
	XWindowAttributes attr;

	if (floatbar->created)
		return TRUE;

	xfc = floatbar->xfc;
	status = XGetWindowAttributes(xfc->display, floatbar->root_window, &attr);
	if (status == 0)
	{
		WLog_WARN(TAG, "XGetWindowAttributes failed");
		return FALSE;
	}
	floatbar->x = attr.x + attr.width / 2 - FLOATBAR_DEFAULT_WIDTH / 2;
	floatbar->y = 0;

	if (((floatbar->flags & 0x0004) == 0) && !floatbar->locked)
		floatbar->y = -FLOATBAR_HEIGHT + 1;

	floatbar->handle =
	    XCreateWindow(xfc->display, floatbar->root_window, floatbar->x, 0, FLOATBAR_DEFAULT_WIDTH,
	                  FLOATBAR_HEIGHT, 0, CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
	floatbar->width = FLOATBAR_DEFAULT_WIDTH;
	floatbar->height = FLOATBAR_HEIGHT;
	floatbar->mode = XF_FLOATBAR_MODE_NONE;
	floatbar->buttons[0] = xf_floatbar_new_button(floatbar, XF_FLOATBAR_BUTTON_CLOSE);
	floatbar->buttons[1] = xf_floatbar_new_button(floatbar, XF_FLOATBAR_BUTTON_RESTORE);
	floatbar->buttons[2] = xf_floatbar_new_button(floatbar, XF_FLOATBAR_BUTTON_MINIMIZE);
	floatbar->buttons[3] = xf_floatbar_new_button(floatbar, XF_FLOATBAR_BUTTON_LOCKED);
	XSelectInput(xfc->display, floatbar->handle,
	             ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
	                 FocusChangeMask | LeaveWindowMask | EnterWindowMask | StructureNotifyMask |
	                 PropertyChangeMask);
	floatbar->created = TRUE;
	return TRUE;
}

BOOL xf_floatbar_toggle_fullscreen(xfFloatbar* floatbar, bool fullscreen)
{
	int i, size;
	bool visible = False;
	xfContext* xfc;

	if (!floatbar || !floatbar->xfc)
		return FALSE;

	xfc = floatbar->xfc;

	/* Only visible if enabled */
	if (floatbar->flags & 0x0001)
	{
		/* Visible if fullscreen and flag visible in fullscreen mode */
		visible |= ((floatbar->flags & 0x0010) != 0) && fullscreen;
		/* Visible if window and flag visible in window mode */
		visible |= ((floatbar->flags & 0x0020) != 0) && !fullscreen;
	}

	if (visible)
	{
		if (!create_floatbar(floatbar))
			return FALSE;

		XMapWindow(xfc->display, floatbar->handle);
		size = ARRAYSIZE(floatbar->buttons);

		for (i = 0; i < size; i++)
		{
			XMapWindow(xfc->display, floatbar->buttons[i]->handle);
		}

		/* If default is hidden (and not sticky) don't show on fullscreen state changes */
		if (((floatbar->flags & 0x0004) == 0) && !floatbar->locked)
			floatbar->y = -FLOATBAR_HEIGHT + 1;

		xf_floatbar_hide_and_show(floatbar);
	}
	else if (floatbar->created)
	{
		XUnmapSubwindows(xfc->display, floatbar->handle);
		XUnmapWindow(xfc->display, floatbar->handle);
	}

	return TRUE;
}

xfFloatbarButton* xf_floatbar_new_button(xfFloatbar* floatbar, int type)
{
	xfFloatbarButton* button;
	button = (xfFloatbarButton*)calloc(1, sizeof(xfFloatbarButton));
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
	button->handle = XCreateWindow(floatbar->xfc->display, floatbar->handle, button->x, 0,
	                               FLOATBAR_BUTTON_WIDTH, FLOATBAR_BUTTON_WIDTH, 0, CopyFromParent,
	                               InputOutput, CopyFromParent, 0, NULL);
	XSelectInput(floatbar->xfc->display, button->handle,
	             ExposureMask | ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
	                 LeaveWindowMask | EnterWindowMask | StructureNotifyMask);
	return button;
}

xfFloatbar* xf_floatbar_new(xfContext* xfc, Window window, const char* name, DWORD flags)
{
	xfFloatbar* floatbar;

	/* Floatbar not enabled */
	if ((flags & 0x0001) == 0)
		return NULL;

	if (!xfc)
		return NULL;

	/* Force disable with remote app */
	if (xfc->remote_app)
		return NULL;

	floatbar = (xfFloatbar*)calloc(1, sizeof(xfFloatbar));

	if (!floatbar)
		return NULL;

	floatbar->title = _strdup(name);

	if (!floatbar->title)
		goto fail;

	floatbar->root_window = window;
	floatbar->flags = flags;
	floatbar->xfc = xfc;
	floatbar->locked = flags & 0x0002;
	xf_floatbar_toggle_fullscreen(floatbar, FALSE);
	return floatbar;
fail:
	xf_floatbar_free(floatbar);
	return NULL;
}

static unsigned long xf_floatbar_get_color(xfFloatbar* floatbar, char* rgb_value)
{
	Colormap cmap;
	XColor color;
	Display* display = floatbar->xfc->display;
	cmap = DefaultColormap(display, XDefaultScreen(display));
	XParseColor(display, cmap, rgb_value, &color);
	XAllocColor(display, cmap, &color);
	return color.pixel;
}

static void xf_floatbar_event_expose(xfFloatbar* floatbar)
{
	GC gc, shape_gc;
	Pixmap pmap;
	XPoint shape[5], border[5];
	int len;
	Display* display = floatbar->xfc->display;

	/* create the pixmap that we'll use for shaping the window */
	pmap = XCreatePixmap(display, floatbar->handle, floatbar->width, floatbar->height, 1);
	gc = XCreateGC(display, floatbar->handle, 0, 0);
	shape_gc = XCreateGC(display, pmap, 0, 0);
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
	XSetForeground(display, shape_gc, 0);
	XFillRectangle(display, pmap, shape_gc, 0, 0, floatbar->width, floatbar->height);
	/* Fill all pixels which should be shown with 1 */
	XSetForeground(display, shape_gc, 1);
	XFillPolygon(display, pmap, shape_gc, shape, 5, 0, CoordModeOrigin);
	XShapeCombineMask(display, floatbar->handle, ShapeBounding, 0, 0, pmap, ShapeSet);
	/* draw the float bar */
	XSetForeground(display, gc, xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_BACKGROUND));
	XFillPolygon(display, floatbar->handle, gc, shape, 4, 0, CoordModeOrigin);
	/* draw an border for the floatbar */
	XSetForeground(display, gc, xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_BORDER));
	XDrawLines(display, floatbar->handle, gc, border, 5, CoordModeOrigin);
	/* draw the host name connected to (limit to maximum file name) */
	len = strnlen(floatbar->title, MAX_PATH);
	XSetForeground(display, gc, xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_FOREGROUND));
	XDrawString(display, floatbar->handle, gc, floatbar->width / 2 - len * 2, 15, floatbar->title,
	            len);
	XFreeGC(display, gc);
	XFreeGC(display, shape_gc);
}

static xfFloatbarButton* xf_floatbar_get_button(xfFloatbar* floatbar, Window window)
{
	int i, size;
	size = ARRAYSIZE(floatbar->buttons);

	for (i = 0; i < size; i++)
	{
		if (floatbar->buttons[i]->handle == window)
		{
			return floatbar->buttons[i];
		}
	}

	return NULL;
}

static void xf_floatbar_button_update_positon(xfFloatbar* floatbar)
{
	xfFloatbarButton* button;
	int i, size;
	xfContext* xfc = floatbar->xfc;
	size = ARRAYSIZE(floatbar->buttons);

	for (i = 0; i < size; i++)
	{
		button = floatbar->buttons[i];

		switch (button->type)
		{
			case XF_FLOATBAR_BUTTON_CLOSE:
				button->x =
				    floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			case XF_FLOATBAR_BUTTON_RESTORE:
				button->x =
				    floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			case XF_FLOATBAR_BUTTON_MINIMIZE:
				button->x =
				    floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			default:
				break;
		}

		XMoveWindow(xfc->display, button->handle, button->x, button->y);
		xf_floatbar_event_expose(floatbar);
	}
}

static void xf_floatbar_button_event_expose(xfFloatbar* floatbar, Window window)
{
	xfFloatbarButton* button = xf_floatbar_get_button(floatbar, window);
	static unsigned char* bits;
	GC gc;
	Pixmap pattern;
	xfContext* xfc = floatbar->xfc;

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
		XSetForeground(xfc->display, gc,
		               xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_BACKGROUND));
	else
		XSetForeground(xfc->display, gc, xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_BORDER));

	XSetBackground(xfc->display, gc, xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_FOREGROUND));
	XCopyPlane(xfc->display, pattern, button->handle, gc, 0, 0, FLOATBAR_BUTTON_WIDTH,
	           FLOATBAR_BUTTON_WIDTH, 0, 0, 1);
	XFreePixmap(xfc->display, pattern);
	XFreeGC(xfc->display, gc);
}

static void xf_floatbar_button_event_buttonpress(xfFloatbar* floatbar, const XButtonEvent* event)
{
	xfFloatbarButton* button = xf_floatbar_get_button(floatbar, event->window);

	if (button)
		button->clicked = TRUE;
}

static void xf_floatbar_button_event_buttonrelease(xfFloatbar* floatbar, const XButtonEvent* event)
{
	xfFloatbarButton* button;
	button = xf_floatbar_get_button(floatbar, event->window);

	if (button)
	{
		if (button->clicked)
			button->onclick(floatbar);
		button->clicked = FALSE;
	}
}

static void xf_floatbar_event_buttonpress(xfFloatbar* floatbar, const XButtonEvent* event)
{
	switch (event->button)
	{
		case Button1:
			if (event->x <= FLOATBAR_BORDER)
				floatbar->mode = XF_FLOATBAR_MODE_RESIZE_LEFT;
			else if (event->x >= (floatbar->width - FLOATBAR_BORDER))
				floatbar->mode = XF_FLOATBAR_MODE_RESIZE_RIGHT;
			else
				floatbar->mode = XF_FLOATBAR_MODE_DRAGGING;

			break;

		default:
			break;
	}
}

static void xf_floatbar_event_buttonrelease(xfFloatbar* floatbar, const XButtonEvent* event)
{
	switch (event->button)
	{
		case Button1:
			floatbar->mode = XF_FLOATBAR_MODE_NONE;
			break;

		default:
			break;
	}
}

static void xf_floatbar_resize(xfFloatbar* floatbar, const XMotionEvent* event)
{
	int x, width, movement;
	xfContext* xfc = floatbar->xfc;
	/* calculate movement which happened on the root window */
	movement = event->x_root - floatbar->last_motion_x_root;

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

static void xf_floatbar_dragging(xfFloatbar* floatbar, const XMotionEvent* event)
{
	int x, movement;
	xfContext* xfc = floatbar->xfc;
	/* calculate movement and new x position */
	movement = event->x_root - floatbar->last_motion_x_root;
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

static void xf_floatbar_event_motionnotify(xfFloatbar* floatbar, const XMotionEvent* event)
{
	int mode;
	Cursor cursor;
	xfContext* xfc = floatbar->xfc;
	mode = floatbar->mode;
	cursor = XCreateFontCursor(xfc->display, XC_arrow);

	if ((event->state & Button1Mask) && (mode > XF_FLOATBAR_MODE_DRAGGING))
	{
		xf_floatbar_resize(floatbar, event);
	}
	else if ((event->state & Button1Mask) && (mode == XF_FLOATBAR_MODE_DRAGGING))
	{
		xf_floatbar_dragging(floatbar, event);
	}
	else
	{
		if (event->x <= FLOATBAR_BORDER || event->x >= floatbar->width - FLOATBAR_BORDER)
			cursor = XCreateFontCursor(xfc->display, XC_sb_h_double_arrow);
	}

	XDefineCursor(xfc->display, xfc->window->handle, cursor);
	XFreeCursor(xfc->display, cursor);
	floatbar->last_motion_x_root = event->x_root;
}

static void xf_floatbar_button_event_focusin(xfFloatbar* floatbar, const XAnyEvent* event)
{
	xfFloatbarButton* button;
	button = xf_floatbar_get_button(floatbar, event->window);

	if (button)
	{
		button->focus = TRUE;
		xf_floatbar_button_event_expose(floatbar, event->window);
	}
}

static void xf_floatbar_button_event_focusout(xfFloatbar* floatbar, const XAnyEvent* event)
{
	xfFloatbarButton* button;
	button = xf_floatbar_get_button(floatbar, event->window);

	if (button)
	{
		button->focus = FALSE;
		xf_floatbar_button_event_expose(floatbar, event->window);
	}
}

static void xf_floatbar_event_focusout(xfFloatbar* floatbar)
{
	xfContext* xfc = floatbar->xfc;

	if (xfc->pointer)
	{
		XDefineCursor(xfc->display, xfc->window->handle, xfc->pointer->cursor);
	}
}

BOOL xf_floatbar_check_event(xfFloatbar* floatbar, const XEvent* event)
{
	xfFloatbarButton* button;
	size_t i, size;

	if (!floatbar || !floatbar->xfc || !event)
		return FALSE;

	if (!floatbar->created)
		return FALSE;

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

BOOL xf_floatbar_event_process(xfFloatbar* floatbar, const XEvent* event)
{
	if (!floatbar || !floatbar->xfc || !event)
		return FALSE;

	if (!floatbar->created)
		return FALSE;

	switch (event->type)
	{
		case Expose:
			if (event->xexpose.window == floatbar->handle)
				xf_floatbar_event_expose(floatbar);
			else
				xf_floatbar_button_event_expose(floatbar, event->xexpose.window);

			break;

		case MotionNotify:
			xf_floatbar_event_motionnotify(floatbar, &event->xmotion);
			break;

		case ButtonPress:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_buttonpress(floatbar, &event->xbutton);
			else
				xf_floatbar_button_event_buttonpress(floatbar, &event->xbutton);

			break;

		case ButtonRelease:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_buttonrelease(floatbar, &event->xbutton);
			else
				xf_floatbar_button_event_buttonrelease(floatbar, &event->xbutton);

			break;

		case EnterNotify:
		case FocusIn:
			if (event->xany.window != floatbar->handle)
				xf_floatbar_button_event_focusin(floatbar, &event->xany);

			break;

		case LeaveNotify:
		case FocusOut:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_focusout(floatbar);
			else
				xf_floatbar_button_event_focusout(floatbar, &event->xany);

			break;

		case ConfigureNotify:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_button_update_positon(floatbar);

			break;

		case PropertyNotify:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_button_update_positon(floatbar);

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

void xf_floatbar_free(xfFloatbar* floatbar)
{
	size_t i, size;
	xfContext* xfc;

	if (!floatbar)
		return;

	free(floatbar->title);
	xfc = floatbar->xfc;
	size = ARRAYSIZE(floatbar->buttons);

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
