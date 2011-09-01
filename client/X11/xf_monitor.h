/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Monitor Handling
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

#ifndef __XF_MONITOR_H
#define __XF_MONITOR_H

#include <freerdp/freerdp.h>
#include <freerdp/rail/rail.h>

struct _MONITOR_INFO
{
	RECTANGLE_16 area;
	RECTANGLE_16 workarea;
	boolean primary;
};
typedef struct _MONITOR_INFO MONITOR_INFO;

struct _VIRTUAL_SCREEN
{
	int nmonitors;
	RECTANGLE_16 area;
	RECTANGLE_16 workarea;
	MONITOR_INFO* monitors;
};
typedef struct _VIRTUAL_SCREEN VIRTUAL_SCREEN;

#include "xfreerdp.h"

boolean xf_detect_monitors(xfInfo* xfi, rdpSettings* settings);

#endif /* __XF_MONITOR_H */
