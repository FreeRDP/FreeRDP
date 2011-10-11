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
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/rail/rail.h>
#include <freerdp/cache/cache.h>

typedef struct xf_info xfInfo;

#include "xf_window.h"
#include "xf_monitor.h"

#define SET_XFI(_instance, _xfi) (_instance)->client = _xfi
#define GET_XFI(_instance) ((xfInfo*) ((_instance)->client))

#define SET_CHANMAN(_instance, _chanman) (_instance)->chanman = _chanman
#define GET_CHANMAN(_instance) ((rdpChanMan*) ((_instance)->chanman))

struct xf_WorkArea
{
	uint32 x;
	uint32 y;
	uint32 width;
	uint32 height;
};
typedef struct xf_WorkArea xfWorkArea;

struct xf_pointer
{
	rdpPointer pointer;
	Cursor cursor;
};
typedef struct xf_pointer xfPointer;

struct xf_info
{
	GC gc;
	int bpp;
	int xfds;
	int depth;
	int width;
	int height;
	int srcBpp;
	GC gc_mono;
	Screen* screen;
	XImage* image;
	Pixmap primary;
	Pixmap drawing;
	Visual* visual;
	Display* display;
	Drawable drawable;
	Pixmap bitmap_mono;
	Colormap colormap;
	int screen_number;
	int scanline_pad;
	boolean big_endian;
	boolean fullscreen;
	boolean grab_keyboard;
	boolean unobscured;
	boolean decorations;
	freerdp* instance;
	xfWindow* window;
	xfWorkArea workArea;
	int current_desktop;
	boolean remote_app;
	HCLRCONV clrconv;
	rdpRail* rail;
	rdpCache* cache;

	HGDI_DC hdc;
	boolean sw_gdi;
	uint8* primary_buffer;

	boolean focused;
	boolean mouse_active;
	boolean mouse_motion;
	boolean fullscreen_toggle;
	uint32 keyboard_layout_id;
	boolean pressed_keys[256];
	XModifierKeymap* modifier_map;
	XSetWindowAttributes attribs;
	boolean complex_regions;
	VIRTUAL_SCREEN vscreen;
	uint8* bmp_codec_none;
	uint8* bmp_codec_nsc;
	void* rfx_context;
	void* nsc_context;
	void* xv_context;
	void* clipboard_context;
	Atom _NET_WM_ICON;
	Atom _MOTIF_WM_HINTS;
	Atom _NET_CURRENT_DESKTOP;
	Atom _NET_WORKAREA;

	Atom _NET_WM_STATE;
	Atom _NET_WM_STATE_FULLSCREEN;
	Atom _NET_WM_STATE_SKIP_TASKBAR;
	Atom _NET_WM_STATE_SKIP_PAGER;

	Atom _NET_WM_WINDOW_TYPE;
	Atom _NET_WM_WINDOW_TYPE_NORMAL;
	Atom _NET_WM_WINDOW_TYPE_DIALOG;
	Atom _NET_WM_WINDOW_TYPE_UTILITY;

	Atom _NET_WM_MOVERESIZE;

	Atom WM_PROTOCOLS;
	Atom WM_DELETE_WINDOW;
};

void xf_toggle_fullscreen(xfInfo* xfi);
boolean xf_post_connect(freerdp* instance);

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(fmt, ...) DEBUG_CLASS(X11, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#ifdef WITH_DEBUG_X11_LOCAL_MOVESIZE
#define DEBUG_X11_LMS(fmt, ...) DEBUG_CLASS(X11_LMS, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11_LMS(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __XFREERDP_H */
