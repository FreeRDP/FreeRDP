/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 RAIL
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_X11_RAIL_H
#define FREERDP_CLIENT_X11_RAIL_H

#include <freerdp/client/rail.h>

#include <X11/X.h>
#include <X11/Xlib.h>

#include "xf_types.h"

enum xf_localmove_state
{
	LMS_NOT_ACTIVE,
	LMS_STARTING,
	LMS_ACTIVE,
	LMS_TERMINATING
};

struct xf_localmove
{
	int root_x;
	int root_y;
	int window_x;
	int window_y;
	/* Retain a received release while the WM geometry is still changing. */
	BOOL completionPending;
	int release_x;
	int release_y;
	enum xf_localmove_state state;
	int direction;
};
typedef struct xf_localmove xfLocalMove;

struct xf_app_window
{
	xfContext* xfc;

	int x;
	int y;
	int width;
	int height;
	char* title;

	UINT32 surfaceId;
	UINT64 windowId;
	UINT32 ownerWindowId;

	UINT32 dwStyle;
	UINT32 dwExStyle;
	UINT32 showState;
	BOOL windowStyleKnown;
	BOOL minMaxInfoKnown;
	RAIL_MINMAXINFO_ORDER minMaxInfo;

	INT32 clientOffsetX;
	INT32 clientOffsetY;
	UINT32 clientAreaWidth;
	UINT32 clientAreaHeight;

	INT32 windowOffsetX;
	INT32 windowOffsetY;
	INT32 windowClientDeltaX;
	INT32 windowClientDeltaY;
	UINT32 windowWidth;
	UINT32 windowHeight;
	UINT32 numWindowRects;
	RECTANGLE_16* windowRects;

	INT32 visibleOffsetX;
	INT32 visibleOffsetY;
	UINT32 numVisibilityRects;
	RECTANGLE_16* visibilityRects;

	UINT32 localWindowOffsetCorrX;
	UINT32 localWindowOffsetCorrY;

	UINT32 resizeMarginLeft;
	UINT32 resizeMarginTop;
	UINT32 resizeMarginRight;
	UINT32 resizeMarginBottom;
	/* Omitted fields do not advertise zero margins (notably Server 2016 dialogs). */
	BOOL resizeMarginXKnown;
	BOOL resizeMarginYKnown;
	BOOL unknownMarginWarning;
	UINT64 lastUnknownMarginReconcile;
	BOOL serverGeometryPending;
	BOOL visibilityShapeDeferred;
	/* Suppress post-release echoes until the drawing channel catches up. */
	UINT64 localMoveGeometryTime;
	/* A real reparented ConfigureNotify needs a current root/size snapshot. */
	BOOL configureGeometryPending;
	/* Deduplicate only the last successfully queued, still-unanswered move.
	 * A server geometry update ends this scope, including adjusted replies. */
	BOOL windowMovePending;
	UINT64 lastWindowMoveTime;
	RAIL_WINDOW_MOVE_ORDER lastWindowMove;

	GC gc;
	int shmid;
	Window handle;
	Window* xfwin;
	BOOL fullscreen;
	BOOL decorations;
	BOOL is_mapped;
	BOOL is_transient;
	xfLocalMove local_move;
	BYTE rail_state;
	BOOL maxVert;
	BOOL maxHorz;
	BOOL minimized;
	BOOL restoreGeometryPending;
	BOOL rail_ignore_configure;
	BOOL rail_fullscreen_normalizing;
	/* Surface updates were skipped while the X window was unmapped, the next
	 * paint has to cover the whole window. */
	BOOL surfaceStale;
	Pixmap pixmap;
	UINT32 pixmapWidth;
	UINT32 pixmapHeight;
	XImage* image;
};
typedef struct xf_app_window xfAppWindow;
typedef struct xf_rail_icon_cache xfRailIconCache;

WINPR_ATTR_NODISCARD
BOOL xf_rail_paint(xfContext* xfc, const RECTANGLE_16* rect);

BOOL xf_rail_paint_surface(xfContext* xfc, UINT64 windowId, const RECTANGLE_16* rect);

WINPR_ATTR_NODISCARD
BOOL xf_rail_send_client_system_command(xfContext* xfc, UINT64 windowId, UINT16 command);

WINPR_ATTR_NODISCARD
BOOL xf_rail_schedule_workarea(xfContext* xfc);

WINPR_ATTR_NODISCARD
BOOL xf_rail_send_activate(xfContext* xfc, Window xwindow, BOOL enabled);

/* Store a deferred repaint request in redraw, if supplied. Repaint only after
 * releasing the window lock; the return value reports geometry handling. */
BOOL xf_rail_adjust_position(xfContext* xfc, xfAppWindow* appWindow, BOOL* redraw);

BOOL xf_rail_has_resize_margins(const xfAppWindow* appWindow);

BOOL xf_rail_window_can_resize(const xfAppWindow* appWindow);

/* Return whether a surface repaint is required after releasing the window lock. */
WINPR_ATTR_NODISCARD
BOOL xf_rail_reconcile_window(xfContext* xfc, xfAppWindow* appWindow);

WINPR_ATTR_NODISCARD
BOOL xf_rail_update_configure_geometry(xfContext* xfc, xfAppWindow* appWindow,
                                       const XConfigureEvent* event);

BOOL xf_rail_sync_configure_geometry(xfContext* xfc, xfAppWindow* appWindow);
void xf_rail_flush_configure_geometry(xfContext* xfc);

BOOL xf_rail_end_local_move(xfContext* xfc, xfAppWindow* appWindow);

WINPR_ATTR_NODISCARD
BOOL xf_rail_enable_remoteapp_mode(xfContext* xfc);

WINPR_ATTR_NODISCARD
BOOL xf_rail_disable_remoteapp_mode(xfContext* xfc);

WINPR_ATTR_NODISCARD
xfAppWindow* xf_rail_add_window(xfContext* xfc, UINT64 id, INT32 x, INT32 y, UINT32 width,
                                UINT32 height, UINT32 surfaceId);

#define xf_rail_return_window(window, alreadyLocked) \
	xf_rail_return_windowFrom((window), (alreadyLocked), __FILE__, __func__, __LINE__)
void xf_rail_return_windowFrom(xfAppWindow* window, BOOL alreadyLocked, const char* file,
                               const char* fkt, size_t line);

#define xf_rail_get_window(xfc, id, alreadyLocked) \
	xf_rail_get_windowFrom((xfc), (id), (alreadyLocked), __FILE__, __func__, __LINE__)

WINPR_ATTR_MALLOC(xf_rail_return_windowFrom, 1)
xfAppWindow* xf_rail_get_windowFrom(xfContext* xfc, UINT64 id, BOOL alreadyLocked, const char* file,
                                    const char* fkt, size_t line);

WINPR_ATTR_NODISCARD
BOOL xf_rail_del_window(xfContext* xfc, UINT64 id);

int xf_rail_init(xfContext* xfc, RailClientContext* rail);

int xf_rail_uninit(xfContext* xfc, RailClientContext* rail);

#endif /* FREERDP_CLIENT_X11_RAIL_H */
