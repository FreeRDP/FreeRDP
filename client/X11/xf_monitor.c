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

#include <freerdp/log.h>

#define TAG CLIENT_TAG("x11")

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "xf_monitor.h"

/* See MSDN Section on Multiple Display Monitors: http://msdn.microsoft.com/en-us/library/dd145071 */

int xf_list_monitors(xfContext* xfc)
{
#ifdef WITH_XINERAMA
	Display* display;
	int major, minor;
	int i, nmonitors = 0;
	XineramaScreenInfo* screen = NULL;

	display = XOpenDisplay(NULL);

	if (!display)
	{
		WLog_ERR(TAG, "failed to open X display");
		return -1;
	}

	if (XineramaQueryExtension(display, &major, &minor))
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

	if (!display)
	{
		WLog_ERR(TAG, "failed to open X display");
		return -1;
	}

	screen = ScreenOfDisplay(display, DefaultScreen(display));

	printf("      * [0] %dx%d\t+%d+%d\n", WidthOfScreen(screen), HeightOfScreen(screen), 0, 0);

	XCloseDisplay(display);
#endif

	return 0;
}

BOOL xf_is_monitor_id_active(xfContext* xfc, UINT32 id)
{
	int index;
	rdpSettings* settings = xfc->settings;

	if (!settings->NumMonitorIds)
		return TRUE;

	for (index = 0; index < settings->NumMonitorIds; index++)
	{
		if (settings->MonitorIds[index] == id)
			return TRUE;
	}

	return FALSE;
}

BOOL xf_detect_monitors(xfContext* xfc)
{
	int i;
	int nmonitors;
	int primaryMonitor;
	int vWidth, vHeight;
	int maxWidth, maxHeight;
	VIRTUAL_SCREEN* vscreen;
	rdpSettings* settings = xfc->settings;

#ifdef WITH_XINERAMA
	int major, minor;
	XineramaScreenInfo* screenInfo = NULL;
#endif

	vscreen = &xfc->vscreen;
	xfc->desktopWidth = settings->DesktopWidth;
	xfc->desktopHeight = settings->DesktopHeight;

#ifdef WITH_XINERAMA
	if (XineramaQueryExtension(xfc->display, &major, &minor))
	{
		if (XineramaIsActive(xfc->display))
		{
			screenInfo = XineramaQueryScreens(xfc->display, &vscreen->nmonitors);

			if (vscreen->nmonitors > 16)
				vscreen->nmonitors = 0;

			if (vscreen->nmonitors)
			{
				for (i = 0; i < vscreen->nmonitors; i++)
				{
					vscreen->monitors[i].area.left = screenInfo[i].x_org;
					vscreen->monitors[i].area.top = screenInfo[i].y_org;
					vscreen->monitors[i].area.right = screenInfo[i].x_org + screenInfo[i].width - 1;
					vscreen->monitors[i].area.bottom = screenInfo[i].y_org + screenInfo[i].height - 1;

					if ((screenInfo[i].x_org == 0) && (screenInfo[i].y_org == 0))
						vscreen->monitors[i].primary = TRUE;
				}
			}

			XFree(screenInfo);
		}
	}
#endif

	/* WORKAROUND: With Remote Application Mode - using NET_WM_WORKAREA
 	 * causes issues with the ability to fully size the window vertically
 	 * (the bottom of the window area is never updated). So, we just set
 	 * the workArea to match the full Screen width/height.
	 */
	if (settings->RemoteApplicationMode || !xf_GetWorkArea(xfc))
	{
		xfc->workArea.x = 0;
		xfc->workArea.y = 0;
		xfc->workArea.width = WidthOfScreen(xfc->screen);
		xfc->workArea.height = HeightOfScreen(xfc->screen);
	}

	if (settings->Fullscreen)
	{
		xfc->desktopWidth = WidthOfScreen(xfc->screen);
		xfc->desktopHeight = HeightOfScreen(xfc->screen);
		maxWidth = xfc->desktopWidth;
		maxHeight = xfc->desktopHeight;
	}
	else if (settings->Workarea)
	{
		xfc->desktopWidth = xfc->workArea.width;
		xfc->desktopHeight = xfc->workArea.height;
		maxWidth = xfc->desktopWidth;
		maxHeight = xfc->desktopHeight;
	}
	else if (settings->PercentScreen)
	{
		xfc->desktopWidth = (xfc->workArea.width * settings->PercentScreen) / 100;
		xfc->desktopHeight = (xfc->workArea.height * settings->PercentScreen) / 100;
		maxWidth = xfc->desktopWidth;
		maxHeight = xfc->desktopHeight;
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
		if (!xf_is_monitor_id_active(xfc, i))
			continue;

		settings->MonitorDefArray[nmonitors].x = vscreen->monitors[i].area.left;
		settings->MonitorDefArray[nmonitors].y = vscreen->monitors[i].area.top;
		settings->MonitorDefArray[nmonitors].width = MIN(vscreen->monitors[i].area.right - vscreen->monitors[i].area.left + 1, xfc->desktopWidth);
		settings->MonitorDefArray[nmonitors].height = MIN(vscreen->monitors[i].area.bottom - vscreen->monitors[i].area.top + 1, xfc->desktopHeight);
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
		xfc->desktopWidth = vscreen->area.right - vscreen->area.left + 1;
		xfc->desktopHeight = vscreen->area.bottom - vscreen->area.top + 1;
	}

	return TRUE;
}
