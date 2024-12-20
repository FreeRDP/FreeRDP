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

#include <rdtk/config.h>

#include <stdint.h>

#include <winpr/wlog.h>
#include <winpr/assert.h>
#include <winpr/cast.h>
#include <rdtk/rdtk.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define TAG "rdtk.sample"

int main(int argc, char** argv)
{
	int rc = 1;
	int pf_count = 0;
	XEvent event;
	XImage* image = NULL;
	Pixmap pixmap = 0;
	Window window = 0;
	rdtkSurface* surface = NULL;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	Display* display = XOpenDisplay(NULL);

	if (!display)
	{
		WLog_ERR(TAG, "Cannot open display");
		return 1;
	}

	const INT32 x = 10;
	const INT32 y = 10;
	const UINT32 width = 640;
	const UINT32 height = 480;

	const int screen_number = DefaultScreen(display);
	const Screen* screen = ScreenOfDisplay(display, screen_number);
	Visual* visual = DefaultVisual(display, screen_number);
	const GC gc = DefaultGC(display, screen_number);
	const int depth = DefaultDepthOfScreen(screen);
	const Window root_window = RootWindow(display, screen_number);
	const unsigned long border = BlackPixel(display, screen_number);
	const unsigned long background = WhitePixel(display, screen_number);

	int scanline_pad = 0;

	XPixmapFormatValues* pfs = XListPixmapFormats(display, &pf_count);

	for (int index = 0; index < pf_count; index++)
	{
		XPixmapFormatValues* pf = &pfs[index];

		if (pf->depth == depth)
		{
			scanline_pad = pf->scanline_pad;
			break;
		}
	}

	XFree(pfs);

	rdtkEngine* engine = rdtk_engine_new();
	const size_t scanline = width * 4ULL;
	uint8_t* buffer = (uint8_t*)calloc(height, scanline);
	if (!engine || !buffer || (depth < 0))
		goto fail;

	surface = rdtk_surface_new(engine, buffer, width, height, scanline);

	rdtk_surface_fill(surface, 0, 0, width, height, 0x3BB9FF);
	rdtk_label_draw(surface, 16, 16, 128, 32, NULL, "label", 0, 0);
	rdtk_button_draw(surface, 16, 64, 128, 32, NULL, "button");
	rdtk_text_field_draw(surface, 16, 128, 128, 32, NULL, "text field");

	window = XCreateSimpleWindow(display, root_window, x, y, width, height, 1, border, background);

	XSelectInput(display, window, ExposureMask | KeyPressMask);
	XMapWindow(display, window);

	XSetFunction(display, gc, GXcopy);
	XSetFillStyle(display, gc, FillSolid);

	pixmap = XCreatePixmap(display, window, width, height, (unsigned)depth);

	image = XCreateImage(display, visual, (unsigned)depth, ZPixmap, 0, (char*)buffer, width, height,
	                     scanline_pad, 0);

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

	rc = 0;
fail:
	if (image)
		XDestroyImage(image);
	XCloseDisplay(display);

	rdtk_surface_free(surface);
	free(buffer);

	rdtk_engine_free(engine);

	return rc;
}
