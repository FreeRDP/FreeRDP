/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_CLIENT_X11_MONITOR_H
#define FREERDP_CLIENT_X11_MONITOR_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

typedef struct
{
	RECTANGLE_16 area;
	RECTANGLE_16 workarea;
	BOOL primary;
} MONITOR_INFO;

typedef struct
{
	int nmonitors;
	RECTANGLE_16 area;
	RECTANGLE_16 workarea;
	MONITOR_INFO* monitors;
} VIRTUAL_SCREEN;

#include "xf_client.h"
#include "xfreerdp.h"

FREERDP_API int xf_list_monitors(xfContext* xfc);
FREERDP_API BOOL xf_detect_monitors(xfContext* xfc, UINT32* pWidth, UINT32* pHeight);
FREERDP_API void xf_monitors_free(xfContext* xfc);

#endif /* FREERDP_CLIENT_X11_MONITOR_H */
