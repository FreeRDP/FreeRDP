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
	int x;
	int y;
	int width;
	int height;
	Window handle;
	Pixmap surface;
	boolean fullscreen;
	boolean decorations;
};

boolean window_GetCurrentDesktop(xfInfo* xfi);
boolean window_GetWorkArea(xfInfo* xfi);

void desktop_fullscreen(xfInfo* xfi, xfWindow* window, boolean fullscreen);
void window_show_decorations(xfInfo* xfi, xfWindow* window, boolean show);

xfWindow* desktop_create(xfInfo* xfi, char* name);

xfWindow* xf_CreateWindow(xfInfo* xfi, int x, int y, int width, int height, uint32 id);
void xf_MoveWindow(xfInfo* xfi, xfWindow* window, int x, int y, int width, int height);
void xf_ShowWindow(xfInfo* xfi, xfWindow* window, uint8 state);
void xf_SetWindowIcon(xfInfo* xfi, xfWindow* window, rdpIcon* icon);
void xf_DestroyWindow(xfInfo* xfi, xfWindow* window);

#endif /* __XF_WINDOW_H */
