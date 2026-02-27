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

#ifndef FREERDP_CLIENT_X11_WINDOW_H
#define FREERDP_CLIENT_X11_WINDOW_H

#include <X11/Xlib.h>

#include <winpr/platform.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gfx.h>

#include "xf_types.h"
#include "xf_rail.h"
#include "xf_floatbar.h"

typedef struct xf_window xfWindow;

// Extended ICCM flags http://standards.freedesktop.org/wm-spec/wm-spec-latest.html
WINPR_PRAGMA_DIAG_PUSH
WINPR_PRAGMA_DIAG_IGNORED_RESERVED_ID_MACRO

#define NET_WM_MOVERESIZE_SIZE_TOPLEFT 0
#define NET_WM_MOVERESIZE_SIZE_TOP 1
#define NET_WM_MOVERESIZE_SIZE_TOPRIGHT 2
#define NET_WM_MOVERESIZE_SIZE_RIGHT 3
#define NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define NET_WM_MOVERESIZE_SIZE_BOTTOM 5
#define NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT 6
#define NET_WM_MOVERESIZE_SIZE_LEFT 7
#define NET_WM_MOVERESIZE_MOVE 8           /* movement only */
#define NET_WM_MOVERESIZE_SIZE_KEYBOARD 9  /* size via keyboard */
#define NET_WM_MOVERESIZE_MOVE_KEYBOARD 10 /* move via keyboard */
#define NET_WM_MOVERESIZE_CANCEL 11        /* cancel operation */

#define NET_WM_STATE_REMOVE 0 /* remove/unset property */
#define NET_WM_STATE_ADD 1    /* add/set property */
#define NET_WM_STATE_TOGGLE 2 /* toggle property */

WINPR_PRAGMA_DIAG_POP

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
	xfFloatbar* floatbar;
	BOOL decorations;
	BOOL is_mapped;
	BOOL is_transient;
};

void xf_ewmhints_init(xfContext* xfc);

BOOL xf_GetWorkArea(xfContext* xfc);

void xf_SetWindowFullscreen(xfContext* xfc, xfWindow* window, BOOL fullscreen);
void xf_SetWindowMinimized(xfContext* xfc, xfWindow* window);
void xf_SetWindowDecorations(xfContext* xfc, Window window, BOOL show);
void xf_SetWindowUnlisted(xfContext* xfc, Window window);

void xf_DestroyDesktopWindow(xfContext* xfc, xfWindow* window);

WINPR_ATTR_MALLOC(xf_DestroyDesktopWindow, 2)
xfWindow* xf_CreateDesktopWindow(xfContext* xfc, char* name, int width, int height);

void xf_ResizeDesktopWindow(xfContext* xfc, xfWindow* window, int width, int height);

Window xf_CreateDummyWindow(xfContext* xfc);
void xf_DestroyDummyWindow(xfContext* xfc, Window window);

BOOL xf_GetWindowProperty(xfContext* xfc, Window window, Atom property, int length,
                          unsigned long* nitems, unsigned long* bytes, BYTE** prop);
void xf_SendClientEvent(xfContext* xfc, Window window, Atom atom, unsigned int numArgs, ...);

BOOL xf_AppWindowCreate(xfContext* xfc, xfAppWindow* appWindow);
int xf_AppWindowInit(xfContext* xfc, xfAppWindow* appWindow);

BOOL xf_AppWindowResize(xfContext* xfc, xfAppWindow* appWindow);

void xf_SetWindowText(xfContext* xfc, xfAppWindow* appWindow, const char* name);
void xf_MoveWindow(xfContext* xfc, xfAppWindow* appWindow, int x, int y, int width, int height);
void xf_ShowWindow(xfContext* xfc, xfAppWindow* appWindow, BYTE state);
// void xf_SetWindowIcon(xfContext* xfc, xfAppWindow* appWindow, rdpIcon* icon);
void xf_SetWindowRects(xfContext* xfc, xfAppWindow* appWindow, RECTANGLE_16* rects, int nrects);
void xf_SetWindowVisibilityRects(xfContext* xfc, xfAppWindow* appWindow, UINT32 rectsOffsetX,
                                 UINT32 rectsOffsetY, RECTANGLE_16* rects, int nrects);
void xf_SetWindowStyle(xfContext* xfc, xfAppWindow* appWindow, UINT32 style, UINT32 ex_style);
void xf_SetWindowActions(xfContext* xfc, xfAppWindow* appWindow);
void xf_UpdateWindowArea(xfContext* xfc, xfAppWindow* appWindow, int x, int y, int width,
                         int height);
UINT xf_AppUpdateWindowFromSurface(xfContext* xfc, gdiGfxSurface* surface);

void xf_AppWindowDestroyImage(xfAppWindow* appWindow);
void xf_DestroyWindow(xfContext* xfc, xfAppWindow* appWindow);
void xf_SetWindowMinMaxInfo(xfContext* xfc, xfAppWindow* appWindow, int maxWidth, int maxHeight,
                            int maxPosX, int maxPosY, int minTrackWidth, int minTrackHeight,
                            int maxTrackWidth, int maxTrackHeight);
void xf_StartLocalMoveSize(xfContext* xfc, xfAppWindow* appWindow, int direction, int x, int y);
void xf_EndLocalMoveSize(xfContext* xfc, xfAppWindow* appWindow);

#define xf_AppWindowFromX11Window(xfc, wnd) \
	xf_AppWindowFromX11WindowFrom((xfc), (wnd), __FILE__, __func__, __LINE__)
WINPR_ATTR_MALLOC(xf_rail_return_windowFrom, 1)
xfAppWindow* xf_AppWindowFromX11WindowFrom(xfContext* xfc, Window wnd, const char* file,
                                           const char* fkt, size_t line);

#define xf_AppWindowsLock(xfc) xfAppWindowsLockFrom((xfc), __FILE__, __func__, __LINE__)
void xfAppWindowsLockFrom(xfContext* xfc, const char* file, const char* fkt, size_t line);

#define xf_AppWindowsUnlock(xfc) xfAppWindowsUnlockFrom((xfc), __FILE__, __func__, __LINE__)
void xfAppWindowsUnlockFrom(xfContext* xfc, const char* file, const char* fkt, size_t line);

const char* window_styles_to_string(UINT32 style, char* buffer, size_t length);
const char* window_styles_ex_to_string(UINT32 styleEx, char* buffer, size_t length);

#endif /* FREERDP_CLIENT_X11_WINDOW_H */
