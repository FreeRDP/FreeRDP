/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <freerdp/config.h>
#include <stdio.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/shape.h>
#include <freerdp/window.h>
#include <winpr/collections.h>
#include <winpr/thread.h>
#include <winpr/synch.h>
#include "../xfreerdp.h"
#include "../xf_event.h"
#include "../xf_rail.h"
#include "../xf_window.h"

static unsigned moves;
static unsigned releases;
static unsigned presses;
static INT32 pressX;
static INT32 pressY;
static UINT32 activatedWindow;
static Atom stateAtom;
static Atom maximizeAtom;
static BOOL observeStateRequests;
static BOOL stateLimitsValid = TRUE;
static unsigned stateRequests;
static XClientMessageEvent activationMessage;

Status XSendEvent(Display* display, Window destination, Bool propagate, long mask, XEvent* event)
{
	WINPR_UNUSED(destination);
	WINPR_UNUSED(propagate);
	WINPR_UNUSED(mask);
	if (observeStateRequests && event->type == ClientMessage &&
	    event->xclient.message_type == stateAtom && (Atom)event->xclient.data.l[1] == maximizeAtom)
	{
		XSizeHints hints = WINPR_C_ARRAY_INIT;
		long supplied = 0;
		stateRequests++;
		stateLimitsValid &= XGetWMNormalHints(display, event->xclient.window, &hints, &supplied) &&
		                    hints.min_width == 136 && hints.max_width == 1940 &&
		                    hints.min_height == 39 && hints.max_height == 1220;
	}
	if (event->type == ClientMessage &&
	    event->xclient.message_type == XInternAtom(display, "_NET_ACTIVE_WINDOW", False))
		activationMessage = event->xclient;
	return True;
}

int freerdp_channels_process_pending_messages(freerdp* instance)
{
	WINPR_UNUSED(instance);
	return 1;
}

static UINT sendActivate(RailClientContext* rail, const RAIL_ACTIVATE_ORDER* order)
{
	WINPR_UNUSED(rail);
	activatedWindow = order->windowId;
	return CHANNEL_RC_OK;
}
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
	if (flags == (PTR_FLAGS_BUTTON1 | PTR_FLAGS_DOWN))
	{
		presses++;
		pressX = x;
		pressY = y;
	}
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

static BOOL apply_server_update(xfContext* xfc, xfAppWindow* window, const WINDOW_ORDER_INFO* info,
                                const WINDOW_STATE_ORDER* state)
{
	if (!xfc->common.context.update->window->WindowUpdate(&xfc->common.context, info, state))
		return FALSE;
	/* This fixture has no WM: its direct X requests have completed after XSync. */
	XSync(xfc->display, False);
	return !window->windowGeometryPending || xf_rail_sync_configure_geometry(xfc, window);
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

static BOOL redirected_geometry_requests(xfContext* xfc, xfAppWindow* window, Display* wm,
                                         Window frame)
{
	BOOL result = FALSE;
	Display* display = xfc->display;
	XSelectInput(wm, frame, SubstructureRedirectMask);
	XSync(wm, False);
	window->width = 500;
	window->height = 420;
	XMoveResizeWindow(wm, window->handle, 0, 0, 500, 420);
	XSync(wm, False);
	(void)xf_MoveWindow(xfc, window, 0, 0, 122, 32);
	(void)xf_MoveWindow(xfc, window, 0, 0, 500, 420);
	XSync(display, False);
	unsigned requests = 0;
	XEvent event = WINPR_C_ARRAY_INIT;
	while (XCheckTypedWindowEvent(wm, frame, ConfigureRequest, &event))
	{
		const XConfigureRequestEvent* request = &event.xconfigurerequest;
		XWindowChanges change = { .x = request->x,
			                      .y = request->y,
			                      .width = request->width,
			                      .height = request->height,
			                      .border_width = request->border_width,
			                      .sibling = request->above,
			                      .stack_mode = request->detail };
		XConfigureWindow(wm, request->window, (unsigned)request->value_mask, &change);
		requests++;
	}
	XSync(wm, False);
	XWindowAttributes observed = WINPR_C_ARRAY_INIT;
	CHECK(requests == 2 && XGetWindowAttributes(display, window->handle, &observed));
	CHECK(observed.width == 500 && observed.height == 420);
	/* A no-op query cannot distinguish a delayed WM from an accepted request
	 * equal to the observed size. The owned grace period must expire. */
	CHECK(!xf_rail_sync_configure_geometry(xfc, window));
	Sleep(510);
	CHECK(xf_rail_sync_configure_geometry(xfc, window));
	CHECK(!window->windowGeometryPending && window->width == 500);
	result = TRUE;
out:
	XSelectInput(wm, frame, 0);
	XMoveResizeWindow(wm, window->handle, 0, 0, 400, 300);
	XSync(wm, False);
	window->width = 400;
	window->windowWidth = 400;
	window->height = 300;
	window->windowHeight = 300;
	window->windowGeometryPending = window->configureGeometryPending = window->serverConfigure =
	    FALSE;
	return result;
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
	CHECK(apply_server_update(xfc, window, &visibility, &state));
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
	CHECK(apply_server_update(xfc, window, &geometry, &state));
	if (!endFirst)
	{
		CHECK(has_shape(display, window->handle, (int)state.windowWidth, (int)state.windowHeight));
		CHECK(xfc->rail->ServerLocalMoveSize(xfc->rail, &end) == CHANNEL_RC_OK);
	}
	/* A matching reply may precede the WM's last tile. Visibility clipping is
	 * restored when the bounded owned interval drains, without extra input. */
	for (unsigned i = 0; i < 25 && window->localMoveGeometryTime; i++)
	{
		CHECK(WaitForSingleObject(xf_rail_geometry_event(xfc), 1000) == WAIT_OBJECT_0);
		xf_rail_flush_configure_geometry(xfc);
	}
	CHECK(!window->localMoveGeometryTime);
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
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(xfc->rail->ServerLocalMoveSize(xfc->rail, &end) == CHANNEL_RC_OK);
	XSync(xfc->display, False);
	CHECK(xf_rail_sync_configure_geometry(xfc, window));
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
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(XGetWMNormalHints(xfc->display, window->handle, &hints, &supplied));
	CHECK(hints.min_width == 380 && hints.max_width == 380);
	CHECK(hints.min_height == 260 && hints.max_height == 260);
	const unsigned before = moves;
	window->width = 600;
	window->height = 500;
	window->serverConfigure = FALSE;
	window->lastGeometryRequest = 0;
	CHECK(!xf_rail_adjust_position(xfc, window, nullptr));
	CHECK(window->windowGeometryPending);
	XSync(xfc->display, False);
	CHECK(xf_rail_sync_configure_geometry(xfc, window));
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

static BOOL wm_owned_resize(xfContext* xfc, xfAppWindow* window, Display* wm)
{
	BOOL grabbed = FALSE;
	const unsigned before = releases;
	const unsigned beforeMoves = moves;
	Display* display = xfc->display;
	XEvent event = WINPR_C_ARRAY_INIT;
	XSelectInput(display, window->handle,
	             ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask);
	XMoveWindow(display, window->handle, 0, 0);
	XSync(display, False);
	while (XPending(display))
		XNextEvent(display, &event);
	XTestFakeMotionEvent(wm, -1, 100, 100, CurrentTime);
	XTestFakeButtonEvent(wm, Button1, True, CurrentTime);
	XSync(wm, False);
	xf_StartLocalMoveSize(xfc, window, NET_WM_MOVERESIZE_MOVE, 100, 100);
	CHECK(XGrabPointer(wm, DefaultRootWindow(wm), False, ButtonReleaseMask, GrabModeAsync,
	                   GrabModeAsync, None, None, CurrentTime) == GrabSuccess);
	grabbed = TRUE;
	/* A WM may tile during a mouse move without sending its configure first. */
	XResizeWindow(wm, window->handle, 600, 500);
	XTestFakeMotionEvent(wm, -1, 330, 210, CurrentTime);
	XTestFakeButtonEvent(wm, Button1, False, CurrentTime);
	XSync(wm, False);
	XSync(display, False);
	CHECK(!XCheckTypedWindowEvent(display, window->handle, ButtonRelease, &event));
	XUngrabPointer(wm, CurrentTime);
	grabbed = FALSE;
	XSync(wm, False);
	XSync(display, False);
	BOOL sawUngrab = FALSE;
	freerdp instance = { .context = &xfc->common.context };
	while (XCheckTypedWindowEvent(display, window->handle, EnterNotify, &event))
	{
		if (event.xcrossing.mode == NotifyUngrab)
		{
			sawUngrab = TRUE;
			CHECK(xf_event_process(&instance, &event));
		}
	}
	CHECK(sawUngrab);
	CHECK(releases == before + 1 && releaseX == 330 && releaseY == 210);
	CHECK(window->local_move.state == LMS_TERMINATING && moves == beforeMoves + 1);
	CHECK(lastMove.right - lastMove.left == 600);
	CHECK(lastMove.bottom - lastMove.top == 500);
	CHECK(xf_event_check_local_move(xfc));
	CHECK(releases == before + 1);
	const RAIL_LOCALMOVESIZE_ORDER end = { .windowId = (UINT32)window->windowId };
	CHECK(xfc->rail->ServerLocalMoveSize(xfc->rail, &end) == CHANNEL_RC_OK);
	CHECK(!xfc->railLocalMoveWindowId);
	return TRUE;
out:
	if (grabbed)
		XUngrabPointer(wm, CurrentTime);
	return FALSE;
}

static gdiGfxSurface testSurface;

static void* get_test_surface(RdpgfxClientContext* gfx, UINT16 surfaceId)
{
	WINPR_UNUSED(gfx);
	return (surfaceId == testSurface.surfaceId) ? &testSurface : nullptr;
}

typedef struct
{
	RdpgfxClientContext* gfx;
	HANDLE locked;
	HANDLE release;
} RepaintLock;

static DWORD WINAPI hold_gfx_lock(void* arg)
{
	RepaintLock* lock = arg;
	EnterCriticalSection(&lock->gfx->mux);
	SetEvent(lock->locked);
	WaitForSingleObject(lock->release, INFINITE);
	LeaveCriticalSection(&lock->gfx->mux);
	return 0;
}

static unsigned ownedPixmapErrors;
static unsigned unrelatedErrors;
static Display* activeTrapOtherDisplay;
static BOOL activeTrapForeignRejected;

static int pixmap_error_handler(Display* display, XErrorEvent* error)
{
	if (activeTrapOtherDisplay && display != activeTrapOtherDisplay)
	{
		/* This callback runs before the owned request is removed. Exercise the
		 * registry while active, including an otherwise identical foreign error. */
		XErrorEvent foreign = *error;
		foreign.display = activeTrapOtherDisplay;
		activeTrapForeignRejected = !xf_AppWindowPixmapError(activeTrapOtherDisplay, &foreign);
		foreign = *error;
		foreign.serial++;
		activeTrapForeignRejected &= !xf_AppWindowPixmapError(display, &foreign);
		foreign = *error;
		foreign.request_code++;
		activeTrapForeignRejected &= !xf_AppWindowPixmapError(display, &foreign);
	}
	if (xf_AppWindowPixmapError(display, error))
		ownedPixmapErrors++;
	else
		unrelatedErrors++;
	return 0;
}

static BOOL asynchronous_pixmap_failure(xfContext* xfc, xfAppWindow* window, Display* other)
{
	BOOL result = FALSE;
	const Drawable drawable = xfc->drawable;
	int (*previous)(Display*, XErrorEvent*) = XSetErrorHandler(pixmap_error_handler);
	const Pixmap backing = window->pixmap;
	const UINT32 oldWidth = window->pixmapWidth;
	const UINT32 oldHeight = window->pixmapHeight;
	Window invalid = XCreateSimpleWindow(xfc->display, drawable, 0, 0, 1, 1, 0, 0, 0);
	XDestroyWindow(xfc->display, invalid);
	XSync(xfc->display, False);
	xfc->drawable = invalid;
	activeTrapOtherDisplay = other;
	window->width += 10;
	window->height += 10;
	CHECK(!xf_AppWindowResize(xfc, window));
	CHECK(ownedPixmapErrors == 1 && !unrelatedErrors);
	CHECK(activeTrapForeignRejected);
	activeTrapOtherDisplay = nullptr;
	CHECK(window->pixmap == backing && window->pixmapWidth == oldWidth &&
	      window->pixmapHeight == oldHeight);
	CHECK(!xf_UpdateWindowArea(xfc, window, 0, 0, window->width, window->height));
	/* A different display's error must not be consumed by the allocation trap. */
	(void)XCreatePixmap(other, invalid, 10, 10, (unsigned)xfc->depth);
	XSync(other, False);
	CHECK(unrelatedErrors == 1);
	xfc->drawable = drawable;
	window->backingRetryTime = 0;
	CHECK(xf_AppWindowResize(xfc, window));
	CHECK(window->pixmap != backing && window->pixmapWidth == (UINT32)window->width);
	result = TRUE;
out:
	activeTrapOtherDisplay = nullptr;
	xfc->drawable = drawable;
	XSetErrorHandler(previous);
	return result;
}

static BOOL painted_pixel(xfContext* xfc, xfAppWindow* window, int x, int y)
{
	XSync(xfc->display, False);
	XImage* image = XGetImage(xfc->display, window->handle, x, y, 1, 1, AllPlanes, ZPixmap);
	if (!image)
		return FALSE;
	const BOOL painted = (XGetPixel(image, 0, 0) & 0xFFFFFF) == 0x123456;
	XDestroyImage(image);
	return painted;
}

static BOOL repaint_gfx_window(xfContext* xfc, xfAppWindow* window)
{
	BOOL result = FALSE;
	RdpgfxClientContext gfx = { .GetSurfaceData = get_test_surface };
	rdpGdi* gdi = xfc->common.context.gdi;
	RdpgfxClientContext* previous = gdi->gfx;
	RepaintLock lock = { .gfx = &gfx };
	HANDLE thread = nullptr;
	InitializeCriticalSection(&gfx.mux);
	testSurface = (gdiGfxSurface){ .surfaceId = 1,
		                           .windowId = window->windowId,
		                           .windowMapped = TRUE,
		                           .width = 1000,
		                           .height = 800,
		                           .mappedWidth = 1000,
		                           .mappedHeight = 800,
		                           .scanline = 4000 };
	region16_init(&testSurface.invalidRegion);
	testSurface.data = calloc(800, 4000);
	CHECK(testSurface.data);
	for (size_t i = 0; i < 800000; i++)
		((UINT32*)testSurface.data)[i] = 0x123456;
	gdi->gfx = &gfx;
	xfc->visual = DefaultVisual(xfc->display, DefaultScreen(xfc->display));
	xfc->scanline_pad = 32;
	CHECK(freerdp_settings_set_bool(xfc->common.context.settings, FreeRDP_SoftwareGdi, TRUE));
	window->surfaceId = 1;
	window->is_mapped = TRUE;
	XResizeWindow(xfc->display, window->handle, 640, 480);
	const XConfigureEvent size = { .send_event = True, .width = 640, .height = 480 };
	CHECK(xf_rail_update_configure_geometry(xfc, window, &size));
	CHECK(window->surfaceStale);
	freerdp instance = { .context = &xfc->common.context };
	xfc->exposedWindow = window->handle;
	xfc->exposedArea.w = 640;
	xfc->exposedArea.h = 480;
	xfc->exposeRequested = TRUE;
	CHECK(xf_event_update_screen(&instance));
	CHECK(!window->surfaceStale && painted_pixel(xfc, window, 630, 470));

	/* Both a never-started WM operation and an active server cancellation
	 * reconcile and paint new bounds after returning the RAIL window. */
	for (int state = LMS_STARTING; state <= LMS_ACTIVE; state++)
	{
		window->local_move.state = state;
		window->serverGeometryPending = TRUE;
		window->serverSizeGeneration++;
		window->windowWidth = (UINT32)(660 + state * 10);
		window->windowHeight = 510;
		const RAIL_LOCALMOVESIZE_ORDER end = { .windowId = (UINT32)window->windowId };
		CHECK(xfc->rail->ServerLocalMoveSize(xfc->rail, &end) == CHANNEL_RC_OK);
		XSync(xfc->display, False);
		xf_rail_flush_configure_geometry(xfc);
		CHECK(!window->surfaceStale && painted_pixel(xfc, window, 650, 500));
	}

	lock.locked = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	lock.release = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	CHECK(lock.locked && lock.release);
	thread = CreateThread(nullptr, 0, hold_gfx_lock, &lock, 0, nullptr);
	CHECK(thread && WaitForSingleObject(lock.locked, 5000) == WAIT_OBJECT_0);
	window->surfaceStale = TRUE;
	CHECK(xf_AppWindowRepaintFromSurface(xfc, window->windowId, 1));
	CHECK(window->surfaceStale); /* A busy GFX thread must not block X11. */
	SetEvent(lock.release);
	CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
	CHECK(xf_AppWindowRepaintFromSurface(xfc, window->windowId, 1));
	CHECK(!window->surfaceStale && painted_pixel(xfc, window, 650, 500));
	testSurface.windowId++;
	window->surfaceStale = TRUE;
	CHECK(xf_AppWindowRepaintFromSurface(xfc, window->windowId, 1));
	CHECK(window->surfaceStale); /* A stale mapping must not paint a different window. */
	testSurface.windowId--;
	CHECK(xf_AppWindowRepaintFromSurface(xfc, window->windowId, 1));
	CHECK(!window->surfaceStale);
	window->is_mapped = FALSE;
	window->surfaceStale = TRUE;
	CHECK(xf_AppWindowRepaintFromSurface(xfc, window->windowId, 1));
	CHECK(window->surfaceStale);
	window->is_mapped = TRUE;
	CHECK(xf_AppWindowRepaintFromSurface(xfc, window->windowId, 1));
	CHECK(!window->surfaceStale);
	window->surfaceStale = TRUE;
	CHECK(xf_AppWindowRepaintFromSurface(xfc, window->windowId, 2));
	CHECK(window->surfaceStale); /* The old surface may already have been deleted. */
	testSurface.windowId++;
	CHECK(xf_AppWindowRepaintFromSurface(xfc, testSurface.windowId, 1));
	CHECK(window->surfaceStale); /* The deferred window may have been deleted too. */
	testSurface.windowId--;
	CHECK(xf_AppWindowRepaintFromSurface(xfc, window->windowId, 1));
	CHECK(!window->surfaceStale);
	result = TRUE;
out:
	if (lock.release)
		SetEvent(lock.release);
	if (thread)
	{
		WaitForSingleObject(thread, INFINITE);
		CloseHandle(thread);
	}
	if (lock.locked)
		CloseHandle(lock.locked);
	if (lock.release)
		CloseHandle(lock.release);
	xf_AppWindowDestroyImage(window);
	window->surfaceId = UINT32_MAX;
	gdi->gfx = previous;
	DeleteCriticalSection(&gfx.mux);
	region16_uninit(&testSurface.invalidRegion);
	free(testSurface.data);
	return result;
}

static BOOL window_state_limits(xfContext* xfc, xfAppWindow* window)
{
#define INIT_ATOM(field) xfc->field = XInternAtom(xfc->display, "_" #field, False)
	INIT_ATOM(NET_WM_STATE);
	INIT_ATOM(NET_WM_STATE_ABOVE);
	INIT_ATOM(NET_WM_STATE_SKIP_PAGER);
	INIT_ATOM(NET_WM_STATE_SKIP_TASKBAR);
	INIT_ATOM(NET_WM_WINDOW_TYPE_DROPDOWN_MENU);
	INIT_ATOM(NET_WM_STATE_MAXIMIZED_VERT);
	INIT_ATOM(NET_WM_STATE_MAXIMIZED_HORZ);
	INIT_ATOM(NET_WM_WINDOW_TYPE);
	INIT_ATOM(NET_WM_WINDOW_TYPE_NORMAL);
	INIT_ATOM(NET_WM_ALLOWED_ACTIONS);
	INIT_ATOM(NET_WM_ACTION_CLOSE);
	INIT_ATOM(NET_WM_ACTION_MINIMIZE);
	INIT_ATOM(NET_WM_ACTION_MOVE);
	INIT_ATOM(NET_WM_ACTION_RESIZE);
	INIT_ATOM(NET_WM_ACTION_MAXIMIZE_HORZ);
	INIT_ATOM(NET_WM_ACTION_MAXIMIZE_VERT);
	INIT_ATOM(NET_WM_ACTION_FULLSCREEN);
	INIT_ATOM(NET_WM_ACTION_CHANGE_DESKTOP);
#undef INIT_ATOM
	stateAtom = xfc->NET_WM_STATE;
	maximizeAtom = xfc->NET_WM_STATE_MAXIMIZED_VERT;
	const UINT32 normal =
	    WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	const UINT32 stripped = WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	WINDOW_ORDER_INFO info = { .windowId = window->windowId,
		                       .fieldFlags = WINDOW_ORDER_FIELD_STYLE };
	WINDOW_STATE_ORDER state = { .style = normal };
	window->resizeCapabilityKnown = FALSE;
	window->rail_state = window->showState = WINDOW_SHOW;
	window->is_mapped = TRUE;
	XMapWindow(xfc->display, window->handle);
	xf_SetWindowMinMaxInfo(xfc, window, 0, 0, 0, 0, 136, 39, 1940, 1220);
	CHECK(apply_server_update(xfc, window, &info, &state));
	state.extendedStyle = WS_EX_TOOLWINDOW;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(window->is_transient);
	state.extendedStyle = 0;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(!window->is_transient && xf_rail_window_can_resize(window));
	state.style = stripped;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(xf_rail_window_can_resize(window));
	XSizeHints hints = WINPR_C_ARRAY_INIT;
	long supplied = 0;
	CHECK(XGetWMNormalHints(xfc->display, window->handle, &hints, &supplied));
	CHECK(hints.min_width == 136 && hints.max_width == 1940);

	/* Model a WM receiving maximize while older, fixed-size hints still
	 * exist. Inspect the actual properties at every outgoing state request. */
	hints.flags = PMinSize | PMaxSize;
	hints.min_width = hints.max_width = window->width;
	hints.min_height = hints.max_height = window->height;
	XSetWMNormalHints(xfc->display, window->handle, &hints);
	observeStateRequests = TRUE;
	info.fieldFlags |= WINDOW_ORDER_FIELD_SHOW | WINDOW_ORDER_FIELD_WND_SIZE;
	state.style = normal | WS_MAXIMIZE;
	state.showState = WINDOW_SHOW_MAXIMIZED;
	state.windowWidth = 1000;
	state.windowHeight = 800;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(stateRequests > 0 && stateLimitsValid);
	state.style = normal;
	state.showState = WINDOW_SHOW;
	state.windowWidth = 400;
	state.windowHeight = 300;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(stateLimitsValid);
	observeStateRequests = FALSE;
	state.style = normal & ~WS_SIZEBOX;
	info.fieldFlags = WINDOW_ORDER_FIELD_STYLE;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(!xf_rail_window_can_resize(window));
	CHECK(XGetWMNormalHints(xfc->display, window->handle, &hints, &supplied));
	CHECK(hints.min_width == 400 && hints.max_width == 400);
	state.style = normal;
	CHECK(apply_server_update(xfc, window, &info, &state));
	return TRUE;
out:
	observeStateRequests = FALSE;
	return FALSE;
}

static BOOL maximized_shape_updates(xfContext* xfc, xfAppWindow* window)
{
	/* Complete the preceding native gesture before the independent show-state
	 * scenario. A server reply no longer retires its WM ownership early. */
	for (unsigned i = 0; i < 25 && window->localMoveGeometryTime; i++)
	{
		CHECK(WaitForSingleObject(xf_rail_geometry_event(xfc), 1000) == WAIT_OBJECT_0);
		xf_rail_flush_configure_geometry(xfc);
	}
	CHECK(!window->localMoveGeometryTime);
	const UINT32 normal = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX;
	const UINT32 stripped = WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	RECTANGLE_16 visible = { .right = 400, .bottom = 280 };
	WINDOW_ORDER_INFO info = { .windowId = window->windowId,
		                       .fieldFlags = WINDOW_ORDER_FIELD_STYLE | WINDOW_ORDER_FIELD_SHOW |
		                                     WINDOW_ORDER_FIELD_WND_SIZE |
		                                     WINDOW_ORDER_FIELD_VISIBILITY };
	WINDOW_STATE_ORDER state = { .style = normal,
		                         .showState = WINDOW_SHOW,
		                         .windowWidth = 400,
		                         .windowHeight = 300,
		                         .numVisibilityRects = 1,
		                         .visibilityRects = &visible };
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(has_shape(xfc->display, window->handle, 400, 280));
	/* The server can announce maximize before the WM resizes its drawable.
	 * The accepted configure must publish an explicit shape at the new size. */
	const unsigned before = moves;
	info.fieldFlags = WINDOW_ORDER_FIELD_SHOW;
	state.showState = WINDOW_SHOW_MAXIMIZED;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(window->stateGeometryPending);
	XResizeWindow(xfc->display, window->handle, 1000, 800);
	XSync(xfc->display, False);
	const XConfigureEvent configured = { .send_event = True, .width = 1000, .height = 800 };
	CHECK(xf_rail_update_configure_geometry(xfc, window, &configured));
	CHECK(!xf_rail_adjust_position(xfc, window, nullptr));
	CHECK(moves == before);
	info.fieldFlags = WINDOW_ORDER_FIELD_STYLE | WINDOW_ORDER_FIELD_WND_SIZE |
	                  WINDOW_ORDER_FIELD_RESIZE_MARGIN_X | WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y;
	state.style = normal | WS_MAXIMIZE;
	state.windowWidth = 1000;
	state.windowHeight = 800;
	state.resizeMarginLeft = state.resizeMarginRight = 8;
	state.resizeMarginTop = state.resizeMarginBottom = 8;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(!window->stateGeometryPending);
	CHECK(xf_rail_adjust_position(xfc, window, nullptr));
	CHECK(moves == before);
	CHECK(has_shape(xfc->display, window->handle, 1000, 800));
	/* A repeated no-op show does not wait for a nonexistent new size. */
	info.fieldFlags = WINDOW_ORDER_FIELD_SHOW;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(!window->stateGeometryPending);
	Bool bounding = False;
	Bool clip = False;
	int bx = 0;
	int by = 0;
	int cx = 0;
	int cy = 0;
	unsigned bw = 0;
	unsigned bh = 0;
	unsigned cw = 0;
	unsigned ch = 0;
	CHECK(XShapeQueryExtents(xfc->display, window->handle, &bounding, &bx, &by, &bw, &bh, &clip,
	                         &cx, &cy, &cw, &ch));
	CHECK(bounding && bw == 1000 && bh == 800);
	/* Temporary frame stripping must not restore the old normal-size clip. */
	info.fieldFlags = WINDOW_ORDER_FIELD_STYLE;
	state.style = stripped;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(xf_rail_update_configure_geometry(xfc, window, &configured));
	CHECK(has_shape(xfc->display, window->handle, 1000, 800));
	/* Normal shaped windows retain their genuine server visibility region. */
	info.fieldFlags |= WINDOW_ORDER_FIELD_SHOW | WINDOW_ORDER_FIELD_WND_SIZE;
	state.style = normal;
	state.showState = WINDOW_SHOW;
	state.windowWidth = 400;
	state.windowHeight = 300;
	CHECK(apply_server_update(xfc, window, &info, &state));
	CHECK(has_shape(xfc->display, window->handle, 400, 280));
	return TRUE;
out:
	return FALSE;
}

static Window pointer_target(Display* display, Window root)
{
	Window target = root;
	for (size_t level = 0; level < 8; level++)
	{
		Window returnedRoot = None;
		Window child = None;
		int rx = 0;
		int ry = 0;
		int x = 0;
		int y = 0;
		unsigned mask = 0;
		if (!XQueryPointer(display, target, &returnedRoot, &child, &rx, &ry, &x, &y, &mask) ||
		    !child)
			return target;
		target = child;
	}
	return target;
}

static BOOL transparent_resize_margins(xfContext* xfc, xfAppWindow* window, Display* injector,
                                       Window frame)
{
	BOOL result = FALSE;
	Window foreground = None;
	Display* display = xfc->display;
	const Window root = DefaultRootWindow(display);
	XEvent event = WINPR_C_ARRAY_INIT;
	window->dwStyle = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX;
	xf_rail_update_window_style(window, window->dwStyle, 0);
	window->is_mapped = TRUE;
	window->rail_state = window->showState = WINDOW_SHOW;
	window->local_move.state = LMS_NOT_ACTIVE;
	window->resizeMarginXKnown = window->resizeMarginYKnown = TRUE;
	window->resizeMarginLeft = window->resizeMarginRight = 7;
	window->resizeMarginTop = 3;
	window->resizeMarginBottom = 7;
	window->windowOffsetX = 200;
	window->windowOffsetY = 200;
	window->windowWidth = 400;
	window->windowHeight = 300;
	XMoveResizeWindow(display, window->handle, 200, 200, 400, 300);
	XMapWindow(display, window->handle);
	XRaiseWindow(display, frame);
	XSync(display, False);
	const XConfigureEvent size = {
		.send_event = True, .x = 200, .y = 200, .width = 400, .height = 300
	};
	CHECK(xf_rail_update_configure_geometry(xfc, window, &size));
	CHECK(window->inputHandle && window->inputMapped);
	XWindowAttributes attributes = WINPR_C_ARRAY_INIT;
	CHECK(XGetWindowAttributes(display, window->inputHandle, &attributes));
	CHECK(attributes.class == InputOnly);
	xfAppWindow* found = xf_AppWindowFromX11Window(xfc, window->inputHandle);
	const BOOL ownerMatches = found == window;
	xf_rail_return_window(found, FALSE);
	CHECK(ownerMatches);
	found = xf_rail_get_window(xfc, window->windowId, FALSE);
	const BOOL idMatches = found == window;
	xf_rail_return_window(found, FALSE);
	CHECK(idMatches);
	freerdp notificationInstance = { .context = &xfc->common.context };
	const XEvent auxiliaryConfigure = { .xconfigure = { .type = ConfigureNotify,
		                                                .event = root,
		                                                .window = window->inputHandle,
		                                                .width = 414,
		                                                .height = 310 } };
	CHECK(xf_event_process(&notificationInstance, &auxiliaryConfigure));
	CHECK(window->width == 400 && window->height == 300);
	const XEvent auxiliaryUnmap = {
		.xunmap = { .type = UnmapNotify, .event = root, .window = window->inputHandle }
	};
	CHECK(xf_event_process(&notificationInstance, &auxiliaryUnmap));
	CHECK(window->is_mapped);
	/* All four margins and corners belong to this window; its content is
	 * a hole in the auxiliary region, and outside points remain untouched. */
	const int points[][2] = { { 198, 350 }, { 602, 350 }, { 400, 198 },
		                      { 400, 502 }, { 198, 198 }, { 602, 505 } };
	for (size_t i = 0; i < ARRAYSIZE(points); i++)
	{
		XTestFakeMotionEvent(injector, -1, points[i][0], points[i][1], CurrentTime);
		XSync(injector, False);
		CHECK(pointer_target(display, root) == window->inputHandle);
	}
	XTestFakeMotionEvent(injector, -1, 400, 350, CurrentTime);
	XSync(injector, False);
	CHECK(pointer_target(display, root) == window->handle);
	XTestFakeMotionEvent(injector, -1, 609, 350, CurrentTime);
	XSync(injector, False);
	CHECK(pointer_target(display, root) != window->inputHandle);

	/* The original failure: a press two pixels outside the visible right
	 * edge must activate the owner and preserve root coordinates. */
	xfc->button_map[0].button = Button1;
	xfc->button_map[0].flags = PTR_FLAGS_BUTTON1;
	xfc->railActiveWindowId = 0;
	const unsigned before = presses;
	XTestFakeMotionEvent(injector, -1, 602, 350, CurrentTime);
	XTestFakeButtonEvent(injector, Button1, True, CurrentTime);
	XSync(injector, False);
	XSync(display, False);
	CHECK(XCheckTypedWindowEvent(display, window->inputHandle, ButtonPress, &event));
	freerdp instance = { .context = &xfc->common.context };
	const Time pressTime = event.xbutton.time;
	CHECK(xf_event_process(&instance, &event));
	CHECK(activationMessage.data.l[0] == 1 && (Time)activationMessage.data.l[1] == pressTime &&
	      pressTime != CurrentTime);
	CHECK(presses == before + 1 && activatedWindow == window->windowId);
	CHECK(pressX == 602 && pressY == 350);
	xf_StartLocalMoveSize(xfc, window, NET_WM_MOVERESIZE_SIZE_RIGHT, 602, 350);
	const unsigned releaseBefore = releases;
	XTestFakeButtonEvent(injector, Button1, False, CurrentTime);
	XSync(injector, False);
	XSync(display, False);
	CHECK(XCheckTypedWindowEvent(display, window->inputHandle, ButtonRelease, &event));
	CHECK(xf_event_process(&instance, &event));
	CHECK(releases == releaseBefore + 1 && window->local_move.state == LMS_TERMINATING);
	const RAIL_LOCALMOVESIZE_ORDER end = { .windowId = (UINT32)window->windowId,
		                                   .posX = 200,
		                                   .posY = 200 };
	CHECK(xfc->rail->ServerLocalMoveSize(xfc->rail, &end) == CHANNEL_RC_OK);

	foreground = XCreateSimpleWindow(display, root, 590, 340, 40, 30, 0, 0, 0);
	XMapRaised(display, foreground);
	xf_UpdateWindowInputRegion(xfc, window);
	XSync(display, False);
	CHECK(pointer_target(display, root) == foreground);
	XLowerWindow(display, foreground);
	xf_UpdateWindowInputRegion(xfc, window);
	XSync(display, False);
	CHECK(pointer_target(display, root) == window->inputHandle);

	/* A WM can lower or hide only its frame. Dispatch the actual ancestor
	 * events; no client configure or explicit region refresh is involved. */
	XLowerWindow(display, frame);
	XSync(display, False);
	while (XCheckWindowEvent(display, frame, StructureNotifyMask | VisibilityChangeMask, &event))
		CHECK(xf_event_process(&instance, &event));
	XSync(display, False);
	CHECK(pointer_target(display, root) == foreground);
	XRaiseWindow(display, frame);
	XUnmapWindow(display, frame);
	XSync(display, False);
	while (XCheckWindowEvent(display, frame, StructureNotifyMask | VisibilityChangeMask, &event))
		CHECK(xf_event_process(&instance, &event));
	XSync(display, False);
	CHECK(!window->inputMapped);
	CHECK(XGetWindowAttributes(display, window->handle, &attributes));
	CHECK(attributes.map_state == IsUnviewable);
	XMapWindow(display, frame);
	XSync(display, False);
	while (XCheckWindowEvent(display, frame, StructureNotifyMask | VisibilityChangeMask, &event))
		CHECK(xf_event_process(&instance, &event));
	CHECK(window->inputMapped);

	/* Follow reparenting-frame movement without changing content geometry. */
	XMoveWindow(display, frame, 100, 50);
	XSync(display, False);
	XConfigureEvent moved = size;
	moved.x = 300;
	moved.y = 250;
	CHECK(xf_rail_update_configure_geometry(xfc, window, &moved));
	XTestFakeMotionEvent(injector, -1, 702, 400, CurrentTime);
	XSync(injector, False);
	CHECK(pointer_target(display, root) == window->inputHandle);

	/* Minimized, fixed, transient and unknown-margin windows must not
	 * leave an input region over another application. */
	window->rail_state = WINDOW_SHOW_MINIMIZED;
	xf_UpdateWindowInputRegion(xfc, window);
	CHECK(!window->inputMapped);
	window->rail_state = WINDOW_SHOW;
	xf_rail_update_window_style(window, WS_VISIBLE | WS_CAPTION | WS_SYSMENU, 0);
	xf_UpdateWindowInputRegion(xfc, window);
	CHECK(!window->inputMapped);
	xf_rail_update_window_style(window, WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX, 0);
	window->resizeMarginXKnown = FALSE;
	xf_UpdateWindowInputRegion(xfc, window);
	CHECK(!window->inputMapped);
	window->resizeMarginXKnown = TRUE;
	window->is_transient = TRUE;
	xf_UpdateWindowInputRegion(xfc, window);
	CHECK(!window->inputMapped);
	window->is_transient = FALSE;
	xf_UpdateWindowInputRegion(xfc, window);
	CHECK(window->inputMapped);
	XUnmapWindow(display, window->handle);
	XSync(display, False);
	const XEvent unmap = { .xunmap = { .type = UnmapNotify, .window = window->handle } };
	CHECK(xf_event_process(&instance, &unmap));
	CHECK(!window->inputMapped && !window->is_mapped);
	XMapWindow(display, window->handle);
	XSync(display, False);
	const XEvent map = { .xmap = { .type = MapNotify, .window = window->handle } };
	CHECK(xf_event_process(&instance, &map));
	CHECK(window->inputMapped);
	result = TRUE;
out:
	if (foreground)
		XDestroyWindow(display, foreground);
	return result;
}

static BOOL deleted_margin_owner(xfContext* xfc)
{
	BOOL inserted = FALSE;
	BOOL result = FALSE;
	xfAppWindow* window = calloc(1, sizeof(*window));
	CHECK(window);
	window->xfc = xfc;
	window->windowId = 0x300d1;
	window->surfaceId = UINT32_MAX;
	window->shmid = -1;
	window->x = 10;
	window->y = 10;
	window->width = 100;
	window->height = 100;
	window->is_mapped = TRUE;
	window->rail_state = WINDOW_SHOW;
	window->resizeMarginXKnown = window->resizeMarginYKnown = TRUE;
	window->resizeMarginLeft = window->resizeMarginRight = 7;
	xf_rail_update_window_style(window, WS_VISIBLE | WS_CAPTION | WS_SIZEBOX, 0);
	window->handle = XCreateSimpleWindow(xfc->display, DefaultRootWindow(xfc->display), 10, 10, 100,
	                                     100, 0, 0, 0);
	CHECK(window->handle);
	CHECK(HashTable_Insert(xfc->railWindows, &window->windowId, window));
	inserted = TRUE;
	XMapWindow(xfc->display, window->handle);
	XSync(xfc->display, False);
	xf_UpdateWindowInputRegion(xfc, window);
	CHECK(window->inputMapped);
	const Window input = window->inputHandle;
	const Window handle = window->handle;
	xfc->appWindow = window;
	CHECK(xf_rail_del_window(xfc, window->windowId));
	inserted = FALSE;
	window = nullptr;
	CHECK(!xfc->appWindow);
	Window root = None;
	Window parent = None;
	Window* children = nullptr;
	unsigned count = 0;
	CHECK(XQueryTree(xfc->display, DefaultRootWindow(xfc->display), &root, &parent, &children,
	                 &count));
	BOOL removed = TRUE;
	for (unsigned i = 0; i < count; i++)
		removed &= children[i] != input && children[i] != handle;
	if (children)
		XFree(children);
	CHECK(removed);
	result = TRUE;
out:
	if (inserted)
	{
		const BOOL deleted = xf_rail_del_window(xfc, window->windowId);
		WINPR_UNUSED(deleted);
	}
	else if (window)
	{
		if (window->handle)
			XDestroyWindow(xfc->display, window->handle);
		free(window);
	}
	return result;
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
	RailClientContext rail = { .ClientWindowMove = sendMove, .ClientActivate = sendActivate };
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
	window.gc = XCreateGC(display, window.handle, 0, nullptr);
	CHECK(window.gc);
	window.xfc = &xfc;
	window.windowId = 0x300d0;
	window.surfaceId = UINT32_MAX;
	window.is_mapped = TRUE;
	window.width = 400;
	window.windowWidth = 400;
	window.height = 300;
	window.windowHeight = 300;
	window.resizeMarginXKnown = window.resizeMarginYKnown = TRUE;
	CHECK(HashTable_Insert(xfc.railWindows, &window.windowId, &window));
	XSelectInput(display, window.handle, ButtonPressMask | ButtonReleaseMask);
	XMapWindow(display, window.handle);
	XMapWindow(display, frame);
	XSync(display, False);
	RECTANGLE_16 visible = { .right = 400, .bottom = 280 };
	xf_SetWindowVisibilityRects(&xfc, &window, 0, 0, &visible, 1);
	CHECK(has_shape(display, window.handle, 400, 280));
	CHECK(redirected_geometry_requests(&xfc, &window, injector, frame));
	CHECK(resize_window(&xfc, &window, injector, 800, 600, TRUE, FALSE));
	CHECK(resize_window(&xfc, &window, injector, 500, 400, FALSE, FALSE));
	CHECK(resize_window(&xfc, &window, injector, 900, 650, FALSE, TRUE));
	CHECK(resize_window(&xfc, &window, injector, 600, 450, TRUE, TRUE));
	CHECK(asynchronous_pixmap_failure(&xfc, &window, injector));
	CHECK(cancel_resize(&xfc, &window));
	CHECK(fixed_size_window(&xfc, &window));
	CHECK(wm_owned_resize(&xfc, &window, injector));
	CHECK(repaint_gfx_window(&xfc, &window));
	CHECK(window_state_limits(&xfc, &window));
	CHECK(maximized_shape_updates(&xfc, &window));
	CHECK(transparent_resize_margins(&xfc, &window, injector, frame));
	CHECK(deleted_margin_owner(&xfc));
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
		if (window.inputHandle)
			XDestroyWindow(display, window.inputHandle);
		if (window.gc)
			XFreeGC(display, window.gc);
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
