/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include <freerdp/config.h>
#include <stdio.h>
#include <freerdp/window.h>
#include <winpr/collections.h>
#include "../xfreerdp.h"
#include "../xf_rail.h"
#include "../xf_window.h"

static xfAppWindow appWindow;
static unsigned translations;
static unsigned moves;
static int rootX = 700;
static int rootY = 245;
static Bool translateSuccess = True;
static Window windowParent = 1;
static RAIL_WINDOW_MOVE_ORDER lastMove;

#define CHECK(condition)                                         \
	do                                                           \
	{                                                            \
		if (!(condition))                                        \
		{                                                        \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			return 1;                                            \
		}                                                        \
	} while (0)

/* Exercise the actual configure/RAIL logic without an X server or RDP server. */
Status XQueryTree(Display* display, Window window, Window* root, Window* parent,
                  Window** children, unsigned int* count)
{
	WINPR_UNUSED(display);
	WINPR_UNUSED(window);
	*root = None;
	*parent = windowParent;
	*children = nullptr;
	*count = 0;
	return 1;
}

Bool XTranslateCoordinates(Display* display, Window src, Window dst, int sx, int sy,
                            int* x, int* y, Window* child)
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

Pixmap XCreatePixmap(Display* display, Drawable drawable, unsigned int width,
                      unsigned int height, unsigned int depth)
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
	WINPR_UNUSED(display);
	WINPR_UNUSED(window);
	WINPR_UNUSED(x);
	WINPR_UNUSED(y);
	return 1;
}

int XMoveResizeWindow(Display* display, Window window, int x, int y,
                       unsigned int width, unsigned int height)
{
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
	return CHANNEL_RC_OK;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	xfContext xfc = { 0 };
	Screen screen = { 0 };
	rdpWindowUpdate windows = { 0 };
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
	appWindow.windowId = 0x300d0;
	appWindow.xfc = &xfc;
	appWindow.surfaceId = UINT32_MAX;
	CHECK(HashTable_Insert(xfc.railWindows, &appWindow.windowId, &appWindow));
	appWindow.is_mapped = TRUE;
	appWindow.windowOffsetX = 48;
	appWindow.windowWidth = 395;
	appWindow.windowHeight = 375;
	XConfigureEvent event = { .send_event = True, .x = 48, .width = 395, .height = 375 };

	/* A queued server echo must use its snapshot, not a newer XTranslate result. */
	CHECK(xf_rail_update_configure_geometry(&xfc, &appWindow, &event));
	CHECK(translations == 0);
	CHECK(appWindow.x == 48 && appWindow.y == 0);
	CHECK(xf_rail_adjust_position(&xfc, &appWindow));
	CHECK(moves == 0);

	/* A parent-relative server echo must not query a transient root position
	 * between the WM's client/frame moves. Its synthetic notification follows. */
	event.send_event = False;
	event.x = 27;
	event.y = 6;
	CHECK(!xf_rail_update_configure_geometry(&xfc, &appWindow, &event));
	CHECK(translations == 0 && moves == 0);
	CHECK(appWindow.x == 48 && appWindow.y == 0);

	/* An active Windows-requested move must keep tracking real notifications. */
	appWindow.local_move.state = LMS_ACTIVE;
	CHECK(xf_rail_update_configure_geometry(&xfc, &appWindow, &event));
	CHECK(translations == 1 && appWindow.x == rootX && appWindow.y == rootY);
	CHECK(!xf_rail_adjust_position(&xfc, &appWindow));
	CHECK(moves == 0);
	appWindow.local_move.state = LMS_NOT_ACTIVE;

	/* A non-reparenting WM's real root-space move must not be deferred. */
	windowParent = None;
	CHECK(xf_rail_update_configure_geometry(&xfc, &appWindow, &event));
	CHECK(translations == 2 && appWindow.x == rootX && appWindow.y == rootY);
	windowParent = 1;
	translations = 0;

	/* Real parent-relative events still translate and report genuine local moves. */
	event.send_event = False;
	event.x = 0;
	event.width = 450;
	event.height = 400;
	appWindow.resizeMarginLeft = appWindow.resizeMarginRight = 7;
	appWindow.resizeMarginBottom = 7;
	CHECK(xf_rail_update_configure_geometry(&xfc, &appWindow, &event));
	CHECK(translations == 1);
	CHECK(xf_rail_adjust_position(&xfc, &appWindow));
	CHECK(moves == 1);
	CHECK(lastMove.left == 693 && lastMove.top == 245);
	CHECK(lastMove.right == 1157 && lastMove.bottom == 652);

	/* A failed translation must not overwrite the last known local geometry. */
	translateSuccess = False;
	rootX = 9999;
	CHECK(!xf_rail_update_configure_geometry(&xfc, &appWindow, &event));
	CHECK(appWindow.x == 700 && appWindow.y == 245);
	translateSuccess = True;

	/* Resizing back to the last server size is still a real size change when
	 * a previous local resize has not been acknowledged by Windows yet. */
	rootX = 700;
	event.width = 395;
	event.height = 375;
	CHECK(xf_rail_update_configure_geometry(&xfc, &appWindow, &event));
	CHECK(appWindow.width == 395 && appWindow.height == 375);
	event.width = 450;
	event.height = 400;
	CHECK(xf_rail_update_configure_geometry(&xfc, &appWindow, &event));

	/* Prevent the signed-16-bit wrap observed during the runaway position loop. */
	appWindow.x = 32760;
	CHECK(!xf_rail_adjust_position(&xfc, &appWindow));
	CHECK(moves == 1);
	appWindow.x = -32000;
	CHECK(xf_rail_adjust_position(&xfc, &appWindow));
	CHECK(moves == 2 && lastMove.left == -32007);

	/* Execute the registered server callback: successive surface sizes must all
	 * apply, including the initial title strip and its immediate expansion. */
	const WINDOW_ORDER_INFO info = { .windowId = appWindow.windowId,
	                                 .fieldFlags = WINDOW_ORDER_FIELD_WND_SIZE };
	const UINT32 sizes[][2] = { { 395, 375 }, { 381, 368 }, { 122, 32 }, { 409, 382 } };
	for (size_t i = 0; i < ARRAYSIZE(sizes); i++)
	{
		const WINDOW_STATE_ORDER state = { .windowWidth = sizes[i][0],
		                                   .windowHeight = sizes[i][1] };
		CHECK(windows.WindowUpdate(&xfc.common.context, &info, &state));
		CHECK(appWindow.windowWidth == sizes[i][0] && appWindow.windowHeight == sizes[i][1]);
		CHECK(appWindow.width == (int)sizes[i][0] && appWindow.height == (int)sizes[i][1]);
	}
	HashTable_ValueObject(xfc.railWindows)->fnObjectFree = nullptr;
	xf_rail_uninit(&xfc, &rail);
	freerdp_settings_free(xfc.common.context.settings);
	return 0;
}
