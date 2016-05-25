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

#include <freerdp/server/shadow.h>

typedef struct x11_shadow_subsystem x11ShadowSubsystem;

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

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

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

struct x11_shadow_subsystem
{
	RDP_SHADOW_SUBSYSTEM_COMMON();

	HANDLE thread;

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
	BOOL composite;

	BOOL use_xshm;
	BOOL use_xfixes;
	BOOL use_xdamage;
	BOOL use_xinerama;

	XImage* fb_image;
	Pixmap fb_pixmap;
	Window root_window;
	XShmSegmentInfo fb_shm_info;

	int cursorHotX;
	int cursorHotY;
	int cursorWidth;
	int cursorHeight;
	UINT32 cursorId;
	BYTE* cursorPixels;
	int cursorMaxWidth;
	int cursorMaxHeight;
	rdpShadowClient* lastMouseClient;

#ifdef WITH_XDAMAGE
	GC xshm_gc;
	Damage xdamage;
	int xdamage_notify_event;
	XserverRegion xdamage_region;
#endif

#ifdef WITH_XFIXES
	int xfixes_cursor_notify_event;
#endif
};

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SHADOW_SERVER_X11_H */
