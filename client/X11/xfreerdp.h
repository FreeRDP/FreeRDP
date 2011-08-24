/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Client
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

#ifndef __XFREERDP_H
#define __XFREERDP_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <freerdp/freerdp.h>
#include <freerdp/chanman/chanman.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/rail/rail.h>

typedef struct xf_info xfInfo;

#include "xf_window.h"

#define SET_XFI(_instance, _xfi) (_instance)->param1 = _xfi
#define GET_XFI(_instance) ((xfInfo*) ((_instance)->param1))

#define SET_CHANMAN(_instance, _chanman) (_instance)->param2 = _chanman
#define GET_CHANMAN(_instance) ((rdpChanMan*) ((_instance)->param2))

struct xf_WorkArea
{
	uint32 x;
	uint32 y;
	uint32 width;
	uint32 height;
};
typedef struct xf_WorkArea xfWorkArea;

struct xf_info
{
	GC gc;
	int bpp;
	int xfds;
	int depth;
	int width;
	int height;
	Screen* screen;
	XImage* image;
	Pixmap primary;
	Visual* visual;
	Display* display;
	Colormap colormap;
	int screen_number;
	int scanline_pad;
	boolean big_endian;
	boolean fullscreen;
	boolean unobscured;
	boolean decoration;
	freerdp* instance;
	xfWindow* window;
	xfWorkArea workArea;
	int current_desktop;
	boolean remote_app;
	rdpRail* rail;

	boolean focused;
	boolean mouse_active;
	boolean mouse_motion;
	boolean fullscreen_toggle;
	uint32 keyboard_layout_id;
	boolean pressed_keys[256];
	XModifierKeymap* modifier_map;
	XSetWindowAttributes attribs;

	Atom _NET_WM_ICON;
	Atom _MOTIF_WM_HINTS;
	Atom _NET_CURRENT_DESKTOP;
	Atom _NET_WORKAREA;
	Atom _NET_WM_STATE;
	Atom _NET_WM_STATE_FULLSCREEN;
};

void xf_toggle_fullscreen(xfInfo* xfi);

#endif /* __XFREERDP_H */
