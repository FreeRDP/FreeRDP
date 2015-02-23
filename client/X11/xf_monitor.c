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
	int nmonitors = 0;
	int primaryMonitorFound = FALSE;
	int vX, vY, vWidth, vHeight;
	int maxWidth, maxHeight;
	VIRTUAL_SCREEN* vscreen;
	rdpSettings* settings = xfc->settings;

	int mouse_x, mouse_y, _dummy_i;
	Window _dummy_w;
	int current_monitor = 0;

#ifdef WITH_XINERAMA
	int major, minor;
	XineramaScreenInfo* screenInfo = NULL;
#endif

	vscreen = &xfc->vscreen;
	xfc->desktopWidth = settings->DesktopWidth;
	xfc->desktopHeight = settings->DesktopHeight;

	/* get mouse location */
	if (!XQueryPointer(xfc->display, DefaultRootWindow(xfc->display),
			&_dummy_w, &_dummy_w, &mouse_x, &mouse_y,
			&_dummy_i, &_dummy_i, (void *) &_dummy_i))
		mouse_x = mouse_y = 0;

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

					/* Determine which monitor that the mouse cursor is on */
					if ((mouse_x >= vscreen->monitors[i].area.left) && 
					    (mouse_x <= vscreen->monitors[i].area.right) &&
					    (mouse_y >= vscreen->monitors[i].area.top) &&
					    (mouse_y <= vscreen->monitors[i].area.bottom))
						current_monitor = i;
				}
			}

			XFree(screenInfo);
		}
	}
#endif

	xfc->fullscreenMonitors.top = xfc->fullscreenMonitors.bottom =
	xfc->fullscreenMonitors.left = xfc->fullscreenMonitors.right = 0;

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

		/* If we have specific monitor information then limit the PercentScreen value
		 * to only affect the current monitor vs. the entire desktop
		 */
		if (vscreen->nmonitors > 0)
		{
			settings->DesktopWidth = ((vscreen->monitors[current_monitor].area.right - vscreen->monitors[current_monitor].area.left + 1) * settings->PercentScreen) / 100;
			settings->DesktopHeight = ((vscreen->monitors[current_monitor].area.bottom - vscreen->monitors[current_monitor].area.top + 1) * settings->PercentScreen) / 100;
		}

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

	/* If single monitor fullscreen OR workarea without remote app */
	if ((settings->Fullscreen && !settings->UseMultimon && !settings->SpanMonitors) ||
			(settings->Workarea && !settings->RemoteApplicationMode))
	{
		/* If no monitors were specified on the command-line then set the current monitor as active */
		if (!settings->NumMonitorIds)
		{
			settings->MonitorIds[0] = current_monitor;
		}

		/* Always sets number of monitors from command-line to just 1.
		 * If the monitor is invalid then we will default back to current monitor
		 * later as a fallback. So, there is no need to validate command-line entry here.
		 */
		settings->NumMonitorIds = 1;
	}

	/* Create array of all active monitors by taking into account monitors requested on the command-line */
	for (i = 0; i < vscreen->nmonitors; i++)
	{
		if (!xf_is_monitor_id_active(xfc, i))
			continue;

		settings->MonitorDefArray[nmonitors].x = vscreen->monitors[i].area.left;
		settings->MonitorDefArray[nmonitors].y = vscreen->monitors[i].area.top;
		settings->MonitorDefArray[nmonitors].width = MIN(vscreen->monitors[i].area.right - vscreen->monitors[i].area.left + 1, xfc->desktopWidth);
		settings->MonitorDefArray[nmonitors].height = MIN(vscreen->monitors[i].area.bottom - vscreen->monitors[i].area.top + 1, xfc->desktopHeight);
		settings->MonitorDefArray[nmonitors].orig_screen = i;

		nmonitors++;
	}

	/* If no monitor is active(bogus command-line monitor specification) - then lets try to fallback to go fullscreen on the current monitor only */
	if (nmonitors == 0 && vscreen->nmonitors > 0)
	{
		settings->MonitorDefArray[0].x = vscreen->monitors[current_monitor].area.left;
		settings->MonitorDefArray[0].y = vscreen->monitors[current_monitor].area.top;
		settings->MonitorDefArray[0].width = MIN(vscreen->monitors[current_monitor].area.right - vscreen->monitors[current_monitor].area.left + 1, xfc->desktopWidth);
		settings->MonitorDefArray[0].height = MIN(vscreen->monitors[current_monitor].area.bottom - vscreen->monitors[current_monitor].area.top + 1, xfc->desktopHeight);
		settings->MonitorDefArray[0].orig_screen = current_monitor;

		nmonitors = 1;
	}

	settings->MonitorCount = nmonitors;

	/* If we have specific monitor information */
	if (settings->MonitorCount)
	{
		/* Initialize bounding rectangle for all monitors */
		vWidth = settings->MonitorDefArray[0].width;
		vHeight = settings->MonitorDefArray[0].height;
		vX = settings->MonitorDefArray[0].x;
		vY = settings->MonitorDefArray[0].y;
		xfc->fullscreenMonitors.top = xfc->fullscreenMonitors.bottom =
		xfc->fullscreenMonitors.left = xfc->fullscreenMonitors.right = settings->MonitorDefArray[0].orig_screen;

		/* Calculate bounding rectangle around all monitors to be used AND
		 * also set the Xinerama indices which define left/top/right/bottom monitors.
		 */
		for (i = 1; i < settings->MonitorCount; i++)
		{
			/* does the same as gdk_rectangle_union */
			int destX = MIN(vX, settings->MonitorDefArray[i].x);
			int destY = MIN(vY, settings->MonitorDefArray[i].y);
			int destWidth = MAX(vX + vWidth, settings->MonitorDefArray[i].x + settings->MonitorDefArray[i].width) - destX;
			int destHeight = MAX(vY + vHeight, settings->MonitorDefArray[i].y + settings->MonitorDefArray[i].height) - destY;

			if (vX != destX)
				xfc->fullscreenMonitors.left = settings->MonitorDefArray[i].orig_screen;
			if (vY != destY)
				xfc->fullscreenMonitors.top = settings->MonitorDefArray[i].orig_screen;
			if (vWidth != destWidth)
				xfc->fullscreenMonitors.right = settings->MonitorDefArray[i].orig_screen;
			if (vHeight != destHeight)
				xfc->fullscreenMonitors.bottom = settings->MonitorDefArray[i].orig_screen;

			vX = destX;
			vY = destY;
			vWidth = destWidth;
			vHeight = destHeight;
		}

		settings->DesktopPosX = vX;
		settings->DesktopPosY = vY;

		vscreen->area.left = 0;
		vscreen->area.right = vWidth - 1;
		vscreen->area.top = 0;
		vscreen->area.bottom = vHeight - 1;

		if (settings->Workarea)
		{
			vscreen->area.top = xfc->workArea.y;
			vscreen->area.bottom = (vHeight - (vHeight - (xfc->workArea.height + xfc->workArea.y))) - 1;
		}

		/* If there are multiple monitors and we have not selected a primary */
		if (!primaryMonitorFound)
		{       
			/* First lets try to see if there is a monitor with a 0,0 coordinate */
			for (i=0; i<settings->MonitorCount; i++)
			{
				if (!primaryMonitorFound && settings->MonitorDefArray[i].x == 0 && settings->MonitorDefArray[i].y == 0)
				{
					settings->MonitorDefArray[i].is_primary = TRUE;
					settings->MonitorLocalShiftX = settings->MonitorDefArray[i].x;
					settings->MonitorLocalShiftY = settings->MonitorDefArray[i].y;
					primaryMonitorFound = TRUE;
				}
			}

			/* If we still do not have a primary monitor then just arbitrarily choose first monitor */
			if (!primaryMonitorFound)
			{
				settings->MonitorDefArray[0].is_primary = TRUE;
				settings->MonitorLocalShiftX = settings->MonitorDefArray[0].x;
				settings->MonitorLocalShiftY = settings->MonitorDefArray[0].y;
				primaryMonitorFound = TRUE;
			}
		}

		/* Subtract monitor shift from monitor variables for server-side use. 
		 * We maintain monitor shift value as Window requires the primary monitor to have a coordinate of 0,0
		 * In some X configurations, no monitor may have a coordinate of 0,0. This can also be happen if the user
		 * requests specific monitors from the command-line as well. So, we make sure to translate our primary monitor's
		 * upper-left corner to 0,0 on the server.
		 */
		for (i=0; i < settings->MonitorCount; i++)
		{
			settings->MonitorDefArray[i].x = settings->MonitorDefArray[i].x - settings->MonitorLocalShiftX;
			settings->MonitorDefArray[i].y = settings->MonitorDefArray[i].y - settings->MonitorLocalShiftY;
		}

		/* Set the desktop width and height according to the bounding rectangle around the active monitors */
		xfc->desktopWidth = vscreen->area.right - vscreen->area.left + 1;
		xfc->desktopHeight = vscreen->area.bottom - vscreen->area.top + 1;
	}

	return TRUE;
}
