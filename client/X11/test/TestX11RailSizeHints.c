/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include "../xfreerdp.h"
#include "../xf_rail.h"
#include "../xf_window.h"

#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	xfContext xfc = { 0 };
	xfAppWindow window = { 0 };
	int status = 1;
	xfc.display = XOpenDisplay(NULL);
	if (!xfc.display)
		return 77;
	xfc.common.context.settings = freerdp_settings_new(0);
	if (!xfc.common.context.settings)
		goto out;
	rdpSettings* settings = xfc.common.context.settings;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1280) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 800))
		goto out;
	window.handle =
	    XCreateSimpleWindow(xfc.display, DefaultRootWindow(xfc.display), 0, 0, 1, 1, 0, 0, 0);
	const struct
	{
		BOOL dynamic;
		int minimumWidth, minimumHeight, maximumWidth, maximumHeight;
		int expectedWidth, expectedHeight;
	} cases[] = {
		{ FALSE, 1, 1, 0, 0, 1280, 800 },           { FALSE, 100, 100, 3000, 2000, 1280, 800 },
		{ FALSE, 100, 100, 900, 600, 900, 600 },    { FALSE, 1400, 900, 3000, 2000, 3000, 2000 },
		{ TRUE, 100, 100, 3000, 2000, 3000, 2000 },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DynamicResolutionUpdate, cases[i].dynamic))
			goto out;
		xf_SetWindowMinMaxInfo(&xfc, &window, 0, 0, 0, 0, cases[i].minimumWidth,
		                       cases[i].minimumHeight, cases[i].maximumWidth,
		                       cases[i].maximumHeight);
		XSizeHints hints = { 0 };
		long supplied = 0;
		if (!XGetWMNormalHints(xfc.display, window.handle, &hints, &supplied) ||
		    !(hints.flags & PMaxSize) || hints.max_width != cases[i].expectedWidth ||
		    hints.max_height != cases[i].expectedHeight ||
		    hints.min_width != cases[i].minimumWidth || hints.min_height != cases[i].minimumHeight)
		{
			fprintf(stderr, "Size hints case %zu failed\n", i);
			goto out;
		}
	}
	/* Reapplying a maximize limit must not reset the server's stricter metadata. */
	if (!freerdp_settings_set_bool(settings, FreeRDP_DynamicResolutionUpdate, FALSE))
		goto out;
	xf_SetWindowMinMaxInfo(&xfc, &window, 0, 0, 0, 0, 100, 120, 900, 600);
	XSizeHints before = { 0 };
	long supplied = 0;
	if (!XGetWMNormalHints(xfc.display, window.handle, &before, &supplied))
		goto out;
	before.flags |= PAspect;
	before.min_aspect.x = 1;
	before.min_aspect.y = 1;
	before.max_aspect.x = 16;
	before.max_aspect.y = 9;
	XSetWMNormalHints(xfc.display, window.handle, &before);
	window.width = 1800;
	window.height = 1200;
	xf_AppWindowReapplySizeHints(&xfc, &window);
	XSizeHints after = { 0 };
	if (!XGetWMNormalHints(xfc.display, window.handle, &after, &supplied) ||
	    after.flags != before.flags || after.min_width != 100 || after.min_height != 120 ||
	    after.max_width != 900 || after.max_height != 600 || after.min_aspect.x != 1 ||
	    after.min_aspect.y != 1 || after.max_aspect.x != 16 || after.max_aspect.y != 9 ||
	    after.width_inc != before.width_inc || after.height_inc != before.height_inc)
		goto out;
	status = 0;
out:
	if (window.handle)
		XDestroyWindow(xfc.display, window.handle);
	freerdp_settings_free(xfc.common.context.settings);
	XCloseDisplay(xfc.display);
	return status;
}
