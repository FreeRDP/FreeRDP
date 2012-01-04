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

#include <freerdp/freerdp.h>
#include <freerdp/channels/channels.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/rail/rail.h>
#include <freerdp/cache/cache.h>

typedef struct xf_info xfInfo;

#include "xf_window.h"
#include "xf_monitor.h"

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

struct xf_bitmap
{
	rdpBitmap bitmap;
	Pixmap pixmap;
};
typedef struct xf_bitmap xfBitmap;

struct xf_glyph
{
	rdpGlyph glyph;
	Pixmap pixmap;
};
typedef struct xf_glyph xfGlyph;

struct xf_context
{
	rdpContext _p;

	xfInfo* xfi;
	rdpSettings* settings;
};
typedef struct xf_context xfContext;

struct xf_info
{
	freerdp* instance;
	xfContext* context;
	rdpContext* _context;

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
	boolean debug;
	xfWindow* window;
	xfWorkArea workArea;
	int current_desktop;
	boolean remote_app;
	boolean disconnect;
	HCLRCONV clrconv;
	Window parent_window;

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
	Atom _NET_WM_WINDOW_TYPE_POPUP;
	Atom _NET_WM_WINDOW_TYPE_DROPDOWN_MENU;

	Atom _NET_WM_MOVERESIZE;
	Atom _NET_MOVERESIZE_WINDOW;

	Atom WM_PROTOCOLS;
	Atom WM_DELETE_WINDOW;
};

void xf_toggle_fullscreen(xfInfo* xfi);
boolean xf_post_connect(freerdp* instance);

enum XF_EXIT_CODE
{
	/* section 0-15: protocol-independent codes */
	XF_EXIT_SUCCESS = 0,
	XF_EXIT_DISCONNECT = 1,
	XF_EXIT_LOGOFF = 2,
	XF_EXIT_IDLE_TIMEOUT = 3,
	XF_EXIT_LOGON_TIMEOUT = 4,
	XF_EXIT_CONN_REPLACED = 5,
	XF_EXIT_OUT_OF_MEMORY = 6,
	XF_EXIT_CONN_DENIED = 7,
	XF_EXIT_CONN_DENIED_FIPS = 8,
	XF_EXIT_USER_PRIVILEGES = 9,
	XF_EXIT_FRESH_CREDENTIALS_REQUIRED = 10,
	XF_EXIT_DISCONNECT_BY_USER = 11,

	/* section 16-31: license error set */
	XF_EXIT_LICENSE_INTERNAL = 16,
	XF_EXIT_LICENSE_NO_LICENSE_SERVER = 17,
	XF_EXIT_LICENSE_NO_LICENSE = 18,
	XF_EXIT_LICENSE_BAD_CLIENT_MSG = 19,
	XF_EXIT_LICENSE_HWID_DOESNT_MATCH = 20,
	XF_EXIT_LICENSE_BAD_CLIENT = 21,
	XF_EXIT_LICENSE_CANT_FINISH_PROTOCOL = 22,
	XF_EXIT_LICENSE_CLIENT_ENDED_PROTOCOL = 23,
	XF_EXIT_LICENSE_BAD_CLIENT_ENCRYPTION = 24,
	XF_EXIT_LICENSE_CANT_UPGRADE = 25,
	XF_EXIT_LICENSE_NO_REMOTE_CONNECTIONS = 26,

	/* section 32-127: RDP protocol error set */
	XF_EXIT_RDP = 32,

	/* section 128-254: xfreerdp specific exit codes */
	XF_EXIT_PARSE_ARGUMENTS = 128,
	XF_EXIT_MEMORY = 129,
	XF_EXIT_PROTOCOL = 130,
	XF_EXIT_CONN_FAILED = 131,

	XF_EXIT_UNKNOWN = 255,
};

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
