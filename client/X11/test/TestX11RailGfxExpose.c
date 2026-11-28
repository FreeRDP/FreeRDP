/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include "../xfreerdp.h"
#include "../xf_window.h"

#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exercise the real cached-pixmap repaint with independent X server pixels.
 * No RDP connection, mocked blit or synthesized Expose is involved. */
int main(int argc, char** argv)
{
	if (argc != 2)
		return 2;
	const int legacy = strcmp(argv[1], "legacy") == 0;
	const int partial = strcmp(argv[1], "partial") == 0;
	const int non_software_gdi = strcmp(argv[1], "gfx-nonsw") == 0;
	const int argb = strcmp(argv[1], "gfx-argb") == 0;
	const int no_pixmap = strcmp(argv[1], "gfx-no-pixmap") == 0;
	const int no_gc = strcmp(argv[1], "gfx-no-gc") == 0;
	if (!legacy && !partial && !non_software_gdi && !argb && !no_pixmap && !no_gc &&
	    strcmp(argv[1], "gfx-sw") != 0)
		return 2;
	Display* display = XOpenDisplay(NULL);
	if (!display)
		return 77;
	int rc = 4;
	xfContext context = { 0 };
	xfAppWindow app = { 0 };
	Colormap colormap = None;
	context.display = display;
	context.log = WLog_Get("com.freerdp.client.expose-control");
	context.common.context.settings = freerdp_settings_new(0);
	if (!context.common.context.settings ||
	    !freerdp_settings_set_bool(context.common.context.settings, FreeRDP_SoftwareGdi,
	                               !non_software_gdi))
		goto done;
	const Window root = DefaultRootWindow(display);
	int depth = DefaultDepth(display, DefaultScreen(display));
	Visual* visual = DefaultVisual(display, DefaultScreen(display));
	if (depth != 24 && depth != 32)
	{
		rc = 77;
		goto done;
	}
	if (argb)
	{
		XVisualInfo info = { 0 };
		if (!XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &info))
		{
			rc = 77;
			goto done;
		}
		depth = 32;
		visual = info.visual;
		colormap = XCreateColormap(display, root, visual, AllocNone);
		if (!colormap)
			goto done;
		XSetWindowAttributes attributes = { 0 };
		attributes.colormap = colormap;
		app.handle = XCreateWindow(display, root, 10, 10, 40, 20, 0, depth, InputOutput, visual,
		                           CWColormap | CWBorderPixel | CWBackPixel, &attributes);
	}
	else
		app.handle = XCreateSimpleWindow(display, root, 10, 10, 40, 20, 0, 0, 0);
	if (!app.handle)
		goto done;
	app.pixmap = XCreatePixmap(display, app.handle, 40, 20, (unsigned)depth);
	if (!app.pixmap)
		goto done;
	app.gc = XCreateGC(display, app.pixmap, 0, NULL);
	app.width = 40;
	app.height = 20;
	app.surfaceId = legacy ? UINT16_MAX : 1;
	if (!app.gc)
		goto done;
	char* primary = calloc(40U * 20U, 4U);
	if (!primary)
		goto done;
	context.image =
	    XCreateImage(display, visual, (unsigned)depth, ZPixmap, 0, primary, 40, 20, 32, 0);
	if (!context.image)
	{
		free(primary);
		goto done;
	}
	for (int y = 0; y < 20; y++)
		for (int x = 0; x < 40; x++)
			XPutPixel(context.image, x, y, depth == 32 ? 0xff654321UL : 0x654321UL);
	XSelectInput(display, app.handle, ExposureMask | StructureNotifyMask);
	XSetForeground(display, app.gc, depth == 32 ? 0xff123456UL : 0x123456UL);
	XFillRectangle(display, app.pixmap, app.gc, 0, 0, 40, 20);
	XMapWindow(display, app.handle);
	XSync(display, False);
	unsigned exposes = 0;
	while (XPending(display))
	{
		XEvent event = { 0 };
		XNextEvent(display, &event);
		if (event.type == MapNotify)
			app.is_mapped = TRUE;
		if (event.type == Expose)
			exposes++;
	}
	if (!exposes)
		goto done;
	const Pixmap saved_pixmap = app.pixmap;
	GC saved_gc = app.gc;
	if (no_pixmap)
		app.pixmap = None;
	if (no_gc)
		app.gc = NULL;
	const BOOL repaint = xf_UpdateWindowArea(&context, &app, partial ? 5 : 0, partial ? 6 : 0,
	                                         partial ? 7 : 40, partial ? 8 : 20);
	app.pixmap = saved_pixmap;
	app.gc = saved_gc;
	if (repaint)
		goto done;
	XSync(display, False);
	XImage* actual = XGetImage(display, app.handle, 0, 0, 40, 20, AllPlanes, ZPixmap);
	XImage* cached = XGetImage(display, app.pixmap, 0, 0, 40, 20, AllPlanes, ZPixmap);
	if (!actual || !cached)
	{
		if (actual)
			XDestroyImage(actual);
		if (cached)
			XDestroyImage(cached);
		goto done;
	}
	unsigned mismatches = 0, cache_mismatches = 0;
	for (int y = 0; y < 20; y++)
	{
		for (int x = 0; x < 40; x++)
		{
			const int inside =
			    !no_pixmap && !no_gc && (!partial || (x >= 5 && x < 12 && y >= 6 && y < 14));
			const unsigned long mask = depth == 32 ? 0xffffffffUL : 0xffffffUL;
			const unsigned long color =
			    (legacy ? 0x654321UL : 0x123456UL) | (depth == 32 ? 0xff000000UL : 0);
			const unsigned long wanted = inside ? color : 0;
			mismatches += (XGetPixel(actual, x, y) & mask) != wanted;
			cache_mismatches += (XGetPixel(cached, x, y) & mask) != color;
		}
	}
	printf("{\"mode\":\"%s\",\"actual_exposes\":%u,\"visible_mismatches\":%u,"
	       "\"cache_mismatches\":%u}\n",
	       argv[1], exposes, mismatches, cache_mismatches);
	XDestroyImage(actual);
	XDestroyImage(cached);
	rc = mismatches || cache_mismatches ? 1 : 0;
done:
	if (context.image)
		XDestroyImage(context.image);
	if (app.gc)
		XFreeGC(display, app.gc);
	if (app.pixmap)
		XFreePixmap(display, app.pixmap);
	if (app.handle)
		XDestroyWindow(display, app.handle);
	if (colormap)
		XFreeColormap(display, colormap);
	freerdp_settings_free(context.common.context.settings);
	XCloseDisplay(display);
	return rc;
}
