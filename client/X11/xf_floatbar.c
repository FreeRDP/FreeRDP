/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Windows
 *
 * Licensed under the Apache License, Version 2.0 (the "License");n
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/cursorfont.h>

#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/sysinfo.h>

#include "xf_floatbar.h"
#include "xf_utils.h"
#include "resource/close.xbm"
#include "resource/lock.xbm"
#include "resource/unlock.xbm"
#include "resource/minimize.xbm"
#include "resource/restore.xbm"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

#define FLOATBAR_HEIGHT 26
#define FLOATBAR_DEFAULT_WIDTH 576
#define FLOATBAR_MIN_WIDTH 200
#define FLOATBAR_BORDER 24
#define FLOATBAR_BUTTON_WIDTH 24
#define FLOATBAR_COLOR_BACKGROUND "RGB:31/6c/a9"
#define FLOATBAR_COLOR_BORDER "RGB:75/9a/c8"
#define FLOATBAR_COLOR_FOREGROUND "RGB:FF/FF/FF"

/* Pointer position (relative to the main window) at or above which the bar is shown */
#define FLOATBAR_SHOW_TRIGGER 10
/* Pointer position (relative to the main window) below which hiding is armed.
 * The gap between the two thresholds is hysteresis: the bar keeps its state there. */
#define FLOATBAR_HIDE_THRESHOLD (FLOATBAR_HEIGHT + 14)
/* The pointer has to stay below FLOATBAR_HIDE_THRESHOLD this long before the bar hides */
#define FLOATBAR_HIDE_DELAY_MS 300
/* Duration of a complete show or hide slide */
#define FLOATBAR_ANIM_DURATION_MS 130
/* Frame interval used while a slide is running */
#define FLOATBAR_FRAME_MS 16

#define XF_FLOATBAR_FLAG_POSITION_MASK 0x00C0
#define XF_FLOATBAR_FLAG_POSITION_LEFT 0x0040
#define XF_FLOATBAR_FLAG_POSITION_RIGHT 0x0080

#define XF_FLOATBAR_MODE_NONE 0
#define XF_FLOATBAR_MODE_DRAGGING 1
#define XF_FLOATBAR_MODE_RESIZE_LEFT 2
#define XF_FLOATBAR_MODE_RESIZE_RIGHT 3

#define XF_FLOATBAR_BUTTON_CLOSE 1
#define XF_FLOATBAR_BUTTON_RESTORE 2
#define XF_FLOATBAR_BUTTON_MINIMIZE 3
#define XF_FLOATBAR_BUTTON_LOCKED 4

typedef BOOL (*OnClick)(xfFloatbar*);

typedef struct
{
	int x;
	int y;
	int type;
	bool focus;
	bool clicked;
	OnClick onclick;
	Window handle;
	/* cached icon bitmap, rebuilt only when the icon changes (lock/unlock) */
	Pixmap pattern;
	const unsigned char* pattern_bits;
} xfFloatbarButton;

/* colors already allocated by xf_floatbar_get_color(), keyed by their rgb string */
typedef struct
{
	const char* rgb;
	unsigned long pixel;
} xfFloatbarColor;

struct xf_floatbar
{
	int x;
	int y;
	int width;
	int height;
	int mode;
	int last_motion_x_root;
	int last_motion_y_root;
	BOOL locked;
	xfFloatbarButton* buttons[4];
	Window handle;
	BOOL hasCursor;
	xfContext* xfc;
	DWORD flags;
	BOOL created;
	Window root_window;
	char* title;
	XFontSet fontSet;
	BOOL entered;
	/* slide animation state, positions are in window y coordinates */
	int target_y;
	BOOL animating;
	int anim_from_y;
	int anim_to_y;
	UINT64 anim_start_ms;
	/* time since the pointer went below FLOATBAR_HIDE_THRESHOLD, 0 if not armed */
	UINT64 hide_armed_ms;
	/* width the buttons and the background were last drawn for */
	int drawn_width;
	/* one entry per floatbar color: background, border, foreground */
	xfFloatbarColor colors[3];
	/* cursors created once in xf_floatbar_init_resources() */
	Cursor cursor_arrow;
	Cursor cursor_resize;
};

static void xf_floatbar_button_free(xfContext* xfc, xfFloatbarButton* button);
static void xf_floatbar_init_resources(xfFloatbar* floatbar);

WINPR_ATTR_MALLOC(xf_floatbar_button_free, 1)
static xfFloatbarButton* xf_floatbar_new_button(xfFloatbar* floatbar, int type);

WINPR_ATTR_NODISCARD
static BOOL xf_floatbar_button_onclick_close(xfFloatbar* floatbar)
{
	if (!floatbar)
		return FALSE;

	return freerdp_abort_connect_context(&floatbar->xfc->common.context);
}

WINPR_ATTR_NODISCARD
static BOOL xf_floatbar_button_onclick_minimize(xfFloatbar* floatbar)
{
	xfContext* xfc = nullptr;

	if (!floatbar || !floatbar->xfc)
		return FALSE;

	xfc = floatbar->xfc;
	xf_SetWindowMinimized(xfc, xfc->window);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL xf_floatbar_button_onclick_restore(xfFloatbar* floatbar)
{
	if (!floatbar)
		return FALSE;

	xf_toggle_fullscreen(floatbar->xfc);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL xf_floatbar_button_onclick_locked(xfFloatbar* floatbar)
{
	if (!floatbar)
		return FALSE;

	floatbar->locked = !((floatbar->locked));
	return xf_floatbar_hide_and_show(floatbar);
}

BOOL xf_floatbar_set_root_y(xfFloatbar* floatbar, int y)
{
	if (!floatbar)
		return FALSE;

	floatbar->last_motion_y_root = y;
	return TRUE;
}

static UINT64 xf_floatbar_now_ms(void)
{
	return winpr_GetTickCount64NS() / 1000000ULL;
}

/* Set the bar to a position immediately and cancel any running slide */
static void xf_floatbar_snap(xfFloatbar* floatbar, int y)
{
	WINPR_ASSERT(floatbar);
	floatbar->y = y;
	floatbar->target_y = y;
	floatbar->animating = FALSE;
	floatbar->hide_armed_ms = 0;
}

static int xf_floatbar_hidden_y(void)
{
	return -FLOATBAR_HEIGHT + 1;
}

/* Decide whether the bar should be shown or hidden from the last known pointer position.
 * Showing is immediate, hiding waits until the pointer stayed away for FLOATBAR_HIDE_DELAY_MS.
 */
static void xf_floatbar_update_target(xfFloatbar* floatbar, UINT64 now)
{
	WINPR_ASSERT(floatbar);

	if (floatbar->last_motion_y_root <= FLOATBAR_SHOW_TRIGGER)
	{
		floatbar->target_y = 0;
		floatbar->hide_armed_ms = 0;
	}
	else if (floatbar->last_motion_y_root > FLOATBAR_HIDE_THRESHOLD)
	{
		if (floatbar->hide_armed_ms == 0)
			floatbar->hide_armed_ms = now;

		if (now - floatbar->hide_armed_ms >= FLOATBAR_HIDE_DELAY_MS)
			floatbar->target_y = xf_floatbar_hidden_y();
	}
	else
	{
		/* between the thresholds: keep the current state */
		floatbar->hide_armed_ms = 0;
	}
}

/* Advance the slide to the position for time now. Uses time based interpolation with
 * ease-out when showing and ease-in when hiding, so the speed does not depend on how
 * often this is called.
 */
static void xf_floatbar_step_animation(xfFloatbar* floatbar, UINT64 now)
{
	int y = 0;
	xfContext* xfc = nullptr;

	WINPR_ASSERT(floatbar);
	xfc = floatbar->xfc;
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);

	if (!floatbar->animating)
	{
		if (floatbar->y == floatbar->target_y)
			return;

		floatbar->animating = TRUE;
		floatbar->anim_from_y = floatbar->y;
		floatbar->anim_to_y = floatbar->target_y;
		floatbar->anim_start_ms = now;
	}
	else if (floatbar->anim_to_y != floatbar->target_y)
	{
		/* target changed while sliding, restart from the current position */
		floatbar->anim_from_y = floatbar->y;
		floatbar->anim_to_y = floatbar->target_y;
		floatbar->anim_start_ms = now;
	}

	const UINT64 elapsed = now - floatbar->anim_start_ms;

	if (elapsed >= FLOATBAR_ANIM_DURATION_MS)
	{
		y = floatbar->anim_to_y;
		floatbar->animating = FALSE;
	}
	else
	{
		const double t = (double)elapsed / FLOATBAR_ANIM_DURATION_MS;
		double progress = 0.0;

		if (floatbar->anim_to_y > floatbar->anim_from_y)
		{
			/* showing: ease-out quartic, starts at full speed and settles softly */
			const double inv = 1.0 - t;
			const double inv2 = inv * inv;
			progress = 1.0 - inv2 * inv2;
		}
		else
		{
			/* hiding: ease-in cubic */
			progress = t * t * t;
		}

		const int delta = floatbar->anim_to_y - floatbar->anim_from_y;
		y = floatbar->anim_from_y + (int)((double)delta * progress);
	}

	if (y != floatbar->y)
	{
		floatbar->y = y;
		LogDynAndXMoveWindow(xfc->log, xfc->display, floatbar->handle, floatbar->x, floatbar->y);
		/* the caller waits for events, make sure the move reaches the server now */
		LogDynAndXFlush(xfc->log, xfc->display);
	}
}

BOOL xf_floatbar_hide_and_show(xfFloatbar* floatbar)
{
	if (!floatbar || !floatbar->xfc)
		return FALSE;

	if (!floatbar->created)
		return TRUE;

	if (floatbar->locked || (floatbar->mode != XF_FLOATBAR_MODE_NONE))
	{
		/* Locked or dragged bars do not move, stop any running slide */
		floatbar->animating = FALSE;
		floatbar->hide_armed_ms = 0;
		return TRUE;
	}

	const UINT64 now = xf_floatbar_now_ms();
	xf_floatbar_update_target(floatbar, now);
	xf_floatbar_step_animation(floatbar, now);
	return TRUE;
}

DWORD xf_floatbar_get_wait_timeout(xfFloatbar* floatbar)
{
	if (!floatbar || !floatbar->created)
		return INFINITE;

	if (floatbar->locked || (floatbar->mode != XF_FLOATBAR_MODE_NONE))
		return INFINITE;

	if (floatbar->animating)
		return FLOATBAR_FRAME_MS;

	/* a pending hide needs a wakeup even if no pointer event arrives */
	if ((floatbar->hide_armed_ms != 0) && (floatbar->target_y != xf_floatbar_hidden_y()))
	{
		const UINT64 now = xf_floatbar_now_ms();
		const UINT64 armed = now - floatbar->hide_armed_ms;

		if (armed >= FLOATBAR_HIDE_DELAY_MS)
			return FLOATBAR_FRAME_MS;

		return WINPR_ASSERTING_INT_CAST(DWORD, FLOATBAR_HIDE_DELAY_MS - armed);
	}

	return INFINITE;
}

WINPR_ATTR_NODISCARD
static BOOL create_floatbar(xfFloatbar* floatbar)
{
	xfContext* xfc = nullptr;
	Status status = 0;
	XWindowAttributes attr = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(floatbar);
	if (floatbar->created)
		return TRUE;

	xfc = floatbar->xfc;
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);

	status = LogDynAndXGetWindowAttributes(xfc->log, xfc->display, floatbar->root_window, &attr);
	if (status != 1)
		return FALSE;

	switch (floatbar->flags & XF_FLOATBAR_FLAG_POSITION_MASK)
	{
		case XF_FLOATBAR_FLAG_POSITION_LEFT:
			floatbar->x = 0;
			break;

		case XF_FLOATBAR_FLAG_POSITION_RIGHT:
			floatbar->x = attr.width - FLOATBAR_DEFAULT_WIDTH;
			break;

		default:
			floatbar->x = attr.width / 2 - FLOATBAR_DEFAULT_WIDTH / 2;
			break;
	}

	/* never start off screen, e.g. if the window is narrower than the floatbar */
	if (floatbar->x < 0)
		floatbar->x = 0;

	{
		int y = 0;

		if (((floatbar->flags & 0x0004) == 0) && !floatbar->locked)
			y = xf_floatbar_hidden_y();

		xf_floatbar_snap(floatbar, y);
	}

	/* create the window at its logical position, not at 0 and then move it */
	floatbar->handle =
	    LogDynAndXCreateWindow(xfc->log, xfc->display, floatbar->root_window, floatbar->x,
	                           floatbar->y, FLOATBAR_DEFAULT_WIDTH, FLOATBAR_HEIGHT, 0,
	                           CopyFromParent, InputOutput, CopyFromParent, 0, nullptr);
	floatbar->width = FLOATBAR_DEFAULT_WIDTH;
	floatbar->height = FLOATBAR_HEIGHT;
	floatbar->drawn_width = 0;
	floatbar->mode = XF_FLOATBAR_MODE_NONE;
	floatbar->buttons[0] = xf_floatbar_new_button(floatbar, XF_FLOATBAR_BUTTON_CLOSE);
	floatbar->buttons[1] = xf_floatbar_new_button(floatbar, XF_FLOATBAR_BUTTON_RESTORE);
	floatbar->buttons[2] = xf_floatbar_new_button(floatbar, XF_FLOATBAR_BUTTON_MINIMIZE);
	floatbar->buttons[3] = xf_floatbar_new_button(floatbar, XF_FLOATBAR_BUTTON_LOCKED);
	LogDynAndXSelectInput(xfc->log, xfc->display, floatbar->handle,
	                      ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
	                          FocusChangeMask | LeaveWindowMask | EnterWindowMask |
	                          StructureNotifyMask | PropertyChangeMask);
	xf_floatbar_init_resources(floatbar);
	floatbar->created = TRUE;
	return TRUE;
}

BOOL xf_floatbar_toggle_fullscreen(xfFloatbar* floatbar, bool fullscreen)
{
	int size = 0;
	bool visible = False;
	xfContext* xfc = nullptr;

	if (!floatbar || !floatbar->xfc)
		return FALSE;

	xfc = floatbar->xfc;
	WINPR_ASSERT(xfc->display);

	/* Only visible if enabled */
	if (floatbar->flags & 0x0001)
	{
		/* Visible if fullscreen and flag visible in fullscreen mode */
		visible |= ((floatbar->flags & 0x0010) != 0) && fullscreen;
		/* Visible if window and flag visible in window mode */
		visible |= ((floatbar->flags & 0x0020) != 0) && !fullscreen;
	}

	if (visible)
	{
		if (!create_floatbar(floatbar))
			return FALSE;

		LogDynAndXMapWindow(xfc->log, xfc->display, floatbar->handle);
		size = ARRAYSIZE(floatbar->buttons);

		for (int i = 0; i < size; i++)
		{
			xfFloatbarButton* button = floatbar->buttons[i];
			LogDynAndXMapWindow(xfc->log, xfc->display, button->handle);
		}

		/* If default is hidden (and not sticky) don't show on fullscreen state changes */
		if (((floatbar->flags & 0x0004) == 0) && !floatbar->locked)
			xf_floatbar_snap(floatbar, xf_floatbar_hidden_y());

		xf_floatbar_hide_and_show(floatbar);
	}
	else if (floatbar->created)
	{
		XUnmapSubwindows(xfc->display, floatbar->handle);
		LogDynAndXUnmapWindow(xfc->log, xfc->display, floatbar->handle);
	}

	return TRUE;
}

xfFloatbarButton* xf_floatbar_new_button(xfFloatbar* floatbar, int type)
{
	xfFloatbarButton* button = nullptr;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(floatbar->xfc);
	WINPR_ASSERT(floatbar->xfc->display);
	WINPR_ASSERT(floatbar->handle);

	button = (xfFloatbarButton*)calloc(1, sizeof(xfFloatbarButton));
	button->type = type;

	switch (type)
	{
		case XF_FLOATBAR_BUTTON_CLOSE:
			button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * type;
			button->onclick = xf_floatbar_button_onclick_close;
			break;

		case XF_FLOATBAR_BUTTON_RESTORE:
			button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * type;
			button->onclick = xf_floatbar_button_onclick_restore;
			break;

		case XF_FLOATBAR_BUTTON_MINIMIZE:
			button->x = floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * type;
			button->onclick = xf_floatbar_button_onclick_minimize;
			break;

		case XF_FLOATBAR_BUTTON_LOCKED:
			button->x = FLOATBAR_BORDER;
			button->onclick = xf_floatbar_button_onclick_locked;
			break;

		default:
			break;
	}

	button->y = 0;
	button->focus = FALSE;
	button->handle =
	    LogDynAndXCreateWindow(floatbar->xfc->log, floatbar->xfc->display, floatbar->handle,
	                           button->x, 0, FLOATBAR_BUTTON_WIDTH, FLOATBAR_BUTTON_WIDTH, 0,
	                           CopyFromParent, InputOutput, CopyFromParent, 0, nullptr);
	LogDynAndXSelectInput(floatbar->xfc->log, floatbar->xfc->display, button->handle,
	                      ExposureMask | ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
	                          LeaveWindowMask | EnterWindowMask | StructureNotifyMask);
	return button;
}

xfFloatbar* xf_floatbar_new(xfContext* xfc, Window window, const char* title, DWORD flags)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);
	WINPR_ASSERT(title);

	/* Floatbar not enabled */
	if ((flags & 0x0001) == 0)
		return nullptr;

	if (!xfc)
		return nullptr;

	/* Force disable with remote app */
	if (xfc->remote_app)
		return nullptr;

	xfFloatbar* floatbar = (xfFloatbar*)calloc(1, sizeof(xfFloatbar));

	if (!floatbar)
		return nullptr;

	floatbar->title = _strdup(title);

	if (!floatbar->title)
		goto fail;

	floatbar->root_window = window;
	floatbar->flags = flags;
	floatbar->xfc = xfc;
	floatbar->locked = (flags & 0x0002) != 0;
	xf_floatbar_toggle_fullscreen(floatbar, FALSE);

	{
		char** missingList = nullptr;
		int missingCount = 0;
		char* defString = nullptr;
		floatbar->fontSet = XCreateFontSet(floatbar->xfc->display, "-*-*-*-*-*-*-*-*-*-*-*-*-*-*",
		                                   &missingList, &missingCount, &defString);

		if (floatbar->fontSet == nullptr)
		{
			WLog_ERR(TAG, "Failed to create fontset");
		}
		XFreeStringList(missingList);
	}
	return floatbar;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	xf_floatbar_free(floatbar);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

WINPR_ATTR_NODISCARD
static unsigned long xf_floatbar_get_color(xfFloatbar* floatbar, char* rgb_value)
{
	XColor color;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(floatbar->xfc);
	WINPR_ASSERT(rgb_value);

	/* allocate each color once, painting then needs no round trip to the X server */
	for (size_t i = 0; i < ARRAYSIZE(floatbar->colors); i++)
	{
		const xfFloatbarColor* entry = &floatbar->colors[i];

		if (entry->rgb && (strcmp(entry->rgb, rgb_value) == 0))
			return entry->pixel;
	}

	Display* display = floatbar->xfc->display;
	WINPR_ASSERT(display);

	Colormap cmap = DefaultColormap(display, XDefaultScreen(display));
	XParseColor(display, cmap, rgb_value, &color);
	XAllocColor(display, cmap, &color);

	for (size_t i = 0; i < ARRAYSIZE(floatbar->colors); i++)
	{
		xfFloatbarColor* entry = &floatbar->colors[i];

		if (!entry->rgb)
		{
			entry->rgb = rgb_value;
			entry->pixel = color.pixel;
			break;
		}
	}

	return color.pixel;
}

/* Create the cursors once, so pointer motion does not create and free one each time */
static void xf_floatbar_init_resources(xfFloatbar* floatbar)
{
	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(floatbar->xfc);
	WINPR_ASSERT(floatbar->xfc->display);

	floatbar->cursor_arrow = XCreateFontCursor(floatbar->xfc->display, XC_arrow);
	floatbar->cursor_resize = XCreateFontCursor(floatbar->xfc->display, XC_sb_h_double_arrow);
}

static void xf_floatbar_event_expose(xfFloatbar* floatbar)
{
	GC gc = nullptr;
	GC shape_gc = nullptr;
	Pixmap pmap = 0;
	Pixmap buffer = 0;
	XPoint shape[5] = WINPR_C_ARRAY_INIT;
	XPoint border[5] = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(floatbar->xfc);

	Display* display = floatbar->xfc->display;
	WINPR_ASSERT(display);

	/* create the pixmap that we'll use for shaping the window */
	pmap = LogDynAndXCreatePixmap(floatbar->xfc->log, display, floatbar->handle,
	                              WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->width),
	                              WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->height), 1);
	gc = LogDynAndXCreateGC(floatbar->xfc->log, display, floatbar->handle, 0, 0);
	shape_gc = LogDynAndXCreateGC(floatbar->xfc->log, display, pmap, 0, 0);
	/* draw into an off-screen buffer and copy it once, avoids flicker */
	buffer = LogDynAndXCreatePixmap(floatbar->xfc->log, display, floatbar->handle,
	                                WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->width),
	                                WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->height),
	                                (unsigned int)DefaultDepth(display, XDefaultScreen(display)));
	/* points for drawing the floatbar */
	shape[0].x = 0;
	shape[0].y = 0;
	shape[1].x = WINPR_ASSERTING_INT_CAST(short, floatbar->width);
	shape[1].y = 0;
	shape[2].x = WINPR_ASSERTING_INT_CAST(short, shape[1].x - FLOATBAR_BORDER);
	shape[2].y = FLOATBAR_HEIGHT;
	shape[3].x = WINPR_ASSERTING_INT_CAST(short, shape[0].x + FLOATBAR_BORDER);
	shape[3].y = FLOATBAR_HEIGHT;
	shape[4].x = shape[0].x;
	shape[4].y = shape[0].y;
	/* points for drawing the border of the floatbar */
	border[0].x = shape[0].x;
	border[0].y = WINPR_ASSERTING_INT_CAST(short, shape[0].y - 1);
	border[1].x = WINPR_ASSERTING_INT_CAST(short, shape[1].x - 1);
	border[1].y = WINPR_ASSERTING_INT_CAST(short, shape[1].y - 1);
	border[2].x = shape[2].x;
	border[2].y = WINPR_ASSERTING_INT_CAST(short, shape[2].y - 1);
	border[3].x = WINPR_ASSERTING_INT_CAST(short, shape[3].x - 1);
	border[3].y = WINPR_ASSERTING_INT_CAST(short, shape[3].y - 1);
	border[4].x = border[0].x;
	border[4].y = border[0].y;
	/* Fill all pixels with 0 */
	LogDynAndXSetForeground(floatbar->xfc->log, display, shape_gc, 0);
	LogDynAndXFillRectangle(floatbar->xfc->log, display, pmap, shape_gc, 0, 0,
	                        WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->width),
	                        WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->height));
	/* Fill all pixels which should be shown with 1 */
	LogDynAndXSetForeground(floatbar->xfc->log, display, shape_gc, 1);
	XFillPolygon(display, pmap, shape_gc, shape, 5, 0, CoordModeOrigin);
	XShapeCombineMask(display, floatbar->handle, ShapeBounding, 0, 0, pmap, ShapeSet);
	/* draw the float bar */
	LogDynAndXSetForeground(floatbar->xfc->log, display, gc,
	                        xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_BACKGROUND));
	XFillPolygon(display, buffer, gc, shape, 4, 0, CoordModeOrigin);
	/* draw an border for the floatbar */
	LogDynAndXSetForeground(floatbar->xfc->log, display, gc,
	                        xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_BORDER));
	XDrawLines(display, buffer, gc, border, 5, CoordModeOrigin);
	/* draw the host name connected to (limit to maximum file name) */
	const size_t len = strnlen(floatbar->title, MAX_PATH);
	LogDynAndXSetForeground(floatbar->xfc->log, display, gc,
	                        xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_FOREGROUND));

	WINPR_ASSERT(len <= INT32_MAX / 2);
	const int fx = floatbar->width / 2 - (int)len * 2;
	if (floatbar->fontSet != nullptr)
	{
		XmbDrawString(display, buffer, floatbar->fontSet, gc, fx, 15, floatbar->title, (int)len);
	}
	else
	{
		XDrawString(display, buffer, gc, fx, 15, floatbar->title, (int)len);
	}

	LogDynAndXCopyArea(floatbar->xfc->log, display, buffer, floatbar->handle, gc, 0, 0,
	                   WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->width),
	                   WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->height), 0, 0);
	LogDynAndXFreePixmap(floatbar->xfc->log, display, buffer);
	LogDynAndXFreeGC(floatbar->xfc->log, display, gc);
	LogDynAndXFreeGC(floatbar->xfc->log, display, shape_gc);
}

WINPR_ATTR_NODISCARD
static xfFloatbarButton* xf_floatbar_get_button(xfFloatbar* floatbar, Window window)
{
	WINPR_ASSERT(floatbar);
	const size_t size = ARRAYSIZE(floatbar->buttons);

	for (size_t i = 0; i < size; i++)
	{
		xfFloatbarButton* button = floatbar->buttons[i];
		if (button->handle == window)
		{
			return button;
		}
	}

	return nullptr;
}

static void xf_floatbar_button_update_positon(xfFloatbar* floatbar)
{
	xfFloatbarButton* button = nullptr;
	WINPR_ASSERT(floatbar);
	xfContext* xfc = floatbar->xfc;
	const size_t size = ARRAYSIZE(floatbar->buttons);

	for (size_t i = 0; i < size; i++)
	{
		button = floatbar->buttons[i];

		switch (button->type)
		{
			case XF_FLOATBAR_BUTTON_CLOSE:
				button->x =
				    floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			case XF_FLOATBAR_BUTTON_RESTORE:
				button->x =
				    floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			case XF_FLOATBAR_BUTTON_MINIMIZE:
				button->x =
				    floatbar->width - FLOATBAR_BORDER - FLOATBAR_BUTTON_WIDTH * button->type;
				break;

			default:
				break;
		}

		WINPR_ASSERT(xfc);
		WINPR_ASSERT(xfc->display);
		LogDynAndXMoveWindow(xfc->log, xfc->display, button->handle, button->x, button->y);
	}

	floatbar->drawn_width = floatbar->width;
	xf_floatbar_event_expose(floatbar);
}

static void xf_floatbar_button_event_expose(xfFloatbar* floatbar, Window window)
{
	xfFloatbarButton* button = xf_floatbar_get_button(floatbar, window);
	static unsigned char* bits;
	GC gc = nullptr;
	Pixmap pattern = 0;
	xfContext* xfc = floatbar->xfc;

	if (!button)
		return;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);
	WINPR_ASSERT(xfc->window);

	gc = LogDynAndXCreateGC(xfc->log, xfc->display, button->handle, 0, 0);
	floatbar = xfc->window->floatbar;
	WINPR_ASSERT(floatbar);

	switch (button->type)
	{
		case XF_FLOATBAR_BUTTON_CLOSE:
			bits = close_bits;
			break;

		case XF_FLOATBAR_BUTTON_RESTORE:
			bits = restore_bits;
			break;

		case XF_FLOATBAR_BUTTON_MINIMIZE:
			bits = minimize_bits;
			break;

		case XF_FLOATBAR_BUTTON_LOCKED:
			if (floatbar->locked)
				bits = lock_bits;
			else
				bits = unlock_bits;

			break;

		default:
			break;
	}

	/* rebuild the icon bitmap only when the icon itself changed */
	if (button->pattern_bits != bits)
	{
		if (button->pattern)
			LogDynAndXFreePixmap(xfc->log, xfc->display, button->pattern);

		button->pattern = XCreateBitmapFromData(xfc->display, button->handle, (const char*)bits,
		                                        FLOATBAR_BUTTON_WIDTH, FLOATBAR_BUTTON_WIDTH);
		button->pattern_bits = bits;
	}

	pattern = button->pattern;

	if (!(button->focus))
		LogDynAndXSetForeground(floatbar->xfc->log, xfc->display, gc,
		                        xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_BACKGROUND));
	else
		LogDynAndXSetForeground(floatbar->xfc->log, xfc->display, gc,
		                        xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_BORDER));

	LogDynAndXSetBackground(xfc->log, xfc->display, gc,
	                        xf_floatbar_get_color(floatbar, FLOATBAR_COLOR_FOREGROUND));
	XCopyPlane(xfc->display, pattern, button->handle, gc, 0, 0, FLOATBAR_BUTTON_WIDTH,
	           FLOATBAR_BUTTON_WIDTH, 0, 0, 1);
	LogDynAndXFreeGC(xfc->log, xfc->display, gc);
}

static void xf_floatbar_button_event_buttonpress(xfFloatbar* floatbar, const XButtonEvent* event)
{
	WINPR_ASSERT(event);
	xfFloatbarButton* button = xf_floatbar_get_button(floatbar, event->window);

	if (button)
		button->clicked = TRUE;
}

static void xf_floatbar_button_event_buttonrelease(xfFloatbar* floatbar, const XButtonEvent* event)
{
	xfFloatbarButton* button = nullptr;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(event);

	button = xf_floatbar_get_button(floatbar, event->window);

	if (button)
	{
		if (button->clicked)
			button->onclick(floatbar);
		button->clicked = FALSE;
	}
}

static void xf_floatbar_event_buttonpress(xfFloatbar* floatbar, const XButtonEvent* event)
{
	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(event);

	switch (event->button)
	{
		case Button1:
			if (event->x <= FLOATBAR_BORDER)
				floatbar->mode = XF_FLOATBAR_MODE_RESIZE_LEFT;
			else if (event->x >= (floatbar->width - FLOATBAR_BORDER))
				floatbar->mode = XF_FLOATBAR_MODE_RESIZE_RIGHT;
			else
				floatbar->mode = XF_FLOATBAR_MODE_DRAGGING;

			break;

		default:
			break;
	}
}

static void xf_floatbar_event_buttonrelease(xfFloatbar* floatbar, const XButtonEvent* event)
{
	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(event);

	switch (event->button)
	{
		case Button1:
			floatbar->mode = XF_FLOATBAR_MODE_NONE;
			break;

		default:
			break;
	}
}

static void xf_floatbar_resize(xfFloatbar* floatbar, const XMotionEvent* event)
{
	int x = 0;
	int width = 0;
	int movement = 0;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(event);

	xfContext* xfc = floatbar->xfc;
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);

	/* calculate movement which happened on the root window */
	movement = event->x_root - floatbar->last_motion_x_root;

	/* set x and width depending if movement happens on the left or right  */
	if (floatbar->mode == XF_FLOATBAR_MODE_RESIZE_LEFT)
	{
		x = floatbar->x + movement;
		width = floatbar->width + movement * -1;
	}
	else
	{
		x = floatbar->x;
		width = floatbar->width + movement;
	}

	/* only resize and move window if still above minimum width */
	if (FLOATBAR_MIN_WIDTH < width)
	{
		LogDynAndXMoveResizeWindow(xfc->log, xfc->display, floatbar->handle, x, 0,
		                           WINPR_ASSERTING_INT_CAST(uint32_t, width),
		                           WINPR_ASSERTING_INT_CAST(uint32_t, floatbar->height));
		floatbar->x = x;
		floatbar->width = width;
	}
}

static void xf_floatbar_dragging(xfFloatbar* floatbar, const XMotionEvent* event)
{
	int x = 0;
	int movement = 0;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(event);
	xfContext* xfc = floatbar->xfc;
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->window);
	WINPR_ASSERT(xfc->display);

	/* calculate movement and new x position */
	movement = event->x_root - floatbar->last_motion_x_root;
	x = floatbar->x + movement;

	/* do nothing if floatbar would be moved out of the window */
	if (x < 0 || (x + floatbar->width) > xfc->window->width)
		return;

	/* move window to new x position */
	LogDynAndXMoveWindow(xfc->log, xfc->display, floatbar->handle, x, 0);
	/* update struct values for the next event */
	floatbar->last_motion_x_root = floatbar->last_motion_x_root + movement;
	floatbar->x = x;
}

static void xf_floatbar_event_motionnotify(xfFloatbar* floatbar, const XMotionEvent* event)
{
	int mode = 0;
	Cursor cursor = 0;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(event);

	xfContext* xfc = floatbar->xfc;
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);

	mode = floatbar->mode;
	cursor = floatbar->cursor_arrow;

	if ((event->state & Button1Mask) && (mode > XF_FLOATBAR_MODE_DRAGGING))
	{
		xf_floatbar_resize(floatbar, event);
	}
	else if ((event->state & Button1Mask) && (mode == XF_FLOATBAR_MODE_DRAGGING))
	{
		xf_floatbar_dragging(floatbar, event);
	}
	else
	{
		if (event->x <= FLOATBAR_BORDER || event->x >= floatbar->width - FLOATBAR_BORDER)
			cursor = floatbar->cursor_resize;
	}

	XDefineCursor(xfc->display, xfc->window->handle, cursor);
	floatbar->last_motion_x_root = event->x_root;
}

static void xf_floatbar_button_event_focusin(xfFloatbar* floatbar, const XAnyEvent* event)
{
	xfFloatbarButton* button = nullptr;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(event);

	button = xf_floatbar_get_button(floatbar, event->window);

	if (button)
	{
		button->focus = TRUE;
		xf_floatbar_button_event_expose(floatbar, event->window);
	}
}

static void xf_floatbar_button_event_focusout(xfFloatbar* floatbar, const XAnyEvent* event)
{
	xfFloatbarButton* button = nullptr;

	WINPR_ASSERT(floatbar);
	WINPR_ASSERT(event);

	button = xf_floatbar_get_button(floatbar, event->window);

	if (button)
	{
		button->focus = FALSE;
		xf_floatbar_button_event_expose(floatbar, event->window);
	}
}

static void xf_floatbar_event_focusout(xfFloatbar* floatbar)
{
	WINPR_ASSERT(floatbar);
	xfContext* xfc = floatbar->xfc;
	WINPR_ASSERT(xfc);

	if (xfc->pointer)
	{
		WINPR_ASSERT(xfc->window);
		WINPR_ASSERT(xfc->pointer);
		XDefineCursor(xfc->display, xfc->window->handle, xfc->pointer->cursor);
	}
}

/* y is relative to the floatbar window, convert it to the main window */
static void xf_floatbar_set_pointer_y(xfFloatbar* floatbar, int y)
{
	WINPR_ASSERT(floatbar);
	floatbar->last_motion_y_root = y + floatbar->y;
}

BOOL xf_floatbar_check_event(xfFloatbar* floatbar, const XEvent* event)
{
	if (!floatbar || !floatbar->xfc || !event)
		return FALSE;

	if (!floatbar->created)
		return FALSE;

	if (event->xany.window == floatbar->handle)
		return TRUE;

	size_t size = ARRAYSIZE(floatbar->buttons);

	for (size_t i = 0; i < size; i++)
	{
		const xfFloatbarButton* button = floatbar->buttons[i];

		if (event->xany.window == button->handle)
			return TRUE;
	}

	return FALSE;
}

BOOL xf_floatbar_event_process(xfFloatbar* floatbar, const XEvent* event)
{
	if (!floatbar || !floatbar->xfc || !event)
		return FALSE;

	if (!floatbar->created)
		return FALSE;

	switch (event->type)
	{
		case Expose:
			if (event->xexpose.window == floatbar->handle)
				xf_floatbar_event_expose(floatbar);
			else
				xf_floatbar_button_event_expose(floatbar, event->xexpose.window);

			break;

		case MotionNotify:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_set_pointer_y(floatbar, event->xmotion.y);

			xf_floatbar_event_motionnotify(floatbar, &event->xmotion);
			break;

		case ButtonPress:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_buttonpress(floatbar, &event->xbutton);
			else
				xf_floatbar_button_event_buttonpress(floatbar, &event->xbutton);

			break;

		case ButtonRelease:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_buttonrelease(floatbar, &event->xbutton);
			else
				xf_floatbar_button_event_buttonrelease(floatbar, &event->xbutton);

			break;

		case EnterNotify:
			floatbar->entered = TRUE;
			if (event->xany.window == floatbar->handle)
				xf_floatbar_set_pointer_y(floatbar, event->xcrossing.y);

			WINPR_FALLTHROUGH
		case FocusIn:
			if (event->xany.window != floatbar->handle)
				xf_floatbar_button_event_focusin(floatbar, &event->xany);

			break;

		case LeaveNotify:
			floatbar->entered = FALSE;
			WINPR_FALLTHROUGH
		case FocusOut:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_event_focusout(floatbar);
			else
				xf_floatbar_button_event_focusout(floatbar, &event->xany);

			break;

		case ConfigureNotify:
			/* a plain move (the slide) does not change the layout, only a resize does */
			if ((event->xany.window == floatbar->handle) &&
			    (event->xconfigure.width != floatbar->drawn_width))
				xf_floatbar_button_update_positon(floatbar);

			break;

		case PropertyNotify:
			if (event->xany.window == floatbar->handle)
				xf_floatbar_button_update_positon(floatbar);

			break;

		default:
			break;
	}

	return floatbar->handle == event->xany.window;
}

void xf_floatbar_button_free(xfContext* xfc, xfFloatbarButton* button)
{
	if (!button)
		return;

	if (button->handle)
	{
		WINPR_ASSERT(xfc);
		WINPR_ASSERT(xfc->display);
		LogDynAndXUnmapWindow(xfc->log, xfc->display, button->handle);
		LogDynAndXDestroyWindow(xfc->log, xfc->display, button->handle);
	}

	if (button->pattern)
		LogDynAndXFreePixmap(xfc->log, xfc->display, button->pattern);

	free(button);
}

void xf_floatbar_free(xfFloatbar* floatbar)
{
	size_t size = 0;
	xfContext* xfc = nullptr;

	if (!floatbar)
		return;

	free(floatbar->title);
	xfc = floatbar->xfc;
	WINPR_ASSERT(xfc);

	size = ARRAYSIZE(floatbar->buttons);

	for (size_t i = 0; i < size; i++)
	{
		xf_floatbar_button_free(xfc, floatbar->buttons[i]);
		floatbar->buttons[i] = nullptr;
	}

	if (floatbar->handle)
	{
		WINPR_ASSERT(xfc->display);
		LogDynAndXUnmapWindow(xfc->log, xfc->display, floatbar->handle);
		LogDynAndXDestroyWindow(xfc->log, xfc->display, floatbar->handle);
	}

	if (floatbar->created)
	{
		XFreeCursor(xfc->display, floatbar->cursor_arrow);
		XFreeCursor(xfc->display, floatbar->cursor_resize);
	}

	free(floatbar);
}

BOOL xf_floatbar_is_locked(xfFloatbar* floatbar)
{
	if (!floatbar)
		return FALSE;
	return floatbar->mode != XF_FLOATBAR_MODE_NONE;
}

BOOL xf_floatbar_is_window(xfFloatbar* floatbar, Window window)
{
	if (!floatbar)
		return FALSE;
	if (floatbar->handle == window)
		return TRUE;
	return floatbar->entered;
}

BOOL xfc_is_floatbar_window(xfContext* xfc, Window window)
{
	if (!xfc || !xfc->window)
		return FALSE;
	return xf_floatbar_is_window(xfc->window->floatbar, window);
}
