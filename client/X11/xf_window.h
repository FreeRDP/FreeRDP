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

typedef struct xf_window xfWindow;

#include "xfreerdp.h"

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
	Pixmap surface;
	boolean fullscreen;
	boolean decorations;
};

void xf_ewmhints_init(xfInfo* xfi);

boolean xf_GetCurrentDesktop(xfInfo* xfi);
boolean xf_GetWorkArea(xfInfo* xfi);

void xf_SetWindowFullscreen(xfInfo* xfi, xfWindow* window, boolean fullscreen);
void xf_SetWindowDecorations(xfInfo* xfi, xfWindow* window, boolean show);

xfWindow* xf_CreateDesktopWindow(xfInfo* xfi, char* name, int width, int height);

xfWindow* xf_CreateWindow(xfInfo* xfi, int x, int y, int width, int height, uint32 id);
void xf_MoveWindow(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height);
void xf_ShowWindow(xfInfo* xfi, xfWindow* window, uint8 state);
void xf_SetWindowIcon(xfInfo* xfi, xfWindow* window, rdpIcon* icon);
void xf_SetWindowVisibilityRects(xfInfo* xfi, xfWindow* window, RECTANGLE_16* rects, int nrects);
void xf_DestroyWindow(xfInfo* xfi, xfWindow* window);

#endif /* __XF_WINDOW_H */
