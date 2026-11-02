/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <freerdp/config.h>
#include <stdio.h>
#include "../xfreerdp.h"
#include "../xf_rail.h"

#define CHECK(condition)                                                          \
	do                                                                            \
	{                                                                             \
		if (!(condition))                                                         \
		{                                                                         \
			(void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			goto out;                                                             \
		}                                                                         \
	} while (0)

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	int result = 1;
	Display* display = XOpenDisplay(nullptr);
	if (!display)
		return 1;
	xfContext xfc = WINPR_C_ARRAY_INIT;
	xfAppWindow window = WINPR_C_ARRAY_INIT;
	const Window root = DefaultRootWindow(display);
	const Window frame = XCreateSimpleWindow(display, root, 200, 300, 600, 500, 0, 0, 0);
	window.handle = XCreateSimpleWindow(display, frame, 4, 22, 400, 300, 0, 0, 0);
	window.xfc = &xfc;
	window.surfaceId = UINT32_MAX;
	xfc.display = display;
	xfc.drawable = root;
	xfc.depth = DefaultDepth(display, DefaultScreen(display));
	xfc.log = WLog_Get("com.freerdp.client.x11.test");
	XMapWindow(display, window.handle);
	XMapWindow(display, frame);
	XSync(display, False);

	/* This old parent-relative event must not supply dimensions for a newer
	 * queried root position. No synthetic ConfigureNotify is sent by this WM. */
	XConfigureEvent event = { .x = 4, .y = 22, .width = 400, .height = 300 };
	CHECK(!xf_rail_update_configure_geometry(&xfc, &window, &event));
	CHECK(window.configureGeometryPending);
	XMoveWindow(display, frame, 250, 350);
	XResizeWindow(display, window.handle, 420, 330);
	XSync(display, False);
	CHECK(xf_rail_sync_configure_geometry(&xfc, &window));
	CHECK(!window.configureGeometryPending);
	CHECK(window.x == 254 && window.y == 372 && window.width == 420 && window.height == 330);
	CHECK(window.pixmap != 0 && window.pixmapWidth == 420 && window.pixmapHeight == 330);

	/* Zero is a valid current root coordinate, not an absent axis. */
	CHECK(!xf_rail_update_configure_geometry(&xfc, &window, &event));
	XMoveWindow(display, frame, 0, 0);
	XMoveWindow(display, window.handle, 0, 0);
	XSync(display, False);
	CHECK(xf_rail_sync_configure_geometry(&xfc, &window));
	CHECK(window.x == 0 && window.y == 0 && window.width == 420 && window.height == 330);

	/* A queued synthetic event keeps its own tuple instead of mixing in the
	 * now-newer X11 state. It also cancels the pending real-event query. */
	CHECK(!xf_rail_update_configure_geometry(&xfc, &window, &event));
	event.send_event = True;
	event.x = 50;
	event.y = 60;
	event.width = 410;
	event.height = 310;
	CHECK(xf_rail_update_configure_geometry(&xfc, &window, &event));
	CHECK(xf_rail_sync_configure_geometry(&xfc, &window));
	CHECK(window.x == 50 && window.y == 60 && window.width == 410 && window.height == 310);
	result = 0;
out:
	if (window.pixmap)
		XFreePixmap(display, window.pixmap);
	XDestroyWindow(display, frame);
	XCloseDisplay(display);
	return result;
}
