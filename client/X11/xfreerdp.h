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

#include "gdi.h"
#include <X11/Xlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/chanman.h>

#define SET_XFI(_instance, _xfi) (_instance)->param1 = _xfi
#define GET_XFI(_instance) ((xfInfo*) ((_instance)->param1))

#define SET_CHANMAN(_instance, _chanman) (_instance)->param2 = _chanman
#define GET_CHANMAN(_instance) ((rdpChanMan*) ((_instance)->param2))

struct xf_info
{
	GC gc;
	int bpp;
	int xfds;
	int depth;
	int width;
	int height;
	Window window;
	Screen* screen;
	XImage* image;
	Pixmap primary;
	Drawable drawing;
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

	GC gc_mono;
	GC gc_default;
	Pixmap bitmap_mono;

	boolean focused;
	boolean mouse_active;
	boolean mouse_motion;
	boolean fullscreen_toggle;
	uint32 keyboard_layout_id;
	boolean pressed_keys[256];
	XModifierKeymap* modifier_map;
};
typedef struct xf_info xfInfo;

void xf_toggle_fullscreen(xfInfo* xfi);

#endif /* __XFREERDP_H */
