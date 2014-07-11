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

typedef struct x11_shadow_server x11ShadowServer;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int x11_shadow_server_start(x11ShadowServer* server);
FREERDP_API int x11_shadow_server_stop(x11ShadowServer* server);

FREERDP_API HANDLE x11_shadow_server_get_thread(x11ShadowServer* server);

FREERDP_API x11ShadowServer* x11_shadow_server_new(int argc, char** argv);
FREERDP_API void x11_shadow_server_free(x11ShadowServer* server);

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

struct x11_shadow_server
{
	DWORD port;
	HANDLE thread;
	freerdp_listener* listener;

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
	int activePeerCount;

	BOOL use_xshm;
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

typedef struct x11_shadow_client x11ShadowClient;

struct x11_shadow_client
{
	rdpContext _p;

	wStream* s;
	BOOL activated;
	HANDLE monitorThread;
	RFX_CONTEXT* rfx_context;
	x11ShadowServer* server;
};

void x11_shadow_peer_accepted(freerdp_listener* instance, freerdp_peer* client);

void* x11_shadow_server_thread(void* param);

void* x11_shadow_update_thread(void* param);

int x11_shadow_cursor_init(x11ShadowServer* server);

void x11_shadow_input_register_callbacks(rdpInput* input);

int x11_shadow_update_encode(freerdp_peer* client, int x, int y, int width, int height);

#endif /* FREERDP_SHADOW_SERVER_X11_H */
