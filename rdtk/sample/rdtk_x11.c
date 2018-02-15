/**
 * RdTk: Remote Desktop Toolkit
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/log.h>
#include <rdtk/rdtk.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define TAG "rdtk.sample"

int main(int argc, char** argv)
{
	GC gc;
	int index;
	int depth;
	int x, y;
	int width;
	int height;
	BYTE* buffer;
	int scanline;
	int pf_count;
	XEvent event;
	XImage* image;
	Pixmap pixmap;
	Screen* screen;
	Visual* visual;
	int scanline_pad;
	int screen_number;
	Display* display;
	Window window;
	Window root_window;
	rdtkEngine* engine;
	rdtkSurface* surface;
	unsigned long border;
	unsigned long background;
	XPixmapFormatValues* pf;
	XPixmapFormatValues* pfs;
 
	display = XOpenDisplay(NULL);

	if (!display)
	{
		WLog_ERR(TAG, "Cannot open display");
		return winpr_exit(1);
	}

	x = 10;
	y = 10;
	width = 640;
	height = 480;

	screen_number = DefaultScreen(display);
	screen = ScreenOfDisplay(display, screen_number);
	visual = DefaultVisual(display, screen_number);
	gc = DefaultGC(display, screen_number);
	depth = DefaultDepthOfScreen(screen);
	root_window = RootWindow(display, screen_number);
	border = BlackPixel(display, screen_number);
	background = WhitePixel(display, screen_number);

	scanline_pad = 0;

	pfs = XListPixmapFormats(display, &pf_count);

	for (index = 0; index < pf_count; index++)
	{
		pf = &pfs[index];

		if (pf->depth == depth)
		{
			scanline_pad = pf->scanline_pad;
			break;
		}
	}

	XFree(pfs);

	engine = rdtk_engine_new();
	if (!engine)
		return winpr_exit(1);

	scanline = width * 4;
	buffer = (BYTE*) calloc(height, scanline);
	if (!buffer)
		return winpr_exit(1);

	surface = rdtk_surface_new(engine, buffer, width, height, scanline);

	rdtk_surface_fill(surface, 0, 0, width, height, 0x3BB9FF);
	rdtk_label_draw(surface, 16, 16, 128, 32, NULL, "label", 0, 0);
	rdtk_button_draw(surface, 16, 64, 128, 32, NULL, "button");
	rdtk_text_field_draw(surface, 16, 128, 128, 32, NULL, "text field");

	window = XCreateSimpleWindow(display, root_window,
			x, y, width, height, 1, border, background);

	XSelectInput(display, window, ExposureMask | KeyPressMask);
	XMapWindow(display, window);

	XSetFunction(display, gc, GXcopy);
	XSetFillStyle(display, gc, FillSolid);

	pixmap = XCreatePixmap(display, window, width, height, depth);

	image = XCreateImage(display, visual, depth, ZPixmap, 0,
			(char*) buffer, width, height, scanline_pad, 0);
 
	while (1)
	{
		XNextEvent(display, &event);

		if (event.type == Expose)
		{
			XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, width, height);
			XCopyArea(display, pixmap, window, gc, 0, 0, width, height, 0, 0);
		}

		if (event.type == KeyPress)
			break;

		if (event.type == ClientMessage)
			break;
	}

	XFlush(display);

	XDestroyImage(image);
	XCloseDisplay(display);

	rdtk_surface_free(surface);
	free(buffer);

	rdtk_engine_free(engine);

	return winpr_exit(0);
}
