/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Thincast Technologies GmbH
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
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

#ifndef FREERDP_CLIENT_X11_FREERDP_H
#define FREERDP_CLIENT_X11_FREERDP_H

typedef struct xf_context xfContext;

#include <freerdp/api.h>

#include "xf_window.h"
#include "xf_monitor.h"
#include "xf_channels.h"

#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/clear.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/progressive.h>
#include <freerdp/codec/region.h>

struct xf_FullscreenMonitors
{
	UINT32 top;
	UINT32 bottom;
	UINT32 left;
	UINT32 right;
};
typedef struct xf_FullscreenMonitors xfFullscreenMonitors;

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
	XImage* image;
};
typedef struct xf_bitmap xfBitmap;

struct xf_glyph
{
	rdpGlyph glyph;
	Pixmap pixmap;
};
typedef struct xf_glyph xfGlyph;

typedef struct xf_clipboard xfClipboard;

/* Value of the first logical button number in X11 which must be */
/* subtracted to go from a button number in X11 to an index into */
/* a per-button array.                                           */
#define BUTTON_BASE Button1

/* Number of buttons that are mapped from X11 to RDP button events. */
#define NUM_BUTTONS_MAPPED 3

struct xf_context
{
	rdpContext context;
	DEFINE_RDP_CLIENT_COMMON();

	GC gc;
	int xfds;
	int depth;

	GC gc_mono;
	BOOL invert;
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
	BOOL decorations;
	BOOL grab_keyboard;
	BOOL unobscured;
	BOOL debug;
	HANDLE x11event;
	xfWindow* window;
	xfAppWindow* appWindow;
	xfPointer* pointer;
	xfWorkArea workArea;
	xfFullscreenMonitors fullscreenMonitors;
	int current_desktop;
	BOOL remote_app;
	HANDLE mutex;
	BOOL UseXThreads;
	BOOL cursorHidden;

	HGDI_DC hdc;
	UINT32 bitmap_size;
	BYTE* bitmap_buffer;

	BOOL frame_begin;
	UINT16 frame_x1;
	UINT16 frame_y1;
	UINT16 frame_x2;
	UINT16 frame_y2;

	int XInputOpcode;

	int savedWidth;
	int savedHeight;
	int savedPosX;
	int savedPosY;

#ifdef WITH_XRENDER
	int scaledWidth;
	int scaledHeight;
	int offset_x;
	int offset_y;
#endif

	BOOL focused;
	BOOL use_xinput;
	BOOL mouse_active;
	BOOL suppress_output;
	BOOL fullscreen_toggle;
	BOOL controlToggle;
	UINT32 KeyboardLayout;
	BOOL KeyboardState[256];
	XModifierKeymap* modifierMap;
	wArrayList* keyCombinations;
	wArrayList* xevents;
	BOOL actionScriptExists;

	XSetWindowAttributes attribs;
	BOOL complex_regions;
	VIRTUAL_SCREEN vscreen;
	void* xv_context;
	TsmfClientContext* tsmf;
	xfClipboard* clipboard;
	CliprdrClientContext* cliprdr;

	Atom UTF8_STRING;

	Atom _NET_WM_ICON;
	Atom _MOTIF_WM_HINTS;
	Atom _NET_CURRENT_DESKTOP;
	Atom _NET_WORKAREA;

	Atom _NET_WM_STATE;
	Atom _NET_WM_STATE_FULLSCREEN;
	Atom _NET_WM_STATE_MAXIMIZED_HORZ;
	Atom _NET_WM_STATE_MAXIMIZED_VERT;
	Atom _NET_WM_STATE_SKIP_TASKBAR;
	Atom _NET_WM_STATE_SKIP_PAGER;

	Atom _NET_WM_FULLSCREEN_MONITORS;

	Atom _NET_WM_NAME;
	Atom _NET_WM_PID;

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
	RdpgfxClientContext* gfx;
	EncomspClientContext* encomsp;

	RailClientContext* rail;
	wHashTable* railWindows;

	BOOL xkbAvailable;
	BOOL xrenderAvailable;

	/* value to be sent over wire for each logical client mouse button */
	int button_map[NUM_BUTTONS_MAPPED];
};

BOOL xf_create_window(xfContext* xfc);
void xf_toggle_fullscreen(xfContext* xfc);
void xf_toggle_control(xfContext* xfc);

void xf_encomsp_init(xfContext* xfc, EncomspClientContext* encomsp);
void xf_encomsp_uninit(xfContext* xfc, EncomspClientContext* encomsp);

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
	XF_EXIT_AUTH_FAILURE = 132,

	XF_EXIT_UNKNOWN = 255,
};

void xf_lock_x11(xfContext* xfc, BOOL display);
void xf_unlock_x11(xfContext* xfc, BOOL display);

BOOL xf_picture_transform_required(xfContext* xfc);
void xf_draw_screen(xfContext* xfc, int x, int y, int w, int h);

FREERDP_API DWORD xf_exit_code_from_disconnect_reason(DWORD reason);

#endif /* FREERDP_CLIENT_X11_FREERDP_H */

