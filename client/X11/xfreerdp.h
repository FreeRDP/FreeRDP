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

#include <freerdp/config.h>

typedef struct xf_context xfContext;

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#endif

#include <freerdp/api.h>

#include "xf_window.h"
#include "xf_monitor.h"
#include "xf_channels.h"

#if defined(CHANNEL_TSMF_CLIENT)
#include <freerdp/client/tsmf.h>
#endif

#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/clear.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/progressive.h>
#include <freerdp/codec/region.h>

#if !defined(XcursorUInt)
typedef unsigned int XcursorUInt;
#endif

#if !defined(XcursorPixel)
typedef XcursorUInt XcursorPixel;
#endif

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
	XcursorPixel* cursorPixels;
	UINT32 nCursors;
	UINT32 mCursors;
	UINT32* cursorWidths;
	UINT32* cursorHeights;
	Cursor* cursors;
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
typedef struct s_xfDispContext xfDispContext;
typedef struct s_xfVideoContext xfVideoContext;
typedef struct xf_rail_icon_cache xfRailIconCache;

/* Number of buttons that are mapped from X11 to RDP button events. */
#define NUM_BUTTONS_MAPPED 11

typedef struct
{
	int button;
	UINT16 flags;
} button_map;

#if defined(WITH_XI)
#define MAX_CONTACTS 20

typedef struct touch_contact
{
	int id;
	int count;
	double pos_x;
	double pos_y;
	double last_x;
	double last_y;

} touchContact;
#endif

struct xf_context
{
	rdpClientContext common;

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
	BOOL mouse_active;
	BOOL fullscreen_toggle;
	UINT32 KeyboardLayout;
	BOOL KeyboardState[256];
	XModifierKeymap* modifierMap;
	wArrayList* keyCombinations;
	wArrayList* xevents;
	BOOL actionScriptExists;

	int attribs_mask;
	XSetWindowAttributes attribs;
	BOOL complex_regions;
	VIRTUAL_SCREEN vscreen;
#if defined(CHANNEL_TSMF_CLIENT)
	void* xv_context;
#endif

	Atom* supportedAtoms;
	unsigned long supportedAtomCount;

	Atom UTF8_STRING;

	Atom _XWAYLAND_MAY_GRAB_KEYBOARD;

	Atom _NET_WM_ICON;
	Atom _MOTIF_WM_HINTS;
	Atom _NET_CURRENT_DESKTOP;
	Atom _NET_WORKAREA;

	Atom _NET_SUPPORTED;
	Atom _NET_SUPPORTING_WM_CHECK;

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
	Atom _NET_WM_WINDOW_TYPE_POPUP_MENU;
	Atom _NET_WM_WINDOW_TYPE_DROPDOWN_MENU;

	Atom _NET_WM_MOVERESIZE;
	Atom _NET_MOVERESIZE_WINDOW;

	Atom WM_STATE;
	Atom WM_PROTOCOLS;
	Atom WM_DELETE_WINDOW;

	/* Channels */
#if defined(CHANNEL_TSMF_CLIENT)
	TsmfClientContext* tsmf;
#endif

	xfClipboard* clipboard;
	CliprdrClientContext* cliprdr;
	xfVideoContext* xfVideo;
	xfDispContext* xfDisp;

	RailClientContext* rail;
	wHashTable* railWindows;
	xfRailIconCache* railIconCache;

	BOOL xkbAvailable;
	BOOL xrenderAvailable;

	/* value to be sent over wire for each logical client mouse button */
	button_map button_map[NUM_BUTTONS_MAPPED];
	BYTE savedMaximizedState;
	UINT32 locked;
	BOOL firstPressRightCtrl;
	BOOL ungrabKeyboardWithRightCtrl;

#if defined(WITH_XI)
	touchContact contacts[MAX_CONTACTS];
	int active_contacts;
	int lastEvType;
	XIDeviceEvent lastEvent;
	double firstDist;
	double lastDist;
	double z_vector;
	double px_vector;
	double py_vector;
#endif
	BOOL xi_rawevent;
	BOOL xi_event;
};

BOOL xf_create_window(xfContext* xfc);
BOOL xf_create_image(xfContext* xfc);
void xf_toggle_fullscreen(xfContext* xfc);

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
	XF_EXIT_NEGO_FAILURE = 133,
	XF_EXIT_LOGON_FAILURE = 134,
	XF_EXIT_ACCOUNT_LOCKED_OUT = 135,
	XF_EXIT_PRE_CONNECT_FAILED = 136,
	XF_EXIT_CONNECT_UNDEFINED = 137,
	XF_EXIT_POST_CONNECT_FAILED = 138,
	XF_EXIT_DNS_ERROR = 139,
	XF_EXIT_DNS_NAME_NOT_FOUND = 140,
	XF_EXIT_CONNECT_FAILED = 141,
	XF_EXIT_MCS_CONNECT_INITIAL_ERROR = 142,
	XF_EXIT_TLS_CONNECT_FAILED = 143,
	XF_EXIT_INSUFFICIENT_PRIVILEGES = 144,
	XF_EXIT_CONNECT_CANCELLED = 145,

	XF_EXIT_CONNECT_TRANSPORT_FAILED = 147,
	XF_EXIT_CONNECT_PASSWORD_EXPIRED = 148,
	XF_EXIT_CONNECT_PASSWORD_MUST_CHANGE = 149,
	XF_EXIT_CONNECT_KDC_UNREACHABLE = 150,
	XF_EXIT_CONNECT_ACCOUNT_DISABLED = 151,
	XF_EXIT_CONNECT_PASSWORD_CERTAINLY_EXPIRED = 152,
	XF_EXIT_CONNECT_CLIENT_REVOKED = 153,
	XF_EXIT_CONNECT_WRONG_PASSWORD = 154,
	XF_EXIT_CONNECT_ACCESS_DENIED = 155,
	XF_EXIT_CONNECT_ACCOUNT_RESTRICTION = 156,
	XF_EXIT_CONNECT_ACCOUNT_EXPIRED = 157,
	XF_EXIT_CONNECT_LOGON_TYPE_NOT_GRANTED = 158,
	XF_EXIT_CONNECT_NO_OR_MISSING_CREDENTIALS = 159,

	XF_EXIT_UNKNOWN = 255,
};

#define xf_lock_x11(xfc) xf_lock_x11_(xfc, __FUNCTION__)
#define xf_unlock_x11(xfc) xf_unlock_x11_(xfc, __FUNCTION__)

void xf_lock_x11_(xfContext* xfc, const char* fkt);
void xf_unlock_x11_(xfContext* xfc, const char* fkt);

BOOL xf_picture_transform_required(xfContext* xfc);

#define xf_draw_screen(_xfc, _x, _y, _w, _h) \
	xf_draw_screen_((_xfc), (_x), (_y), (_w), (_h), __FUNCTION__, __FILE__, __LINE__)
void xf_draw_screen_(xfContext* xfc, int x, int y, int w, int h, const char* fkt, const char* file,
                     int line);

BOOL xf_keyboard_update_modifier_map(xfContext* xfc);

DWORD xf_exit_code_from_disconnect_reason(DWORD reason);

#endif /* FREERDP_CLIENT_X11_FREERDP_H */
