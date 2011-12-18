/**
 * FreeRDP: A Remote Desktop Protocol Client
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
#include <freerdp/utils/memory.h>

typedef struct xf_localmove xfLocalMove;
typedef struct xf_window xfWindow;

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

enum xf_localmove_state
{
	LMS_NOT_ACTIVE,
	LMS_STARTING,
	LMS_ACTIVE,
	LMS_TERMINATING
};

struct xf_localmove
{
	int root_x;  // relative to root
	int root_y;
	int window_x; // relative to window
	int window_y;
	enum xf_localmove_state state;
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
	Window handle;
	boolean fullscreen;
	boolean decorations;
	rdpWindow* window;
	boolean is_mapped;
	boolean is_transient;
	xfLocalMove local_move;
};

void xf_ewmhints_init(xfInfo* xfi);

boolean xf_GetCurrentDesktop(xfInfo* xfi);
boolean xf_GetWorkArea(xfInfo* xfi);

void xf_SetWindowFullscreen(xfInfo* xfi, xfWindow* window, boolean fullscreen);
void xf_SetWindowDecorations(xfInfo* xfi, xfWindow* window, boolean show);
void xf_SetWindowUnlisted(xfInfo* xfi, xfWindow* window);

xfWindow* xf_CreateDesktopWindow(xfInfo* xfi, char* name, int width, int height, boolean decorations);
void xf_ResizeDesktopWindow(xfInfo* xfi, xfWindow* window, int width, int height);

xfWindow* xf_CreateWindow(xfInfo* xfi, rdpWindow* wnd, int x, int y, int width, int height, uint32 id);
void xf_MoveWindow(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height);
void xf_ShowWindow(xfInfo* xfi, xfWindow* window, uint8 state);
void xf_SetWindowIcon(xfInfo* xfi, xfWindow* window, rdpIcon* icon);
void xf_SetWindowRects(xfInfo* xfi, xfWindow* window, RECTANGLE_16* rects, int nrects);
void xf_SetWindowVisibilityRects(xfInfo* xfi, xfWindow* window, RECTANGLE_16* rects, int nrects);
void xf_SetWindowStyle(xfInfo* xfi, xfWindow* window, uint32 style, uint32 ex_style);
void xf_UpdateWindowArea(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height);
boolean xf_IsWindowBorder(xfInfo* xfi, xfWindow* xfw, int x, int y);
void xf_DestroyWindow(xfInfo* xfi, xfWindow* window);

void xf_SetWindowMinMaxInfo(xfInfo* xfi, xfWindow* window, int maxWidth, int maxHeight,
		int maxPosX, int maxPosY, int minTrackWidth, int minTrackHeight, int maxTrackWidth, int maxTrackHeight);


void xf_StartLocalMoveSize(xfInfo* xfi, xfWindow* window, int direction, int x, int y);
void xf_EndLocalMoveSize(xfInfo *xfi, xfWindow *window);
void xf_SendClientEvent(xfInfo *xfi, xfWindow* window, Atom atom, unsigned int numArgs, ...);

#endif /* __XF_WINDOW_H */
