/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <freerdp/config.h>
#include <stdio.h>
#include <X11/keysym.h>
#include <freerdp/window.h>
#include <winpr/collections.h>
#include <winpr/sysinfo.h>
#include "../xfreerdp.h"
#include "../xf_rail.h"
#include "../xf_window.h"
#include "../xf_event.h"

static xfAppWindow appWindow;
static unsigned translations;
static unsigned moves;
static unsigned xMoves;
static UINT moveResult = CHANNEL_RC_OK;
static int rootX = 700;
static int rootY = 245;
static Status querySuccess = 1;
static Window windowParent = 1;
static RAIL_WINDOW_MOVE_ORDER lastMove;
static unsigned releases;
static int releaseX;
static int releaseY;
static Bool pointerSuccess = True;
static BOOL inputSuccess = TRUE;
static KeySym lookupKey = XK_Return;
static Status attributesSuccess = 1;
static Bool translateSuccess = True;
static unsigned attributeQueries;
static BOOL changingAttributes;
static int currentWidth = 500;
static int currentHeight = 420;

Status XGetWindowAttributes(Display* display, Window window, XWindowAttributes* attributes)
{
	WINPR_UNUSED(display);
	WINPR_UNUSED(window);
	attributeQueries++;
	*attributes = (XWindowAttributes){ .root = 1, .width = currentWidth, .height = currentHeight };
	if (changingAttributes)
		attributes->width += (int)(attributeQueries % 2);
	return attributesSuccess;
}

Bool XQueryPointer(Display* display, Window window, Window* root, Window* child, int* rx, int* ry,
                   int* cx, int* cy, unsigned int* mask)
{
	WINPR_UNUSED(display);
	WINPR_UNUSED(window);
	*root = *child = None;
	*rx = rootX;
	*ry = rootY;
	*cx = *cy = 0;
	*mask = 0;
	return pointerSuccess;
}

KeySym XLookupKeysym(XKeyEvent* event, int index)
{
	WINPR_UNUSED(event);
	WINPR_UNUSED(index);
	return lookupKey;
}

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
	return inputSuccess;
}

#define CHECK(condition)                                                          \
	do                                                                            \
	{                                                                             \
		if (!(condition))                                                         \
		{                                                                         \
			(void)fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			goto out;                                                             \
		}                                                                         \
	} while (0)

/* Exercise the actual configure/RAIL logic without an X server or RDP server. */
Status XQueryTree(Display* display, Window window, Window* root, Window* parent, Window** children,
                  unsigned int* count)
{
	WINPR_UNUSED(display);
	WINPR_UNUSED(window);
	*root = None;
	*parent = windowParent;
	*children = nullptr;
	*count = 0;
	return querySuccess;
}

Bool XTranslateCoordinates(Display* display, Window src, Window dst, int sx, int sy, int* x, int* y,
                           Window* child)
{
	WINPR_UNUSED(display);
	WINPR_UNUSED(src);
	WINPR_UNUSED(dst);
	WINPR_UNUSED(sx);
	WINPR_UNUSED(sy);
	translations++;
	*x = rootX;
	*y = rootY;
	*child = None;
	return translateSuccess;
}

Pixmap XCreatePixmap(Display* display, Drawable drawable, unsigned int width, unsigned int height,
                     unsigned int depth)
{
	WINPR_UNUSED(display);
	WINPR_UNUSED(drawable);
	WINPR_UNUSED(width);
	WINPR_UNUSED(height);
	WINPR_UNUSED(depth);
	return 1;
}

int XFreePixmap(Display* display, Pixmap pixmap)
{
	WINPR_UNUSED(display);
	WINPR_UNUSED(pixmap);
	return 1;
}

void XLockDisplay(Display* display)
{
	WINPR_UNUSED(display);
}

void XUnlockDisplay(Display* display)
{
	WINPR_UNUSED(display);
}

int XMoveWindow(Display* display, Window window, int x, int y)
{
	xMoves++;
	WINPR_UNUSED(display);
	WINPR_UNUSED(window);
	WINPR_UNUSED(x);
	WINPR_UNUSED(y);
	return 1;
}

int XMoveResizeWindow(Display* display, Window window, int x, int y, unsigned int width,
                      unsigned int height)
{
	xMoves++;
	WINPR_UNUSED(display);
	WINPR_UNUSED(window);
	WINPR_UNUSED(x);
	WINPR_UNUSED(y);
	WINPR_UNUSED(width);
	WINPR_UNUSED(height);
	return 1;
}

static UINT sendMove(RailClientContext* rail, const RAIL_WINDOW_MOVE_ORDER* order)
{
	WINPR_UNUSED(rail);
	moves++;
	lastMove = *order;
	return moveResult;
}

static BOOL test_configure_snapshots(xfContext* xfc, XConfigureEvent* event)
{
	/* A queued server echo must use its snapshot, not a newer XTranslate result. */
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(translations == 0);
	CHECK(appWindow.x == 48 && appWindow.y == 0);
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == 0);

	/* A parent-relative server echo must not query a transient root position
	 * between the WM's client/frame moves. Its synthetic notification follows. */
	event->send_event = False;
	event->x = 27;
	event->y = 6;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(translations == 0 && moves == 0);
	CHECK(appWindow.x == 48 && appWindow.y == 0);

	/* Active moves must not capture the transient doubled root coordinate. */
	appWindow.local_move.state = LMS_ACTIVE;
	rootX = 2597;
	rootY = 369;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(translations == 0 && appWindow.x == 48 && appWindow.y == 0);
	event->send_event = True;
	event->x = 1295;
	event->y = 186;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(translations == 0 && appWindow.x == 1295 && appWindow.y == 186);
	CHECK(appWindow.windowOffsetX == 48 && appWindow.windowOffsetY == 0);
	CHECK(!xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == 0);
	/* A real child resize cannot be combined with the older root position. */
	event->send_event = False;
	event->x = 27;
	event->y = 6;
	event->width = 409;
	event->height = 382;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(appWindow.x == 1295 && appWindow.y == 186);
	CHECK(appWindow.width == 395 && appWindow.height == 375 && translations == 0);
	event->send_event = True;
	event->x = 1295;
	event->y = 186;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(appWindow.width == 409 && appWindow.height == 382);
	event->width = 395;
	event->height = 375;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	appWindow.local_move.state = LMS_NOT_ACTIVE;

	/* A non-reparenting WM's real root-space move must not be deferred. */
	windowParent = None;
	event->send_event = False;
	event->x = 700;
	event->y = 245;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(translations == 0 && appWindow.x == 700 && appWindow.y == 245);
	windowParent = 1;
	translations = 0;

	/* Wait for a complete synthetic root-space resize snapshot. */
	event->send_event = False;
	event->x = 0;
	event->width = 450;
	event->height = 400;
	appWindow.resizeMarginLeft = appWindow.resizeMarginRight = 7;
	appWindow.resizeMarginBottom = 7;
	appWindow.resizeMarginXKnown = appWindow.resizeMarginYKnown = TRUE;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(moves == 0);
	event->send_event = True;
	event->x = 700;
	event->y = 245;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(translations == 0 && appWindow.x == 700 && appWindow.y == 245);
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == 1);
	CHECK(lastMove.left == 693 && lastMove.top == 245);
	CHECK(lastMove.right == 1157 && lastMove.bottom == 652);
	CHECK(appWindow.windowMovePending);
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == 1);

	/* The synthetic partner of a real resize must not enqueue the same move. */
	event->send_event = True;
	event->x = 700;
	event->y = 245;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == 1);

	/* A failed parent query must not overwrite the last known local geometry. */
	event->send_event = False;
	querySuccess = 0;
	rootX = 9999;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(appWindow.x == 700 && appWindow.y == 245);
	querySuccess = 1;

	/* Resizing back to the last server size is still a real size change when
	 * a previous local resize has not been acknowledged by Windows yet. */
	rootX = 700;
	event->send_event = True;
	event->x = 700;
	event->y = 245;
	event->width = 395;
	event->height = 375;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(appWindow.width == 395 && appWindow.height == 375);
	event->width = 450;
	event->height = 400;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));

	/* Prevent the signed-16-bit wrap observed during the runaway position loop. */
	appWindow.x = 32760;
	CHECK(!xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == 1);
	appWindow.x = -32000;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == 2 && lastMove.left == -32007);
	return TRUE;
out:
	return FALSE;
}

static BOOL test_server_reconciliation(xfContext* xfc, XConfigureEvent* event)
{
	rdpWindowUpdate* windows = xfc->common.context.update->window;

	/* Execute the registered server callback: successive surface sizes must all
	 * apply, including the initial title strip and its immediate expansion. */
	const WINDOW_ORDER_INFO info = { .windowId = appWindow.windowId,
		                             .fieldFlags = WINDOW_ORDER_FIELD_WND_SIZE };
	const UINT32 sizes[][2] = { { 395, 375 }, { 381, 368 }, { 122, 32 }, { 409, 382 } };
	for (size_t i = 0; i < ARRAYSIZE(sizes); i++)
	{
		const WINDOW_STATE_ORDER state = { .windowWidth = sizes[i][0],
			                               .windowHeight = sizes[i][1] };
		CHECK(windows->WindowUpdate(&xfc->common.context, &info, &state));
		CHECK(appWindow.windowWidth == sizes[i][0] && appWindow.windowHeight == sizes[i][1]);
		CHECK(appWindow.width == (int)sizes[i][0] && appWindow.height == (int)sizes[i][1]);
	}

	/* The local-move final report must use the confirmed synthetic snapshot.
	 * Keyboard completion needs no pointer query (and this test has no X server). */
	appWindow.local_move.state = LMS_ACTIVE;
	appWindow.local_move.direction = NET_WM_MOVERESIZE_MOVE_KEYBOARD;
	event->send_event = True;
	event->x = 1295;
	event->y = 186;
	event->width = 409;
	event->height = 382;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(xf_rail_end_local_move(xfc, &appWindow));
	CHECK(appWindow.local_move.state == LMS_TERMINATING);
	CHECK(lastMove.left == 1288 && lastMove.top == 186);
	CHECK(appWindow.windowOffsetX == 48 && appWindow.windowOffsetY == 0);
	CHECK(translations == 0);

	/* A normalized server reply retires the pending request without requiring
	 * equal visible and outer bounds. Metadata alone does not retire it. */
	const unsigned before = moves;
	appWindow.local_move.state = LMS_NOT_ACTIVE;
	appWindow.x = 700;
	appWindow.y = 245;
	appWindow.width = 450;
	appWindow.height = 400;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == before + 1);
	const WINDOW_ORDER_INFO unchanged = { .windowId = appWindow.windowId };
	const WINDOW_STATE_ORDER empty = { 0 };
	CHECK(windows->WindowUpdate(&xfc->common.context, &unchanged, &empty));
	CHECK(appWindow.windowMovePending);
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == before + 1);

	const WINDOW_ORDER_INFO adjusted = { .windowId = appWindow.windowId,
		                                 .fieldFlags = WINDOW_ORDER_FIELD_WND_OFFSET |
		                                               WINDOW_ORDER_FIELD_WND_SIZE };
	const WINDOW_STATE_ORDER normalized = {
		.windowOffsetX = 707, .windowOffsetY = 245, .windowWidth = 436, .windowHeight = 393
	};
	CHECK(windows->WindowUpdate(&xfc->common.context, &adjusted, &normalized));
	CHECK(!appWindow.windowMovePending);
	CHECK(appWindow.x == 707 && appWindow.width == 436 && appWindow.height == 393);
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == before + 1);

	/* A later gesture may request the same rectangle again. A -> B -> A must
	 * also remain valid; this is not a permanent rectangle blacklist. */
	appWindow.x = 700;
	appWindow.width = 450;
	appWindow.height = 400;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == before + 2);
	appWindow.x = 701;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	appWindow.x = 700;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == before + 4);

	/* Failed sends are never cached as successful or suppressed on retry. */
	appWindow.windowMovePending = FALSE;
	moveResult = ERROR_INTERNAL_ERROR;
	CHECK(!xf_rail_adjust_position(xfc, &appWindow));
	CHECK(!xf_rail_adjust_position(xfc, &appWindow));
	CHECK(!appWindow.windowMovePending && moves == before + 6);
	moveResult = CHANNEL_RC_OK;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(appWindow.windowMovePending && moves == before + 7);
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == before + 7);
	appWindow.lastWindowMoveTime = winpr_GetTickCount64NS() - 501ULL * 1000000ULL;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == before + 8);
	return TRUE;
out:
	return FALSE;
}

static BOOL test_native_moves(xfContext* xfc, XConfigureEvent* event)
{
	rdpWindowUpdate* windows = xfc->common.context.update->window;
	RailClientContext* rail = xfc->rail;
	const WINDOW_ORDER_INFO unchanged = { .windowId = appWindow.windowId };
	const WINDOW_STATE_ORDER empty = WINPR_C_ARRAY_INIT;
	const WINDOW_ORDER_INFO adjusted = { .windowId = appWindow.windowId,
		                                 .fieldFlags = WINDOW_ORDER_FIELD_WND_OFFSET |
		                                               WINDOW_ORDER_FIELD_WND_SIZE };

	/* Missing margins are not zero, and are not inherited from an owner. */
	const unsigned checkpoint = moves;
	appWindow.resizeMarginXKnown = appWindow.resizeMarginYKnown = FALSE;
	appWindow.ownerWindowId = 0x40118;
	const WINDOW_ORDER_INFO marginX = { .windowId = appWindow.windowId,
		                                .fieldFlags = WINDOW_ORDER_FIELD_RESIZE_MARGIN_X };
	CHECK(windows->WindowUpdate(&xfc->common.context, &marginX, &empty));
	CHECK(appWindow.resizeMarginXKnown && !appWindow.resizeMarginYKnown);
	CHECK(!xf_rail_has_resize_margins(&appWindow));
	CHECK(windows->WindowUpdate(&xfc->common.context, &unchanged, &empty));
	CHECK(appWindow.resizeMarginXKnown && !appWindow.resizeMarginYKnown);
	appWindow.windowOffsetX = appWindow.x = 481;
	appWindow.windowOffsetY = appWindow.y = 114;
	appWindow.windowWidth = appWindow.width = 413;
	appWindow.windowHeight = appWindow.height = 229;
	appWindow.x = 228;
	appWindow.y = 239;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == checkpoint);
	CHECK(appWindow.x == 481 && appWindow.y == 114 && appWindow.width == 413);
	const unsigned restored = xMoves;
	appWindow.y = 0;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(xMoves == restored && moves == checkpoint);
	appWindow.lastUnknownMarginReconcile = winpr_GetTickCount64NS() - 501ULL * 1000000ULL;
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(appWindow.x == 481 && appWindow.y == 114 && xMoves == restored + 1);

	/* Server-initiated native mouse moves need no outer rectangle, even on
	 * a Server 2016 dialog. Motion, focus and grab events must not end them. */
	xfc->remote_app = TRUE;
	appWindow.local_move.state = LMS_STARTING;
	appWindow.local_move.direction = NET_WM_MOVERESIZE_MOVE;
	XEvent input = { 0 };
	const int incidental[] = { MotionNotify, EnterNotify, LeaveNotify,  FocusIn,
		                       FocusOut,     ButtonPress, ClientMessage };
	for (size_t i = 0; i < ARRAYSIZE(incidental); i++)
	{
		input.type = incidental[i];
		CHECK(xf_event_suppress_events(xfc, &appWindow, &input));
		CHECK(appWindow.local_move.state == LMS_STARTING && releases == 0);
	}
	input.type = ConfigureNotify;
	CHECK(!xf_event_suppress_events(xfc, &appWindow, &input));
	CHECK(appWindow.local_move.state == LMS_ACTIVE);
	event->send_event = True;
	event->x = 228;
	event->y = 239;
	event->width = 413;
	event->height = 229;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	input.type = MotionNotify;
	CHECK(xf_event_suppress_events(xfc, &appWindow, &input));
	CHECK(appWindow.local_move.state == LMS_ACTIVE && releases == 0);
	rootX = 300;
	rootY = 250;
	input.type = ButtonRelease;
	input.xbutton.button = Button1;
	CHECK(xf_event_suppress_events(xfc, &appWindow, &input));
	CHECK(releases == 1 && releaseX == 300 && releaseY == 250);
	CHECK(moves == checkpoint && appWindow.local_move.state == LMS_TERMINATING);
	CHECK(appWindow.windowOffsetX == 481 && appWindow.windowOffsetY == 114);
	CHECK(xf_rail_end_local_move(xfc, &appWindow));
	CHECK(releases == 1);
	CHECK(!xf_rail_adjust_position(xfc, &appWindow));

	const RAIL_LOCALMOVESIZE_ORDER end = { .windowId = (UINT32)appWindow.windowId,
		                                   .moveSizeType = RAIL_WMSZ_MOVE };
	const WINDOW_STATE_ORDER movedDialog = {
		.windowOffsetX = 228, .windowOffsetY = 239, .windowWidth = 413, .windowHeight = 229
	};
	CHECK(windows->WindowUpdate(&xfc->common.context, &adjusted, &movedDialog));
	CHECK(appWindow.serverGeometryPending);
	CHECK(rail->ServerLocalMoveSize(rail, &end) == CHANNEL_RC_OK);
	CHECK(appWindow.local_move.state == LMS_NOT_ACTIVE && !appWindow.serverGeometryPending);
	CHECK(xf_rail_adjust_position(xfc, &appWindow));
	CHECK(moves == checkpoint && appWindow.width == 413 && appWindow.height == 229);

	/* Regression: twenty completed native moves do not progressively shrink. */
	for (int i = 0; i < 20; i++)
	{
		appWindow.local_move.state = LMS_ACTIVE;
		event->x = 250 + i;
		event->y = 200 + i;
		CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
		CHECK(xf_rail_end_local_move(xfc, &appWindow));
		const WINDOW_STATE_ORDER reply = { .windowOffsetX = event->x,
			                               .windowOffsetY = event->y,
			                               .windowWidth = 413,
			                               .windowHeight = 229 };
		CHECK(windows->WindowUpdate(&xfc->common.context, &adjusted, &reply));
		CHECK(rail->ServerLocalMoveSize(rail, &end) == CHANNEL_RC_OK);
		CHECK(xf_rail_adjust_position(xfc, &appWindow));
		CHECK(appWindow.width == 413 && appWindow.height == 229 && moves == checkpoint);
	}

	/* XI2 uses the generic pointer handlers, not core X event suppression. */
	const unsigned releasesBeforeXI = releases;
	const unsigned translationsBeforeXI = translations;
	appWindow.local_move.state = LMS_ACTIVE;
	CHECK(xf_generic_MotionNotify(xfc, 10, 10, appWindow.handle, TRUE));
	CHECK(xf_generic_ButtonEvent(xfc, 10, 10, Button1, appWindow.handle, TRUE, TRUE));
	CHECK(releases == releasesBeforeXI && translations == translationsBeforeXI);
	CHECK(appWindow.local_move.state == LMS_ACTIVE);
	xfc->xi_event = TRUE;
	input.type = ButtonRelease;
	input.xbutton.button = Button1;
	CHECK(xf_event_suppress_events(xfc, &appWindow, &input));
	CHECK(releases == releasesBeforeXI && appWindow.local_move.state == LMS_ACTIVE);
	CHECK(xf_generic_ButtonEvent(xfc, 10, 10, Button1, appWindow.handle, TRUE, FALSE));
	xfc->xi_event = FALSE;
	CHECK(releases == releasesBeforeXI + 1 && moves == checkpoint);
	CHECK(appWindow.local_move.state == LMS_TERMINATING);
	RAIL_LOCALMOVESIZE_ORDER endWithoutType = end;
	endWithoutType.moveSizeType = 0;
	CHECK(rail->ServerLocalMoveSize(rail, &endWithoutType) == CHANNEL_RC_OK);
	CHECK(translations == translationsBeforeXI && appWindow.local_move.state == LMS_NOT_ACTIVE);

	/* Failed pointer reads or input sends do not falsely finish a gesture. */
	appWindow.local_move.state = LMS_STARTING;
	pointerSuccess = False;
	CHECK(!xf_rail_end_local_move(xfc, &appWindow));
	CHECK(appWindow.local_move.state == LMS_STARTING);
	pointerSuccess = True;
	inputSuccess = FALSE;
	CHECK(!xf_rail_end_local_move(xfc, &appWindow));
	CHECK(appWindow.local_move.state == LMS_STARTING);
	inputSuccess = TRUE;
	CHECK(xf_rail_end_local_move(xfc, &appWindow));
	CHECK(rail->ServerLocalMoveSize(rail, &end) == CHANNEL_RC_OK);

	/* Unknown-margin keyboard moves and resizes stay server-driven instead of
	 * requesting an unsafe local interaction or emitting a zero-margin PDU. */
	const RAIL_LOCALMOVESIZE_ORDER start = { .windowId = (UINT32)appWindow.windowId,
		                                     .isMoveSizeStart = 1,
		                                     .moveSizeType = RAIL_WMSZ_RIGHT };
	CHECK(rail->ServerLocalMoveSize(rail, &start) == CHANNEL_RC_OK);
	CHECK(appWindow.local_move.state == LMS_NOT_ACTIVE && moves == checkpoint);
	input.type = MotionNotify;
	CHECK(!xf_event_suppress_events(xfc, &appWindow, &input));
	RAIL_LOCALMOVESIZE_ORDER keyboardStart = start;
	keyboardStart.moveSizeType = RAIL_WMSZ_KEYMOVE;
	CHECK(rail->ServerLocalMoveSize(rail, &keyboardStart) == CHANNEL_RC_OK);
	CHECK(appWindow.local_move.state == LMS_NOT_ACTIVE && moves == checkpoint);

	return TRUE;
out:
	return FALSE;
}

static BOOL test_resize_and_keyboard_completion(xfContext* xfc, XConfigureEvent* event)
{
	rdpWindowUpdate* windows = xfc->common.context.update->window;
	RailClientContext* rail = xfc->rail;
	const unsigned checkpoint = moves;
	const WINDOW_ORDER_INFO marginY = { .windowId = appWindow.windowId,
		                                .fieldFlags = WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y };
	const WINDOW_ORDER_INFO unchanged = { .windowId = appWindow.windowId };
	const WINDOW_STATE_ORDER empty = WINPR_C_ARRAY_INIT;
	const RAIL_LOCALMOVESIZE_ORDER end = { .windowId = (UINT32)appWindow.windowId,
		                                   .moveSizeType = RAIL_WMSZ_MOVE };
	XEvent input = WINPR_C_ARRAY_INIT;

	/* Explicit zero margins ARE usable, and survive later omitted fields. */
	CHECK(windows->WindowUpdate(&xfc->common.context, &marginY, &empty));
	CHECK(xf_rail_has_resize_margins(&appWindow));
	CHECK(windows->WindowUpdate(&xfc->common.context, &unchanged, &empty));
	CHECK(xf_rail_has_resize_margins(&appWindow));
	appWindow.windowMovePending = FALSE;
	appWindow.local_move.state = LMS_ACTIVE;
	appWindow.local_move.direction = NET_WM_MOVERESIZE_SIZE_RIGHT;
	appWindow.width = 530;
	appWindow.height = 269;
	CHECK(xf_rail_end_local_move(xfc, &appWindow));
	CHECK(moves == checkpoint + 1);
	CHECK(lastMove.right - lastMove.left == 530 && lastMove.bottom - lastMove.top == 269);
	CHECK(appWindow.windowWidth == 413 && appWindow.windowHeight == 229);
	CHECK(rail->ServerLocalMoveSize(rail, &end) == CHANNEL_RC_OK);

	/* Keyboard completion is explicit, not triggered by an arbitrary key. */
	appWindow.local_move.state = LMS_ACTIVE;
	appWindow.local_move.direction = NET_WM_MOVERESIZE_MOVE_KEYBOARD;
	appWindow.windowMovePending = FALSE;
	input.type = KeyPress;
	lookupKey = XK_Left;
	CHECK(!xf_event_suppress_events(xfc, &appWindow, &input));
	CHECK(appWindow.local_move.state == LMS_ACTIVE);
	lookupKey = XK_Return;
	CHECK(!xf_event_suppress_events(xfc, &appWindow, &input));
	CHECK(appWindow.local_move.state == LMS_TERMINATING && moves == checkpoint + 2);
	appWindow.local_move.state = LMS_ACTIVE;
	lookupKey = XK_Escape;
	CHECK(!xf_event_suppress_events(xfc, &appWindow, &input));
	CHECK(appWindow.local_move.state == LMS_TERMINATING && moves == checkpoint + 2);
	CHECK(rail->ServerLocalMoveSize(rail, &end) == CHANNEL_RC_OK);
	appWindow.width = 450;
	appWindow.height = 400;

	/* Invalid event sizes must not replace an otherwise valid snapshot. */
	event->width = 0;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(appWindow.width == 450 && appWindow.height == 400);

	return TRUE;
out:
	return FALSE;
}

static BOOL test_real_configure_snapshots(xfContext* xfc, XConfigureEvent* event)
{
	RailClientContext* rail = xfc->rail;
	const RAIL_LOCALMOVESIZE_ORDER end = { .windowId = (UINT32)appWindow.windowId,
		                                   .moveSizeType = RAIL_WMSZ_MOVE };

	/* Real-only reparented resizes must converge without a synthetic event.
	 * Query dimensions as well as position: this queued event is older than X11. */
	const unsigned movesBeforeRealOnly = moves;
	appWindow.local_move.state = LMS_NOT_ACTIVE;
	event->send_event = False;
	event->x = event->y = 0;
	event->width = 460;
	event->height = 410;
	rootX = 620;
	rootY = 170;
	currentWidth = 500;
	currentHeight = 420;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(appWindow.configureGeometryPending);
	CHECK(appWindow.width == 450 && appWindow.height == 400);
	CHECK(xf_rail_sync_configure_geometry(xfc, &appWindow));
	CHECK(!appWindow.configureGeometryPending);
	CHECK(appWindow.x == 620 && appWindow.y == 170);
	CHECK(appWindow.width == 500 && appWindow.height == 420);
	CHECK(moves == movesBeforeRealOnly);

	/* A synthetic snapshot cancels the queued query and keeps its own tuple. */
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	const unsigned queriesBeforeSynthetic = attributeQueries;
	event->send_event = True;
	event->x = 650;
	event->y = 190;
	event->width = 510;
	CHECK(xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(xf_rail_sync_configure_geometry(xfc, &appWindow));
	CHECK(attributeQueries == queriesBeforeSynthetic && appWindow.x == 650);
	CHECK(appWindow.width == 510 && !appWindow.configureGeometryPending);

	/* Failures and unstable reads preserve the previous complete snapshot;
	 * a later flush can retry, without spinning or accepting mixed geometry. */
	event->send_event = False;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	attributesSuccess = 0;
	CHECK(!xf_rail_sync_configure_geometry(xfc, &appWindow));
	attributesSuccess = 1;
	translateSuccess = False;
	CHECK(!xf_rail_sync_configure_geometry(xfc, &appWindow));
	translateSuccess = True;
	changingAttributes = TRUE;
	const unsigned queriesBeforeUnstable = attributeQueries;
	CHECK(!xf_rail_sync_configure_geometry(xfc, &appWindow));
	CHECK(attributeQueries == queriesBeforeUnstable + 4);
	CHECK(appWindow.x == 650 && appWindow.width == 510 && appWindow.configureGeometryPending);
	changingAttributes = FALSE;
	xf_rail_flush_configure_geometry(xfc);
	CHECK(!appWindow.configureGeometryPending && appWindow.x == 620 && appWindow.width == 500);
	CHECK(moves == movesBeforeRealOnly);

	/* Native resize completion flushes pending geometry before WINDOWMOVE,
	 * even if pointer release precedes the batch flush. */
	appWindow.local_move.state = LMS_ACTIVE;
	appWindow.local_move.direction = NET_WM_MOVERESIZE_SIZE_RIGHT;
	appWindow.windowMovePending = FALSE;
	currentWidth = 550;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	CHECK(xf_rail_end_local_move(xfc, &appWindow));
	CHECK(moves == movesBeforeRealOnly + 1 && appWindow.width == 550);
	CHECK(!appWindow.configureGeometryPending && appWindow.local_move.state == LMS_TERMINATING);
	CHECK(lastMove.right - lastMove.left == 550);
	CHECK(rail->ServerLocalMoveSize(rail, &end) == CHANNEL_RC_OK);
	/* At a normal event-batch boundary, a focused idle window reports the
	 * single checked current snapshot; another flush does not resend it. */
	xfc->focused = TRUE;
	rootX = 625;
	currentWidth = 560;
	CHECK(!xf_rail_update_configure_geometry(xfc, &appWindow, event));
	xf_rail_flush_configure_geometry(xfc);
	CHECK(appWindow.x == 625 && appWindow.width == 560 && !appWindow.configureGeometryPending);
	CHECK(moves == movesBeforeRealOnly + 2);
	xf_rail_flush_configure_geometry(xfc);
	CHECK(moves == movesBeforeRealOnly + 2);
	xfc->focused = FALSE;
	/* Unmapping cancels a queued query rather than probing a vanished window. */
	appWindow.configureGeometryPending = TRUE;
	appWindow.is_mapped = FALSE;
	const unsigned queriesBeforeUnmap = attributeQueries;
	xf_rail_flush_configure_geometry(xfc);
	CHECK(!appWindow.configureGeometryPending && attributeQueries == queriesBeforeUnmap);
	return TRUE;
out:
	return FALSE;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	int result = 1;
	BOOL railInitialized = FALSE;
	xfContext xfc = WINPR_C_ARRAY_INIT;
	Screen screen = WINPR_C_ARRAY_INIT;
	rdpWindowUpdate windows = WINPR_C_ARRAY_INIT;
	rdpUpdate update = { .window = &windows };
	RailClientContext rail = { .ClientWindowMove = sendMove };
	rdpGdi gdi = { .gfx = (RdpgfxClientContext*)1 };
	xfc.log = WLog_Get("com.freerdp.client.x11");
	xfc.screen = &screen;
	xfc.depth = 24;
	xfc.UseXThreads = TRUE;
	xfc.common.context.update = &update;
	xfc.common.context.gdi = &gdi;
	xfc.common.context.settings = freerdp_settings_new(0);
	CHECK(xfc.common.context.settings);
	CHECK(xf_rail_init(&xfc, &rail));
	railInitialized = TRUE;
	appWindow.windowId = 0x300d0;
	appWindow.xfc = &xfc;
	appWindow.surfaceId = UINT32_MAX;
	CHECK(HashTable_Insert(xfc.railWindows, &appWindow.windowId, &appWindow));
	appWindow.is_mapped = TRUE;
	appWindow.windowOffsetX = 48;
	appWindow.windowWidth = 395;
	appWindow.windowHeight = 375;
	XConfigureEvent event = { .send_event = True, .x = 48, .width = 395, .height = 375 };

	/* Keep the fixture and event across scenarios to exercise consecutive
	 * gestures and pending server replies in their original order. */
	CHECK(test_configure_snapshots(&xfc, &event));
	CHECK(test_server_reconciliation(&xfc, &event));
	CHECK(test_native_moves(&xfc, &event));
	CHECK(test_resize_and_keyboard_completion(&xfc, &event));
	CHECK(test_real_configure_snapshots(&xfc, &event));
	result = 0;
out:
	if (railInitialized)
	{
		/* The window is owned by the test, not by the RAIL window table. */
		HashTable_ValueObject(xfc.railWindows)->fnObjectFree = nullptr;
		xf_rail_uninit(&xfc, &rail);
	}
	appWindow.xfc = nullptr;
	rail.custom = nullptr;
	freerdp_settings_free(xfc.common.context.settings);
	return result;
}
