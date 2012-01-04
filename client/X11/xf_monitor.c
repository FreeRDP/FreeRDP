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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "xf_monitor.h"

/* See MSDN Section on Multiple Display Monitors: http://msdn.microsoft.com/en-us/library/dd145071 */

boolean xf_detect_monitors(xfInfo* xfi, rdpSettings* settings)
{
	int i;
	VIRTUAL_SCREEN* vscreen;

#ifdef WITH_XINERAMA
	int ignored, ignored2;
	XineramaScreenInfo* screen_info = NULL;
#endif

	vscreen = &xfi->vscreen;

	if (xf_GetWorkArea(xfi) != true)
	{
		xfi->workArea.x = 0;
		xfi->workArea.y = 0;
		xfi->workArea.width = WidthOfScreen(xfi->screen);
		xfi->workArea.height = HeightOfScreen(xfi->screen);
	}

	if (settings->fullscreen)
	{
		settings->width = WidthOfScreen(xfi->screen);
		settings->height = HeightOfScreen(xfi->screen);	
	}
	else if (settings->workarea)
	{
		settings->width = xfi->workArea.width;
		settings->height = xfi->workArea.height;
	}
	else if (settings->percent_screen)
	{
		settings->width = (xfi->workArea.width * settings->percent_screen) / 100;
		settings->height = (xfi->workArea.height * settings->percent_screen) / 100;
	}

	if (settings->fullscreen != true && settings->workarea != true)
		return true;

#ifdef WITH_XINERAMA
	if (XineramaQueryExtension(xfi->display, &ignored, &ignored2))
	{
		if (XineramaIsActive(xfi->display))
		{
			screen_info = XineramaQueryScreens(xfi->display, &vscreen->nmonitors);

			if (vscreen->nmonitors > 16)
				vscreen->nmonitors = 0;

			vscreen->monitors = xzalloc(sizeof(MONITOR_INFO) * vscreen->nmonitors);

			if (vscreen->nmonitors)
			{
				for (i = 0; i < vscreen->nmonitors; i++)
				{
					vscreen->monitors[i].area.left = screen_info[i].x_org;
					vscreen->monitors[i].area.top = screen_info[i].y_org;
					vscreen->monitors[i].area.right = screen_info[i].x_org + screen_info[i].width - 1;
					vscreen->monitors[i].area.bottom = screen_info[i].y_org + screen_info[i].height - 1;

					if ((screen_info[i].x_org == 0) && (screen_info[i].y_org == 0))
						vscreen->monitors[i].primary = true;
				}
			}

			XFree(screen_info);
		}
	}
#endif

	settings->num_monitors = vscreen->nmonitors;

	for (i = 0; i < vscreen->nmonitors; i++)
	{
		settings->monitors[i].x = vscreen->monitors[i].area.left;
		settings->monitors[i].y = vscreen->monitors[i].area.top;
		settings->monitors[i].width = vscreen->monitors[i].area.right - vscreen->monitors[i].area.left + 1;
		settings->monitors[i].height = vscreen->monitors[i].area.bottom - vscreen->monitors[i].area.top + 1;
		settings->monitors[i].is_primary = vscreen->monitors[i].primary;

		vscreen->area.left = MIN(vscreen->monitors[i].area.left, vscreen->area.left);
		vscreen->area.right = MAX(vscreen->monitors[i].area.right, vscreen->area.right);
		vscreen->area.top = MIN(vscreen->monitors[i].area.top, vscreen->area.top);
		vscreen->area.bottom = MAX(vscreen->monitors[i].area.bottom, vscreen->area.bottom);
	}

	if (settings->num_monitors)
	{
		settings->width = vscreen->area.right - vscreen->area.left + 1;
		settings->height = vscreen->area.bottom - vscreen->area.top + 1;
	}

	return true;
}
