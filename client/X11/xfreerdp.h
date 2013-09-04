/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

typedef struct xf_context xfContext;

#include <freerdp/api.h>

#include "xf_window.h"
#include "xf_monitor.h"
#include "xf_channels.h"

struct xf_WorkArea
{
	UINT32 x;
	UINT32 y;
	UINT32 width;
	UINT32 height;
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
	rdpContext context;
	DEFINE_RDP_CLIENT_COMMON();

	freerdp* instance;
	rdpSettings* settings;

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
	BOOL big_endian;
	BOOL fullscreen;
	BOOL grab_keyboard;
	BOOL unobscured;
	BOOL debug;
	xfWindow* window;
	xfPointer* pointer;
	xfWorkArea workArea;
	int current_desktop;
	BOOL remote_app;
	BOOL disconnect;
	HCLRCONV clrconv;
	HANDLE mutex;
	BOOL UseXThreads;
	BOOL cursorHidden;

	HGDI_DC hdc;
	BYTE* primary_buffer;

	BOOL frame_begin;
	UINT16 frame_x1;
	UINT16 frame_y1;
	UINT16 frame_x2;
	UINT16 frame_y2;

	int originalWidth;
	int originalHeight;
	int currentWidth;
	int currentHeight;
	int XInputOpcode;
	BOOL enableScaling;

	int offset_x;
	int offset_y;

	BOOL focused;
	BOOL use_xinput;
	BOOL mouse_active;
	BOOL suppress_output;
	BOOL fullscreen_toggle;
	UINT32 keyboard_layout_id;
	BOOL pressed_keys[256];
	XModifierKeymap* modifier_map;
	XSetWindowAttributes attribs;
	BOOL complex_regions;
	VIRTUAL_SCREEN vscreen;
	BYTE* bmp_codec_none;
	BYTE* bmp_codec_nsc;
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

	Atom WM_STATE;
	Atom WM_PROTOCOLS;
	Atom WM_DELETE_WINDOW;

	/* Channels */
	RdpeiClientContext* rdpei;
};

void xf_create_window(xfContext* xfc);
void xf_toggle_fullscreen(xfContext* xfc);
BOOL xf_post_connect(freerdp* instance);

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

void xf_lock_x11(xfContext* xfc, BOOL display);
void xf_unlock_x11(xfContext* xfc, BOOL display);

void xf_draw_screen_scaled(xfContext* xfc, int x, int y, int w, int h, BOOL scale);
void xf_transform_window(xfContext* xfc);

FREERDP_API DWORD xf_exit_code_from_disconnect_reason(DWORD reason);

#endif /* __XFREERDP_H */

