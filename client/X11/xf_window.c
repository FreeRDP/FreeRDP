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

#define MWM_HINTS_DECORATIONS		(1L << 1)
#define PROP_MOTIF_WM_HINTS_ELEMENTS	5

struct _PropMotifWmHints
{
	uint32 flags;
	uint32 functions;
	uint32 decorations;
	sint32 inputMode;
	uint32 status;
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

void window_show_decorations(xfInfo* xfi, xfWindow* window, boolean show)
{
	Atom atom;
	PropMotifWmHints hints;

	hints.decorations = 0;
	hints.flags = MWM_HINTS_DECORATIONS;

	atom = XInternAtom(xfi->display, "_MOTIF_WM_HINTS", False);

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

	window->decorations = False;
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
			0, 0, xfi->width, xfi->height, 0, xfi->depth, InputOutput, xfi->visual,
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

void window_destroy(xfInfo* xfi, xfWindow* window)
{
	XDestroyWindow(xfi->display, window->handle);
	xfree(window);
}
