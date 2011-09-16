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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <freerdp/common/color.h>

struct xf_info
{
	int bpp;
	int depth;
	int width;
	int height;
	int number;
	XImage* image;
	Screen* screen;
	Visual* visual;
	Display* display;
	int scanline_pad;
	HCLRCONV clrconv;
};
typedef struct xf_info xfInfo;

#endif /* __XFREERDP_H */
