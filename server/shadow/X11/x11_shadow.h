/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_SHADOW_SERVER_X11_H
#define FREERDP_SHADOW_SERVER_X11_H

#include <winpr/crt.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

typedef struct xf_info xfInfo;
typedef struct xf_server xfServer;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Server Interface
 */

FREERDP_API int x11_shadow_server_global_init();
FREERDP_API int x11_shadow_server_global_uninit();

FREERDP_API int x11_shadow_server_start(xfServer* server);
FREERDP_API int x11_shadow_server_stop(xfServer* server);

FREERDP_API HANDLE x11_shadow_server_get_thread(xfServer* server);

FREERDP_API xfServer* x11_shadow_server_new(int argc, char** argv);
FREERDP_API void x11_shadow_server_free(xfServer* server);

#ifdef __cplusplus
}
#endif

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/listener.h>
#include <freerdp/codec/color.h>

#include <X11/Xlib.h>

#ifdef WITH_XSHM
#include <X11/extensions/XShm.h>
#endif

#ifdef WITH_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#ifdef WITH_XTEST
#include <X11/extensions/XTest.h>
#endif

#ifdef WITH_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

struct xf_info
{
	int bpp;
	int xfds;
	int depth;
	int width;
	int height;
	int number;
	XImage* image;
	Screen* screen;
	Visual* visual;
	Display* display;
	int scanline_pad;
	int bytesPerPixel;
	HCLRCONV clrconv;
	BOOL use_xshm;
	int activePeerCount;

	XImage* fb_image;
	Pixmap fb_pixmap;
	Window root_window;
	XShmSegmentInfo fb_shm_info;

#ifdef WITH_XDAMAGE
	GC xdamage_gc;
	Damage xdamage;
	int xdamage_notify_event;
	XserverRegion xdamage_region;
#endif

#ifdef WITH_XFIXES
	int xfixes_notify_event;
#endif
};

struct xf_server
{
	DWORD port;
	HANDLE thread;
	freerdp_listener* listener;
};

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/listener.h>
#include <freerdp/utils/stopwatch.h>

typedef struct xf_peer_context xfPeerContext;

#define PeerEvent_Base						0

#define PeerEvent_Class						(PeerEvent_Base + 1)

#define PeerEvent_EncodeRegion					1

struct xf_peer_context
{
	rdpContext _p;

	int fps;
	wStream* s;
	xfInfo* info;
	HANDLE mutex;
	BOOL activated;
	HANDLE monitorThread;
	HANDLE updateReadyEvent;
	HANDLE updateSentEvent;
	RFX_CONTEXT* rfx_context;
};

void xf_peer_accepted(freerdp_listener* instance, freerdp_peer* client);

void* x11_shadow_server_thread(void* param);

void* xf_update_thread(void* param);

int xf_cursor_init(xfInfo* xfi);

XImage* xf_snapshot(xfPeerContext* xfp, int x, int y, int width, int height);
void xf_xdamage_subtract_region(xfPeerContext* xfp, int x, int y, int width, int height);
int xf_update_encode(freerdp_peer* client, int x, int y, int width, int height);

void xf_input_synchronize_event(rdpInput* input, UINT32 flags);
void xf_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code);
void xf_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code);
void xf_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
void xf_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);
void xf_input_register_callbacks(rdpInput* input);

#endif /* FREERDP_SHADOW_SERVER_X11_H */
