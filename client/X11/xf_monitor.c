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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <winpr/crt.h>

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "xf_monitor.h"

/* See MSDN Section on Multiple Display Monitors: http://msdn.microsoft.com/en-us/library/dd145071 */

int xf_list_monitors(xfContext* xfc)
{
#ifdef WITH_XINERAMA
	Display* display;
	int i, nmonitors = 0;
	int ignored, ignored2;
	XineramaScreenInfo* screen = NULL;

	display = XOpenDisplay(NULL);

	if (XineramaQueryExtension(display, &ignored, &ignored2))
	{
		if (XineramaIsActive(display))
		{
			screen = XineramaQueryScreens(display, &nmonitors);

			for (i = 0; i < nmonitors; i++)
			{
				printf("      %s [%d] %dx%d\t+%d+%d\n",
				       (i == 0) ? "*" : " ", i,
				       screen[i].width, screen[i].height,
				       screen[i].x_org, screen[i].y_org);
			}

			XFree(screen);
		}
	}

	XCloseDisplay(display);
#else
	Screen* screen;
	Display* display;

	display = XOpenDisplay(NULL);

	screen = ScreenOfDisplay(display, DefaultScreen(display));
	printf("      * [0] %dx%d\t+%d+%d\n", WidthOfScreen(screen), HeightOfScreen(screen), 0, 0);

	XCloseDisplay(display);
#endif

	return 0;
}

BOOL xf_detect_monitors(xfContext* xfc, rdpSettings* settings)
{
	int i, j;
	int nmonitors;
	int primaryMonitor;
	int vWidth, vHeight;
	int maxWidth, maxHeight;
	VIRTUAL_SCREEN* vscreen;

#ifdef WITH_XINERAMA
	int ignored, ignored2;
	XineramaScreenInfo* screen_info = NULL;
#endif

	vscreen = &xfc->vscreen;

#ifdef WITH_XINERAMA
	if (XineramaQueryExtension(xfc->display, &ignored, &ignored2))
	{
		if (XineramaIsActive(xfc->display))
		{
			screen_info = XineramaQueryScreens(xfc->display, &vscreen->nmonitors);

			if (vscreen->nmonitors > 16)
				vscreen->nmonitors = 0;

			vscreen->monitors = malloc(sizeof(MONITOR_INFO) * vscreen->nmonitors);
			ZeroMemory(vscreen->monitors, sizeof(MONITOR_INFO) * vscreen->nmonitors);

			if (vscreen->nmonitors)
			{
				for (i = 0; i < vscreen->nmonitors; i++)
				{
					vscreen->monitors[i].area.left = screen_info[i].x_org;
					vscreen->monitors[i].area.top = screen_info[i].y_org;
					vscreen->monitors[i].area.right = screen_info[i].x_org + screen_info[i].width - 1;
					vscreen->monitors[i].area.bottom = screen_info[i].y_org + screen_info[i].height - 1;

					if ((screen_info[i].x_org == 0) && (screen_info[i].y_org == 0))
						vscreen->monitors[i].primary = TRUE;
				}
			}

			XFree(screen_info);
		}
	}
#endif

	if (!xf_GetWorkArea(xfc))
	{
		xfc->workArea.x = 0;
		xfc->workArea.y = 0;
		xfc->workArea.width = WidthOfScreen(xfc->screen);
		xfc->workArea.height = HeightOfScreen(xfc->screen);
	}

	if (settings->Fullscreen)
	{
		settings->DesktopWidth = WidthOfScreen(xfc->screen);
		settings->DesktopHeight = HeightOfScreen(xfc->screen);
		maxWidth = settings->DesktopWidth;
		maxHeight = settings->DesktopHeight;
	}
	else if (settings->Workarea)
	{
		settings->DesktopWidth = xfc->workArea.width;
		settings->DesktopHeight = xfc->workArea.height;
		maxWidth = settings->DesktopWidth;
		maxHeight = settings->DesktopHeight;
	}
	else if (settings->PercentScreen)
	{
		settings->DesktopWidth = (xfc->workArea.width * settings->PercentScreen) / 100;
		settings->DesktopHeight = (xfc->workArea.height * settings->PercentScreen) / 100;
		maxWidth = settings->DesktopWidth;
		maxHeight = settings->DesktopHeight;
	}
	else
	{
		maxWidth = WidthOfScreen(xfc->screen);
		maxHeight = HeightOfScreen(xfc->screen);
	}

	if (!settings->Fullscreen && !settings->Workarea && !settings->UseMultimon)
		return TRUE;

	if ((settings->Fullscreen && !settings->UseMultimon && !settings->SpanMonitors) ||
			(settings->Workarea && !settings->RemoteApplicationMode))
	{
		/* Select a single monitor */

		if (settings->NumMonitorIds != 1)
		{
			settings->NumMonitorIds = 1;
			settings->MonitorIds = (UINT32*) malloc(sizeof(UINT32) * settings->NumMonitorIds);
			settings->MonitorIds[0] = 0;

			for (i = 0; i < vscreen->nmonitors; i++)
			{
				if (vscreen->monitors[i].primary)
				{
					settings->MonitorIds[0] = i;
					break;
				}
			}
		}
	}

	nmonitors = 0;
	primaryMonitor = 0;

	for (i = 0; i < vscreen->nmonitors; i++)
	{
		if (settings->NumMonitorIds)
		{
			BOOL found = FALSE;

			for (j = 0; j < settings->NumMonitorIds; j++)
			{
				if (settings->MonitorIds[j] == i)
					found = TRUE;
			}

			if (!found)
				continue;
		}

		settings->MonitorDefArray[nmonitors].x = vscreen->monitors[i].area.left;
		settings->MonitorDefArray[nmonitors].y = vscreen->monitors[i].area.top;
		settings->MonitorDefArray[nmonitors].width = MIN(vscreen->monitors[i].area.right - vscreen->monitors[i].area.left + 1, settings->DesktopWidth);
		settings->MonitorDefArray[nmonitors].height = MIN(vscreen->monitors[i].area.bottom - vscreen->monitors[i].area.top + 1, settings->DesktopHeight);
		settings->MonitorDefArray[nmonitors].is_primary = vscreen->monitors[i].primary;

		primaryMonitor |= vscreen->monitors[i].primary;
		nmonitors++;
	}

	settings->MonitorCount = nmonitors;

	vWidth = vHeight = 0;
	settings->DesktopPosX = maxWidth - 1;
	settings->DesktopPosY = maxHeight - 1;

	for (i = 0; i < settings->MonitorCount; i++)
	{
		settings->DesktopPosX = MIN(settings->DesktopPosX, settings->MonitorDefArray[i].x);
		settings->DesktopPosY = MIN(settings->DesktopPosY, settings->MonitorDefArray[i].y);

		vWidth += settings->MonitorDefArray[i].width;
		vHeight = MAX(vHeight, settings->MonitorDefArray[i].height);
	}

	vscreen->area.left = 0;
	vscreen->area.right = vWidth - 1;
	vscreen->area.top = 0;
	vscreen->area.bottom = vHeight - 1;

	if (settings->Workarea)
	{
		vscreen->area.top = xfc->workArea.y;
		vscreen->area.bottom = (vHeight - (vHeight - (xfc->workArea.height + xfc->workArea.y))) - 1;
	}

	if (nmonitors && !primaryMonitor)
		settings->MonitorDefArray[0].is_primary = TRUE;
	
	if (settings->MonitorCount)
	{
		settings->DesktopWidth = vscreen->area.right - vscreen->area.left + 1;
		settings->DesktopHeight = vscreen->area.bottom - vscreen->area.top + 1;
	}

	return TRUE;
}
