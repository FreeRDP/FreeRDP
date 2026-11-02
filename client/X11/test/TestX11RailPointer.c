/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include "../xfreerdp.h"
#include "../xf_rail.h"

#include <stdio.h>

static UINT move_count = 0;
static RAIL_WINDOW_MOVE_ORDER last_move = { 0 };

static UINT capture_move(RailClientContext* context, const RAIL_WINDOW_MOVE_ORDER* move)
{
	(void)context;
	move_count++;
	last_move = *move;
	return CHANNEL_RC_OK;
}

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	xfAppWindow window = { 0 };
	window.width = 1280;
	window.height = 800;
	window.windowWidth = 1280;
	window.windowHeight = 800;
	window.dwStyle = WS_MAXIMIZE;
	/* Local WM placement differs from the source origin. */
	window.x = 639;
	window.y = 222;
	int x = 600;
	int y = 10;
	xf_rail_pointer_to_desktop(&window, 1239, 232, &x, &y);
	if (x != 600 || y != 10)
		return 1;
	window.windowOffsetX = 200;
	window.windowOffsetY = 100;
	x = 25;
	y = 30;
	xf_rail_pointer_to_desktop(&window, 225, 130, &x, &y);
	if (x != 225 || y != 130)
		return 2;
	window.dwStyle = WS_MAXIMIZE;
	window.windowOffsetX = -8;
	window.windowOffsetY = -8;
	window.resizeMarginLeft = 8;
	window.resizeMarginTop = 8;
	window.windowWidth = 1296;
	window.windowHeight = 816;
	x = 25;
	y = 30;
	xf_rail_pointer_to_desktop(&window, 664, 252, &x, &y);
	if (x != 25 || y != 30)
		return 3;
	window.dwStyle = 0;
	window.windowOffsetX = INT32_MAX;
	window.windowOffsetY = INT32_MIN;
	x = INT_MAX;
	y = INT_MIN;
	xf_rail_pointer_to_desktop(&window, INT_MAX, INT_MIN, &x, &y);
	if (x != UINT16_MAX || y != 0)
		return 4;
	window.windowOffsetX = 0;
	window.windowOffsetY = 0;
	x = 933;
	y = 54;
	if (!xf_rail_popup_to_local(&window, 639, 222, &x, &y) || x != 1572 || y != 276)
		return 5;
	window.dwStyle = WS_POPUP;
	window.dwExStyle = WS_EX_NOACTIVATE;
	window.windowOffsetX = 933;
	window.windowOffsetY = 54;
	x = 10;
	y = 20;
	xf_rail_pointer_to_desktop(&window, 1582, 296, &x, &y);
	if (x != 943 || y != 74)
		return 6;
	window.windowOffsetX = INT32_MIN;
	x = INT_MAX;
	y = 0;
	if (xf_rail_popup_to_local(&window, INT_MAX, 0, &x, &y) || x != INT_MAX || y != 0)
		return 7;
	/* Monotonic event-root positions must survive lagging/overtaking RAIL metadata. */
	window.dwStyle = 0;
	window.dwExStyle = 0;
	const struct
	{
		int rootX, localX, serverX;
	} drag[] = { { 520, 415, 105 }, { 525, 420, 132 }, { 530, 410, 116 },
		         { 750, 400, 350 }, { 950, 600, 480 }, { 1150, 800, 600 } };
	for (size_t i = 0; i < sizeof(drag) / sizeof(drag[0]); i++)
	{
		window.windowOffsetX = drag[i].serverX;
		window.windowOffsetY = 100;
		x = drag[i].localX;
		y = 10;
		xf_rail_pointer_to_desktop(&window, drag[i].rootX, 110, &x, &y);
		if (x != drag[i].rootX || y != 110)
		{
			fprintf(stderr, "Queued drag event %zu: got %d,%d expected %d,110\n", i, x, y,
			        drag[i].rootX);
			return 8;
		}
	}
	/* Superseded configure notifications must not echo an obsolete move. */
	window.rail_configure.pending = TRUE;
	window.rail_configure.x = 300;
	window.rail_configure.y = 150;
	window.rail_configure.width = 900;
	window.rail_configure.height = 600;
	window.x = 280;
	window.y = 150;
	window.width = 900;
	window.height = 600;
	xfContext xfc = { 0 };
	RailClientContext rail = { 0 };
	xfc.rail = &rail;
	rail.ClientWindowMove = capture_move;
	window.is_mapped = TRUE;
	window.rail_state = WINDOW_SHOW;
	if (xf_rail_adjust_position(&xfc, &window) || move_count != 0)
		return 14;
	if (xf_rail_configure_ack(&window) || !window.rail_configure.pending)
		return 9;
	window.rail_configure.x = 500;
	window.x = 300;
	if (xf_rail_configure_ack(&window))
		return 10;
	window.x = 500;
	window.height = 590;
	if (xf_rail_configure_ack(&window))
		return 11;
	window.height = 600;
	if (!xf_rail_configure_ack(&window) || window.rail_configure.pending)
		return 12;
	window.x = 510;
	if (!xf_rail_configure_ack(&window))
		return 13;
	window.windowOffsetX = 500;
	window.windowOffsetY = 150;
	window.windowWidth = 900;
	window.windowHeight = 600;
	window.resizeMarginLeft = window.resizeMarginRight = 0;
	window.resizeMarginTop = window.resizeMarginBottom = 0;
	if (!xf_rail_adjust_position(&xfc, &window) || move_count != 1 || last_move.left != 510 ||
	    last_move.top != 150 || last_move.right != 1410 || last_move.bottom != 750)
		return 15;
	return 0;
}
