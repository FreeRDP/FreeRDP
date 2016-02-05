/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Windows
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

#ifndef __XF_WINDOW_H
#define __XF_WINDOW_H

#include <X11/Xlib.h>

#include <freerdp/freerdp.h>

typedef struct xf_app_window xfAppWindow;

typedef struct xf_localmove xfLocalMove;
typedef struct xf_window xfWindow;

#include "xf_client.h"
#include "xfreerdp.h"

// Extended ICCM flags http://standards.freedesktop.org/wm-spec/wm-spec-latest.html
#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8   /* movement only */
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9   /* size via keyboard */
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10   /* move via keyboard */
#define _NET_WM_MOVERESIZE_CANCEL           11   /* cancel operation */

#define _NET_WM_STATE_REMOVE 0 /* remove/unset property */
#define _NET_WM_STATE_ADD 1 /* add/set property */
#define _NET_WM_STATE_TOGGLE 2 /* toggle property */

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
	enum xf_localmove_state state;
	int direction;
};

struct xf_window
{
	GC gc;
	int left;
	int top;
	int right;
	int bottom;
	int width;
	int height;
	int shmid;
	Window handle;
	Window* xfwin;
	BOOL decorations;
	BOOL is_mapped;
	BOOL is_transient;
};

struct xf_app_window
{
	xfContext* xfc;

	int x;
	int y;
	int width;
	int height;
	char* title;

	UINT32 windowId;
	UINT32 ownerWindowId;

	UINT32 dwStyle;
	UINT32 dwExStyle;
	UINT32 showState;

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
	BOOL rail_ignore_configure;
};

void xf_ewmhints_init(xfContext* xfc);

BOOL xf_GetCurrentDesktop(xfContext* xfc);
BOOL xf_GetWorkArea(xfContext* xfc);

void xf_SetWindowFullscreen(xfContext* xfc, xfWindow* window, BOOL fullscreen);
void xf_SetWindowDecorations(xfContext* xfc, Window window, BOOL show);
void xf_SetWindowUnlisted(xfContext* xfc, Window window);

xfWindow* xf_CreateDesktopWindow(xfContext* xfc, char* name, int width, int height);
void xf_ResizeDesktopWindow(xfContext* xfc, xfWindow* window, int width, int height);
void xf_DestroyDesktopWindow(xfContext* xfc, xfWindow* window);

BOOL xf_GetWindowProperty(xfContext* xfc, Window window, Atom property, int length,
		unsigned long* nitems, unsigned long* bytes, BYTE** prop);
void xf_SendClientEvent(xfContext* xfc, Window window, Atom atom, unsigned int numArgs, ...);

int xf_AppWindowInit(xfContext* xfc, xfAppWindow* appWindow);
void xf_SetWindowText(xfContext* xfc, xfAppWindow* appWindow, char* name);
void xf_MoveWindow(xfContext* xfc, xfAppWindow* appWindow, int x, int y, int width, int height);
void xf_ShowWindow(xfContext* xfc, xfAppWindow* appWindow, BYTE state);
//void xf_SetWindowIcon(xfContext* xfc, xfAppWindow* appWindow, rdpIcon* icon);
void xf_SetWindowRects(xfContext* xfc, xfAppWindow* appWindow, RECTANGLE_16* rects, int nrects);
void xf_SetWindowVisibilityRects(xfContext* xfc, xfAppWindow* appWindow, UINT32 rectsOffsetX, UINT32 rectsOffsetY, RECTANGLE_16* rects, int nrects);
void xf_SetWindowStyle(xfContext* xfc, xfAppWindow* appWindow, UINT32 style, UINT32 ex_style);
void xf_UpdateWindowArea(xfContext* xfc, xfAppWindow* appWindow, int x, int y, int width, int height);
void xf_DestroyWindow(xfContext* xfc, xfAppWindow* appWindow);
void xf_SetWindowMinMaxInfo(xfContext* xfc, xfAppWindow* appWindow,
		int maxWidth, int maxHeight, int maxPosX, int maxPosY,
		int minTrackWidth, int minTrackHeight, int maxTrackWidth, int maxTrackHeight);
void xf_StartLocalMoveSize(xfContext* xfc, xfAppWindow* appWindow, int direction, int x, int y);
void xf_EndLocalMoveSize(xfContext* xfc, xfAppWindow* appWindow);
xfAppWindow* xf_AppWindowFromX11Window(xfContext* xfc, Window wnd);

#endif /* __XF_WINDOW_H */
