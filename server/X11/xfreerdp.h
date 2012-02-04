/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP X11 Server
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

#include <freerdp/codec/color.h>

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

typedef struct xf_info xfInfo;

#include "xf_event.h"

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
	boolean use_xshm;

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
};

#endif /* __XFREERDP_H */
