// TODO: RETURN VALUES
// TODO: CHECK FOR BUILD ERRORS
// TODO: ARE CODING GUIDELINES FULLFILLED
// TODO: ADD COMMENTS TO CODE WHERE IT IS NOT CLEAR WHAT HAPPENS
// TODO: CHECK FOR NECESSARY ERROR HANDLING ON XLIB CALLS
// TODO: CHECK FOR BAD EVENT LOOPS
// TODO: CHECK IF PROCESSOR USAGE IS AS BEFORE 

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

#define TAG CLIENT_TAG("x11")

#define FLOATBAR_HEIGHT				26
#define FLOATBAR_DEFAULT_WIDTH		576
#define FLOATBAR_MIN_WIDTH			200
#define FLOATBAR_BORDER				24
#define FLOATBAR_BUTTON_WIDTH		24

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_X11(...) do { } while (0)
#endif

#include "xf_floatbar.h"
#include "resource/close.xbm"
#include "resource/lock.xbm"
#include "resource/unlock.xbm"
#include "resource/minimize.xbm"
#include "resource/restore.xbm"

void xf_floatbar_button_onClick_close(xfContext* xfc) {
	ExitProcess(EXIT_SUCCESS);
}

void xf_floatbar_button_onClick_minimize(xfContext* xfc) {
	xf_SetWindowMinimized(xfc, xfc->window);
}

void xf_floatbar_button_onClick_restore(xfContext* xfc) {
	xf_toggle_fullscreen(xfc);
}

xfFloatbarButton* xf_floatbar_new_button(xfContext* xfc, xfFloatbar* floatbar, int type) {
	// TODO: Free Part?
	xfFloatbarButton* button;
	button = (xfFloatbarButton*) calloc(1, sizeof(xfFloatbarButton));

	button->type = type;
	
	switch(type) {
		case XF_FLOATBAR_BUTTON_CLOSE:
			button->x = floatbar->width-FLOATBAR_BORDER-FLOATBAR_BUTTON_WIDTH*type;
			button->onClick = xf_floatbar_button_onClick_close;
			break;
		case XF_FLOATBAR_BUTTON_RESTORE:
			button->x = floatbar->width-FLOATBAR_BORDER-FLOATBAR_BUTTON_WIDTH*type;
			button->onClick = xf_floatbar_button_onClick_restore;
			break;
		case XF_FLOATBAR_BUTTON_MINIMIZE:
			button->x = floatbar->width-FLOATBAR_BORDER-FLOATBAR_BUTTON_WIDTH*type;
			button->onClick = xf_floatbar_button_onClick_minimize;
			break;
		case XF_FLOATBAR_BUTTON_LOCK:
			button->x = floatbar->x+FLOATBAR_BORDER;
			break;
		default:
			break;
	}
	
	button->y = 0;
	button->focus = FALSE;
	button->handle = XCreateWindow(xfc->display, floatbar->handle, button->x, 0, FLOATBAR_BUTTON_WIDTH, FLOATBAR_BUTTON_WIDTH, 0, 
										CopyFromParent, InputOutput, CopyFromParent, 0, NULL);

	XSelectInput(xfc->display, button->handle, ExposureMask | ButtonPressMask | ButtonReleaseMask | 
				FocusChangeMask | LeaveWindowMask | EnterWindowMask | StructureNotifyMask);

	XMapWindow(xfc->display, button->handle);

	return button;
}

xfFloatbar* xf_floatbar_new(xfContext* xfc, Window window, int width) {
	xfFloatbar* floatbar;
	floatbar = (xfFloatbar*) calloc(1, sizeof(xfFloatbar));

	floatbar->locked = TRUE;
	floatbar->x = width/2 - FLOATBAR_DEFAULT_WIDTH/2;
	floatbar->y = 0;
	floatbar->handle = XCreateWindow(xfc->display, window, floatbar->x, 0, FLOATBAR_DEFAULT_WIDTH, FLOATBAR_HEIGHT, 0, 
										CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
	floatbar->width = FLOATBAR_DEFAULT_WIDTH;
	floatbar->height = FLOATBAR_HEIGHT;
	floatbar->mode = XF_FLOATBAR_MODE_NONE;
	floatbar->buttons[0] = xf_floatbar_new_button(xfc, floatbar, XF_FLOATBAR_BUTTON_CLOSE);
	floatbar->buttons[1] = xf_floatbar_new_button(xfc, floatbar, XF_FLOATBAR_BUTTON_RESTORE);
	floatbar->buttons[2] = xf_floatbar_new_button(xfc, floatbar, XF_FLOATBAR_BUTTON_MINIMIZE);
	
	XSelectInput(xfc->display, floatbar->handle, ExposureMask | ButtonPressMask | ButtonReleaseMask | 
				PointerMotionMask | FocusChangeMask | LeaveWindowMask | EnterWindowMask | StructureNotifyMask | PropertyChangeMask);
	
	XMapWindow(xfc->display, floatbar->handle);

	return floatbar;
}

void xf_floatbar_event_Expose(xfContext* xfc, XEvent* event) {
	GC gc, shape_gc;
	Colormap cmap;
	Pixmap pmap;
	XColor color;
	XPoint poly[5], poly1[5];
	xfFloatbar* floatbar;

	floatbar = xfc->window->floatbar;

	/* create the pixmap that we'll use for shaping the window */
    pmap = XCreatePixmap(xfc->display, floatbar->handle, floatbar->width, floatbar->height, 1);

	poly[0].x = 0;
	poly[0].y = 0;
	poly[1].x = floatbar->width;
	poly[1].y = 0;
	poly[2].x = poly[1].x-FLOATBAR_BORDER;
	poly[2].y = FLOATBAR_HEIGHT;
	poly[3].x = poly[0].x+FLOATBAR_BORDER;
	poly[3].y = FLOATBAR_HEIGHT;
	poly[4].x = poly[0].x; 
	poly[4].y = poly[0].y;

	poly1[0].x = poly[0].x;
	poly1[0].y = poly[0].y-1;
	poly1[1].x = poly[1].x-1;
	poly1[1].y = poly[1].y-1;
	poly1[2].x = poly[2].x;
	poly1[2].y = poly[2].y-1;
	poly1[3].x = poly[3].x-1;
	poly1[3].y = poly[3].y-1;
	poly1[4].x = poly1[0].x; 
	poly1[4].y = poly1[0].y;

	gc = XCreateGC(xfc->display, floatbar->handle, 0, 0);
	shape_gc = XCreateGC(xfc->display, pmap, 0, 0);
	cmap = DefaultColormap(xfc->display, XDefaultScreen(xfc->display));

	/* Fill all pixels with 0 */
	XSetForeground(xfc->display, shape_gc, 0);
    XFillRectangle(xfc->display, pmap, shape_gc, 0, 0, floatbar->width,
                        floatbar->height);

	/* Fill all pixels which should be shown with 1 */
	XSetForeground(xfc->display, shape_gc, 1);
	XFillPolygon(xfc->display, pmap, shape_gc, poly, 5, 0, CoordModeOrigin);

	XShapeCombineMask(xfc->display, floatbar->handle, ShapeBounding, 0, 0, pmap, ShapeSet);

	XSync(xfc->display, False);

	/* draw the float bar */
	XParseColor(xfc->display, cmap, "RGB:31/6c/a9", &color);
	XAllocColor(xfc->display, cmap, &color);
	XSetForeground(xfc->display, gc, color.pixel);

	XFillPolygon(xfc->display, floatbar->handle, gc, poly, 4, 0, CoordModeOrigin);

	/* draw an border for the floatbar */
	XParseColor(xfc->display, cmap, "RGB:75/9a/c8", &color);
	XAllocColor(xfc->display, cmap, &color);
	XSetForeground(xfc->display, gc, color.pixel);

	XDrawLines(xfc->display, floatbar->handle, gc, poly1, 5, CoordModeOrigin);
	
	/* draw the host name connected to */
	XParseColor(xfc->display, cmap, "RGB:FF/FF/FF", &color);
	XAllocColor(xfc->display, cmap, &color);
	XSetForeground(xfc->display, gc, color.pixel);

	int len = strlen(xfc->context.settings->ServerHostname);
	XDrawString(xfc->display, floatbar->handle, gc, floatbar->width/2-len*2, 15,
                xfc->context.settings->ServerHostname, len);

	XFlush(xfc->display);
}

xfFloatbarButton* xf_floatbar_button_getButton(xfContext* xfc, XEvent* event) {
	xfFloatbar* floatbar;
	xfFloatbarButton* button;
	int i, size;
	
	size = sizeof(floatbar->buttons) / sizeof(floatbar->buttons[0]);
	floatbar = xfc->window->floatbar;

	for(i=0; i<size; i++)
	{
		if (floatbar->buttons[i]->handle == event->xany.window) {
			return floatbar->buttons[i];
		}
	}

	return NULL;
}

void xf_floatbar_button_updatePositon(xfContext* xfc, XEvent* event) {
	xfFloatbar* floatbar;
	xfFloatbarButton* button;
	int i, size;
	
	floatbar = xfc->window->floatbar;
	size = sizeof(floatbar->buttons) / sizeof(floatbar->buttons[0]);

	for(i=0; i<size; i++)
	{
		button = floatbar->buttons[i];

		switch(button->type) {
			case XF_FLOATBAR_BUTTON_CLOSE:
				button->x = floatbar->width-FLOATBAR_BORDER-FLOATBAR_BUTTON_WIDTH*button->type;
				break;
			case XF_FLOATBAR_BUTTON_RESTORE:
				button->x = floatbar->width-FLOATBAR_BORDER-FLOATBAR_BUTTON_WIDTH*button->type;
				break;
			case XF_FLOATBAR_BUTTON_MINIMIZE:
				button->x = floatbar->width-FLOATBAR_BORDER-FLOATBAR_BUTTON_WIDTH*button->type;
				break;
			case XF_FLOATBAR_BUTTON_LOCK:
				button->x = floatbar->x+FLOATBAR_BORDER;
				break;
			default:
				break;
		}

		XMoveWindow(xfc->display, button->handle, button->x , button->y);
		
		xf_floatbar_event_Expose(xfc, event);
	}
}

void xf_floatbar_button_event_Expose(xfContext* xfc, XEvent* event) {
	xfFloatbarButton* button;
	static unsigned char* bits;
	GC gc;
	Pixmap pattern;
	Colormap cmap;
	XColor color;

	button = xf_floatbar_button_getButton(xfc, event);
	gc = XCreateGC(xfc->display, button->handle, 0, 0);
	cmap = DefaultColormap(xfc->display, XDefaultScreen(xfc->display));

	switch(button->type) {
		case XF_FLOATBAR_BUTTON_CLOSE:
			bits = &close_bits;
			break;
		case XF_FLOATBAR_BUTTON_RESTORE:
			bits = &restore_bits;
			break;
		case XF_FLOATBAR_BUTTON_MINIMIZE:
			bits = &minimize_bits;	
			break;
		case XF_FLOATBAR_BUTTON_LOCK:
			// TODO: ACTIVE/INACTIVE
			bits = &lock_bits;	
			break;
		default:
			break;
	}

	pattern = XCreateBitmapFromData(xfc->display, button->handle, bits, FLOATBAR_BUTTON_WIDTH, FLOATBAR_BUTTON_WIDTH);

	if(!(button->focus)) {
		XSetForeground(xfc->display, gc, WhitePixel(xfc->display, DefaultScreen(xfc->display)));		
		XParseColor(xfc->display, cmap, "RGB:31/6c/a9", &color);
		XAllocColor(xfc->display, cmap, &color);
		XSetForeground(xfc->display, gc, color.pixel);
	} else {
		XParseColor(xfc->display, cmap, "RGB:75/9a/c8", &color);
		XAllocColor(xfc->display, cmap, &color);
		XSetForeground(xfc->display, gc, color.pixel);
	}

	XParseColor(xfc->display, cmap, "RGB:FF/FF/FF", &color);
	XAllocColor(xfc->display, cmap, &color);
	XSetBackground(xfc->display, gc, color.pixel);
	
	XCopyPlane(xfc->display, pattern, button->handle, gc, 0, 0, FLOATBAR_BUTTON_WIDTH, FLOATBAR_BUTTON_WIDTH, 0, 0, 1);

	XSync(xfc->display, False);
}

void xf_floatbar_button_event_ButtonPress(xfContext* xfc, XEvent* event) {
	xfFloatbarButton* button;

	button = xf_floatbar_button_getButton(xfc, event);
	
	button->clicked = TRUE;
}

void xf_floatbar_button_event_ButtonRelease(xfContext* xfc, XEvent* event) {
	xfFloatbarButton* button;

	button = xf_floatbar_button_getButton(xfc, event);
	if(button->clicked)
		button->onClick(xfc);
}

void xf_floatbar_event_ButtonPress(xfContext* xfc, XEvent* event) {
	xfFloatbar* floatbar;
	floatbar = xfc->window->floatbar;

	switch (event->xbutton.button) {
		case Button1:
			if(event->xmotion.x <= FLOATBAR_BORDER)
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

void xf_floatbar_event_ButtonRelease(xfContext* xfc, XEvent* event) {
	switch (event->xbutton.button) {
		case Button1:
			xfc->window->floatbar->mode = XF_FLOATBAR_MODE_NONE;
			break;
		default:
			break;
	}
}

void xf_floatbar_resize(xfContext* xfc, XEvent* event) {
	xfFloatbar* floatbar;
	floatbar = xfc->window->floatbar;
	int x, width, movement;

	/* calculate movement which happened on the root window */
	movement = event->xmotion.x_root - floatbar->last_motion_x_root;

	/* set x and width depending if movement happens on the left or right  */
	if(floatbar->mode == XF_FLOATBAR_MODE_RESIZE_LEFT) {
		x = floatbar->x + movement;
		width = floatbar->width + movement * -1;
	} else {
		x = floatbar->x;
		width = floatbar->width + movement;
	}
	
	/* only resize and move window if still above minimum width */ 
	if (FLOATBAR_MIN_WIDTH < width) {
		XMoveResizeWindow(xfc->display, floatbar->handle, x, 0, width, floatbar->height);
		floatbar->x = x;
		floatbar->width = width;
	}
}

void xf_floatbar_dragging(xfContext* xfc, XEvent* event) {
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

void xf_floatbar_event_MotionNotify(xfContext* xfc, XEvent* event) {	
	int mode; 
	xfFloatbar* floatbar;

	mode = xfc->window->floatbar->mode;
	floatbar = xfc->window->floatbar;

	if (event->xmotion.state && Button1Mask && mode > XF_FLOATBAR_MODE_DRAGGING)  {
			xf_floatbar_resize(xfc, event);
	} else if(event->xmotion.state && Button1Mask && mode == XF_FLOATBAR_MODE_DRAGGING) {
			xf_floatbar_dragging(xfc, event);
    } else {
		if(event->xmotion.x <= FLOATBAR_BORDER || event->xmotion.x >= xfc->window->floatbar->width - FLOATBAR_BORDER) {
			Cursor cursor;
			cursor = XCreateFontCursor(xfc->display, XC_sb_h_double_arrow);
			XDefineCursor(xfc->display, xfc->window->handle, cursor);
			XFreeCursor(xfc->display, cursor);
		} else {
			Cursor cursor;
	
			cursor = XCreateFontCursor(xfc->display, XC_arrow);
			XDefineCursor(xfc->display, xfc->window->handle, cursor);
			XFreeCursor(xfc->display, cursor);		
		}
	}
	
	xfc->window->floatbar->last_motion_x_root = event->xmotion.x_root;
}

void xf_floatbar_button_event_FocusIn(xfContext* xfc, XEvent* event) {
	xfFloatbarButton* button;
	
	button = xf_floatbar_button_getButton(xfc, event);
	button->focus = TRUE;

	xf_floatbar_button_event_Expose(xfc, event);
}

void xf_floatbar_button_event_FocusOut(xfContext* xfc, XEvent* event) {
	xfFloatbarButton* button;
	
	button = xf_floatbar_button_getButton(xfc, event);
	button->focus = FALSE;
	button->clicked = FALSE;

	xf_floatbar_button_event_Expose(xfc, event);
}

void xf_floatbar_event_FocusOut(xfContext* xfc, XEvent* event) {
	Cursor cursor;
	
	cursor = XCreateFontCursor(xfc->display, XC_arrow);
	XDefineCursor(xfc->display, xfc->window->handle, cursor);
	XFreeCursor(xfc->display, cursor);
}

void xf_floatbar_event_process(xfContext* xfc, XEvent* event) {
	xfFloatbar* floatbar;

	floatbar = xfc->window->floatbar;

	switch (event->type)
	{
		case Expose:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_Expose(xfc, event);
			else
				xf_floatbar_button_event_Expose(xfc, event);
			break;

		case MotionNotify:
			xf_floatbar_event_MotionNotify(xfc, event);
			break;

		case ButtonPress:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_ButtonPress(xfc, event);
			else
				xf_floatbar_button_event_ButtonPress(xfc, event);
			break;

		case ButtonRelease:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_ButtonRelease(xfc, event);
			else
				xf_floatbar_button_event_ButtonRelease(xfc, event);
			break;

		case EnterNotify:
		case FocusIn:
			if (event->xany.window != floatbar->handle)
				xf_floatbar_button_event_FocusIn(xfc, event);
			break;

		case LeaveNotify:
		case FocusOut:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_FocusOut(xfc, event);
			else
				xf_floatbar_button_event_FocusOut(xfc, event);
			break;

		case ConfigureNotify:
			if(event->xany.window == floatbar->handle)
				xf_floatbar_button_updatePositon(xfc, event);
			break;

		case PropertyNotify:
			if(event->xany.window == floatbar->handle)
				xf_floatbar_button_updatePositon(xfc, event);
			break;

		default:
			break;
	}
}

void xf_floatbar_free(xfFloatbar* floatbar) {
	// TODO: how to free it?
	// TODO: did i miss any frees?
}