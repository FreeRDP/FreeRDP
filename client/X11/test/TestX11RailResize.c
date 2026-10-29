/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <freerdp/config.h>
#include <stdio.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/shape.h>
#include <freerdp/window.h>
#include <winpr/collections.h>
#include "../xfreerdp.h"
#include "../xf_event.h"
#include "../xf_rail.h"
#include "../xf_window.h"

static unsigned moves;
static unsigned releases;
static INT32 releaseX;
static INT32 releaseY;
static RAIL_WINDOW_MOVE_ORDER lastMove;

#define CHECK(condition)                                                          \
	do                                                                            \
	{                                                                             \
		if (!(condition))                                                         \
		{                                                                         \
			(void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			goto out;                                                             \
		}                                                                         \
	} while (0)

BOOL freerdp_client_send_button_event(rdpClientContext* context, BOOL relative, UINT16 flags,
                                      INT32 x, INT32 y)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(relative);
	if (flags == PTR_FLAGS_BUTTON1)
	{
		releases++;
		releaseX = x;
		releaseY = y;
	}
	return TRUE;
}

static UINT sendMove(RailClientContext* rail, const RAIL_WINDOW_MOVE_ORDER* order)
{
	WINPR_UNUSED(rail);
	moves++;
	lastMove = *order;
	return CHANNEL_RC_OK;
}

static BOOL has_shape(Display* display, Window window, int width, int height)
{
	int count = 0;
	int ordering = 0;
	XRectangle* rects = XShapeGetRectangles(display, window, ShapeBounding, &count, &ordering);
	const BOOL matches = rects && (count == 1) && (rects[0].x == 0) && (rects[0].y == 0) &&
	                     (rects[0].width == width) && (rects[0].height == height);
	if (rects)
		XFree(rects);
	return matches;
}

static BOOL resize_window(xfContext* xfc, xfAppWindow* window, Display* injector, int width,
                          int height, BOOL endFirst, BOOL xi2)
{
	const unsigned before = releases;
	const unsigned beforeMoves = moves;
	Display* display = xfc->display;
	XEvent release = WINPR_C_ARRAY_INIT;
	const WINDOW_ORDER_INFO visibility = { .windowId = window->windowId,
		                                   .fieldFlags = WINDOW_ORDER_FIELD_VISIBILITY };
	RECTANGLE_16 visible = { .right = (UINT16)window->width,
		                     .bottom = (UINT16)(window->height - 20) };
	WINDOW_STATE_ORDER state = { .numVisibilityRects = 1, .visibilityRects = &visible };
	const WINDOW_ORDER_INFO geometry = { .windowId = window->windowId,
		                                 .fieldFlags = WINDOW_ORDER_FIELD_WND_SIZE |
		                                               WINDOW_ORDER_FIELD_VISIBILITY };
	const RAIL_LOCALMOVESIZE_ORDER end = { .windowId = (UINT32)window->windowId };

	/* Select a real press before handing the gesture to the WM. StartLocalMoveSize
	 * drops that implicit grab, just as it does with a reparenting WM. */
	XTestFakeMotionEvent(injector, -1, window->width - 10, window->height - 30, CurrentTime);
	XTestFakeButtonEvent(injector, Button1, True, CurrentTime);
	XSync(injector, False);
	XSync(display, False);
	xf_StartLocalMoveSize(xfc, window, NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT, window->width - 10,
	                      window->height - 30);
	/* Later visibility updates must not reinstall the old clipping shape. */
	CHECK(xfc->common.context.update->window->WindowUpdate(&xfc->common.context, &visibility,
	                                                       &state));
	XResizeWindow(display, window->handle, (unsigned)width, (unsigned)height);
	XSync(display, False);
	CHECK(has_shape(display, window->handle, width, height));
	XConfigureEvent configure = {
		.send_event = True, .x = 0, .y = 0, .width = width, .height = height
	};
	CHECK(xf_rail_update_configure_geometry(xfc, window, &configure));
	window->local_move.state = LMS_ACTIVE;

	/* A release in the newly exposed bottom-right region must reach the client,
	 * rather than its frame. XSendEvent would bypass precisely this hit test. */
	XTestFakeMotionEvent(injector, -1, width - 2, height - 2, CurrentTime);
	XTestFakeButtonEvent(injector, Button1, False, CurrentTime);
	XSync(injector, False);
	XSync(display, False);
	CHECK(XCheckTypedWindowEvent(display, window->handle, ButtonRelease, &release));
	if (xi2)
	{
		/* Core duplicates are suppressed when XI2 owns the button path. */
		xfc->xi_event = TRUE;
		CHECK(xf_event_suppress_events(xfc, window, &release));
		CHECK(releases == before);
		CHECK(xf_generic_ButtonEvent(xfc, release.xbutton.x, release.xbutton.y, Button1,
		                             window->handle, TRUE, FALSE));
		xfc->xi_event = FALSE;
	}
	else
		CHECK(xf_event_suppress_events(xfc, window, &release));
	CHECK(window->local_move.state == LMS_TERMINATING);
	CHECK(moves == beforeMoves + 1 && releases == before + 1);
	CHECK(lastMove.right - lastMove.left == width && lastMove.bottom - lastMove.top == height);
	CHECK(releaseX == width - 2 && releaseY == height - 2);
	CHECK(xf_rail_end_local_move(xfc, window));
	CHECK(releases == before + 1 && moves == beforeMoves + 1);
	if (endFirst)
		CHECK(xfc->rail->ServerLocalMoveSize(xfc->rail, &end) == CHANNEL_RC_OK);
	CHECK(has_shape(display, window->handle, width, height));
	/* Accept a server-adjusted size and its clipped visible area.
	 * Both channel orderings must restore the latest shape, not the old one. */
	state.windowWidth = (UINT32)(width - 7);
	state.windowHeight = (UINT32)(height - 9);
	visible.right = (UINT16)state.windowWidth;
	visible.bottom = (UINT16)(state.windowHeight - 20);
	CHECK(
	    xfc->common.context.update->window->WindowUpdate(&xfc->common.context, &geometry, &state));
	if (!endFirst)
	{
		CHECK(has_shape(display, window->handle, (int)state.windowWidth, (int)state.windowHeight));
		CHECK(xfc->rail->ServerLocalMoveSize(xfc->rail, &end) == CHANNEL_RC_OK);
	}
	XSync(display, False);
	CHECK(window->local_move.state == LMS_NOT_ACTIVE);
	CHECK(has_shape(display, window->handle, visible.right, visible.bottom));
	return TRUE;
out:
	xfc->xi_event = FALSE;
	return FALSE;
}

static BOOL cancel_resize(xfContext* xfc, xfAppWindow* window)
{
	const unsigned before = releases;
	const unsigned beforeMoves = moves;
	RECTANGLE_16 visible = { .right = (UINT16)(window->width - 10),
		                     .bottom = (UINT16)(window->height - 30) };
	const WINDOW_ORDER_INFO info = { .windowId = window->windowId,
		                             .fieldFlags = WINDOW_ORDER_FIELD_WND_SIZE |
		                                           WINDOW_ORDER_FIELD_VISIBILITY };
	const WINDOW_STATE_ORDER state = { .windowWidth = visible.right,
		                               .windowHeight = (UINT32)(visible.bottom + 20),
		                               .numVisibilityRects = 1,
		                               .visibilityRects = &visible };
	const RAIL_LOCALMOVESIZE_ORDER end = { .windowId = (UINT32)window->windowId };
	xf_StartLocalMoveSize(xfc, window, NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT, 100, 100);
	CHECK(xfc->common.context.update->window->WindowUpdate(&xfc->common.context, &info, &state));
	CHECK(xfc->rail->ServerLocalMoveSize(xfc->rail, &end) == CHANNEL_RC_OK);
	XSync(xfc->display, False);
	CHECK(window->width == (int)state.windowWidth && window->height == (int)state.windowHeight);
	CHECK(has_shape(xfc->display, window->handle, visible.right, visible.bottom));
	CHECK(window->local_move.state == LMS_NOT_ACTIVE && releases == before && moves == beforeMoves);

	/* An unmap aborts a local gesture without sending a stale completion. */
	xf_StartLocalMoveSize(xfc, window, NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT, 100, 100);
	XUnmapWindow(xfc->display, window->handle);
	XSync(xfc->display, False);
	const XEvent unmap = { .xunmap = { .type = UnmapNotify, .window = window->handle } };
	CHECK(!xf_event_suppress_events(xfc, window, &unmap));
	CHECK(window->local_move.state == LMS_NOT_ACTIVE);
	CHECK(has_shape(xfc->display, window->handle, visible.right, visible.bottom));
	CHECK(releases == before && moves == beforeMoves);
	return TRUE;
out:
	return FALSE;
}

static BOOL fixed_size_window(xfContext* xfc, xfAppWindow* window)
{
	XSizeHints hints = WINPR_C_ARRAY_INIT;
	long supplied = 0;
	XMapWindow(xfc->display, window->handle);
	window->windowStyleKnown = TRUE;
	window->dwStyle = WS_CAPTION | WS_SYSMENU;
	xf_SetWindowMinMaxInfo(xfc, window, 0, 0, 0, 0, 136, 39, 1670, 932);
	CHECK(XGetWMNormalHints(xfc->display, window->handle, &hints, &supplied));
	CHECK(hints.min_width == window->width && hints.max_width == window->width);
	CHECK(hints.min_height == window->height && hints.max_height == window->height);
	/* A programmatic server resize is authoritative even for a fixed dialog. */
	const WINDOW_ORDER_INFO info = { .windowId = window->windowId,
		                             .fieldFlags = WINDOW_ORDER_FIELD_WND_SIZE };
	const WINDOW_STATE_ORDER state = { .windowWidth = 380, .windowHeight = 260 };
	CHECK(xfc->common.context.update->window->WindowUpdate(&xfc->common.context, &info, &state));
	CHECK(XGetWMNormalHints(xfc->display, window->handle, &hints, &supplied));
	CHECK(hints.min_width == 380 && hints.max_width == 380);
	CHECK(hints.min_height == 260 && hints.max_height == 260);
	const unsigned before = moves;
	window->width = 600;
	window->height = 500;
	CHECK(xf_rail_adjust_position(xfc, window));
	CHECK(window->width == 380 && window->height == 260 && moves == before);
	XWindowAttributes attributes = WINPR_C_ARRAY_INIT;
	CHECK(XGetWindowAttributes(xfc->display, window->handle, &attributes));
	CHECK(attributes.width == 380 && attributes.height == 260);
	/* Restoring resize permission restores the last RAIL limits as well. */
	window->dwStyle |= WS_SIZEBOX;
	xf_UpdateWindowSizeHints(xfc, window);
	CHECK(XGetWMNormalHints(xfc->display, window->handle, &hints, &supplied));
	CHECK(hints.min_width == 136 && hints.max_width == 1670);
	CHECK(hints.min_height == 39 && hints.max_height == 932);
	return TRUE;
out:
	return FALSE;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	int result = 1;
	BOOL initialized = FALSE;
	xfContext xfc = WINPR_C_ARRAY_INIT;
	xfAppWindow window = WINPR_C_ARRAY_INIT;
	rdpWindowUpdate windows = WINPR_C_ARRAY_INIT;
	rdpUpdate update = { .window = &windows };
	rdpGdi gdi = { .gfx = (RdpgfxClientContext*)1 };
	RailClientContext rail = { .ClientWindowMove = sendMove };
	Display* display = XOpenDisplay(nullptr);
	Display* injector = XOpenDisplay(nullptr);
	Window frame = None;
	CHECK(display && injector);
	xfc.display = display;
	xfc.screen = DefaultScreenOfDisplay(display);
	xfc.drawable = DefaultRootWindow(display);
	xfc.depth = DefaultDepth(display, DefaultScreen(display));
	xfc.remote_app = TRUE;
	xfc.UseXThreads = TRUE;
	xfc.log = WLog_Get("com.freerdp.client.x11.test");
	xfc.NET_WM_MOVERESIZE = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
	xfc.common.context.update = &update;
	xfc.common.context.gdi = &gdi;
	xfc.common.context.settings = freerdp_settings_new(0);
	CHECK(xfc.common.context.settings);
	CHECK(xf_rail_init(&xfc, &rail));
	initialized = TRUE;
	frame = XCreateSimpleWindow(display, xfc.drawable, 0, 0, 1100, 900, 0, 0, 0);
	window.handle = XCreateSimpleWindow(display, frame, 0, 0, 400, 300, 0, 0, 0);
	window.xfc = &xfc;
	window.windowId = 0x300d0;
	window.surfaceId = UINT32_MAX;
	window.is_mapped = TRUE;
	window.width = window.windowWidth = 400;
	window.height = window.windowHeight = 300;
	window.resizeMarginXKnown = window.resizeMarginYKnown = TRUE;
	CHECK(HashTable_Insert(xfc.railWindows, &window.windowId, &window));
	XSelectInput(display, window.handle, ButtonPressMask | ButtonReleaseMask);
	XMapWindow(display, window.handle);
	XMapWindow(display, frame);
	XSync(display, False);
	RECTANGLE_16 visible = { .right = 400, .bottom = 280 };
	xf_SetWindowVisibilityRects(&xfc, &window, 0, 0, &visible, 1);
	CHECK(has_shape(display, window.handle, 400, 280));
	CHECK(resize_window(&xfc, &window, injector, 800, 600, TRUE, FALSE));
	CHECK(resize_window(&xfc, &window, injector, 500, 400, FALSE, FALSE));
	CHECK(resize_window(&xfc, &window, injector, 900, 650, FALSE, TRUE));
	CHECK(resize_window(&xfc, &window, injector, 600, 450, TRUE, TRUE));
	CHECK(cancel_resize(&xfc, &window));
	CHECK(fixed_size_window(&xfc, &window));
	result = 0;
out:
	if (initialized)
	{
		HashTable_ValueObject(xfc.railWindows)->fnObjectFree = nullptr;
		xf_rail_uninit(&xfc, &rail);
	}
	free(window.visibilityRects);
	if (display)
	{
		if (window.pixmap)
			XFreePixmap(display, window.pixmap);
		if (frame)
			XDestroyWindow(display, frame);
		XCloseDisplay(display);
	}
	if (injector)
		XCloseDisplay(injector);
	freerdp_settings_free(xfc.common.context.settings);
	return result;
}
