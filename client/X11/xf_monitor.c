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
				printf("      %s [%d] %hdx%hd\t+%hd+%hd\n",
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
	printf("      * [0] %dx%d\t+0+0\n", WidthOfScreen(screen),
	       HeightOfScreen(screen));
	XCloseDisplay(display);
#endif
	return 0;
}

static BOOL xf_is_monitor_id_active(xfContext* xfc, UINT32 id)
{
	int index;
	rdpSettings* settings = xfc->context.settings;

	if (!settings->NumMonitorIds)
		return TRUE;

	for (index = 0; index < settings->NumMonitorIds; index++)
	{
		if (settings->MonitorIds[index] == id)
			return TRUE;
	}

	return FALSE;
}

BOOL xf_detect_monitors(xfContext* xfc, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	int i;
	int nmonitors = 0;
	int monitor_index = 0;
	BOOL primaryMonitorFound = FALSE;
	VIRTUAL_SCREEN* vscreen;
	rdpSettings* settings = xfc->context.settings;
	int mouse_x, mouse_y, _dummy_i;
	Window _dummy_w;
	int current_monitor = 0;
	Screen* screen;
#ifdef WITH_XINERAMA
	int major, minor;
	XineramaScreenInfo* screenInfo = NULL;
#endif
	vscreen = &xfc->vscreen;
	*pMaxWidth = settings->DesktopWidth;
	*pMaxHeight = settings->DesktopHeight;

	/* get mouse location */
	if (!XQueryPointer(xfc->display, DefaultRootWindow(xfc->display),
	                   &_dummy_w, &_dummy_w, &mouse_x, &mouse_y,
	                   &_dummy_i, &_dummy_i, (void*) &_dummy_i))
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
					vscreen->monitors[i].area.bottom = screenInfo[i].y_org + screenInfo[i].height -
					                                   1;

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
		*pMaxWidth = WidthOfScreen(xfc->screen);
		*pMaxHeight = HeightOfScreen(xfc->screen);
	}
	else if (settings->Workarea)
	{
		*pMaxWidth = xfc->workArea.width;
		*pMaxHeight = xfc->workArea.height;
	}
	else if (settings->PercentScreen)
	{
		/* If we have specific monitor information then limit the PercentScreen value
		 * to only affect the current monitor vs. the entire desktop
		 */
		if (vscreen->nmonitors > 0)
		{
			*pMaxWidth = vscreen->monitors[current_monitor].area.right -
			             vscreen->monitors[current_monitor].area.left + 1;
			*pMaxHeight = vscreen->monitors[current_monitor].area.bottom -
			              vscreen->monitors[current_monitor].area.top + 1;

			if (settings->PercentScreenUseWidth)
				*pMaxWidth = ((vscreen->monitors[current_monitor].area.right -
				               vscreen->monitors[current_monitor].area.left + 1) * settings->PercentScreen) /
				             100;

			if (settings->PercentScreenUseHeight)
				*pMaxHeight = ((vscreen->monitors[current_monitor].area.bottom -
				                vscreen->monitors[current_monitor].area.top + 1) * settings->PercentScreen) /
				              100;
		}
		else
		{
			*pMaxWidth = xfc->workArea.width;
			*pMaxHeight = xfc->workArea.height;

			if (settings->PercentScreenUseWidth)
				*pMaxWidth = (xfc->workArea.width * settings->PercentScreen) / 100;

			if (settings->PercentScreenUseHeight)
				*pMaxHeight = (xfc->workArea.height * settings->PercentScreen) / 100;
		}
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
		settings->MonitorDefArray[nmonitors].width = MIN(vscreen->monitors[i].area.right
		        - vscreen->monitors[i].area.left + 1, *pMaxWidth);
		settings->MonitorDefArray[nmonitors].height = MIN(
		            vscreen->monitors[i].area.bottom - vscreen->monitors[i].area.top + 1,
		            *pMaxHeight);
		settings->MonitorDefArray[nmonitors].orig_screen = i;

		if (i == settings->MonitorIds[0])
		{
			settings->MonitorDefArray[nmonitors].is_primary = TRUE;
			primaryMonitorFound = TRUE;
		}

		nmonitors++;
	}

	/* If no monitor is active(bogus command-line monitor specification) - then lets try to fallback to go fullscreen on the current monitor only */
	if (nmonitors == 0 && vscreen->nmonitors > 0)
	{
		settings->MonitorDefArray[0].x = vscreen->monitors[current_monitor].area.left;
		settings->MonitorDefArray[0].y = vscreen->monitors[current_monitor].area.top;
		settings->MonitorDefArray[0].width = MIN(
		        vscreen->monitors[current_monitor].area.right -
		        vscreen->monitors[current_monitor].area.left + 1, *pMaxWidth);
		settings->MonitorDefArray[0].height = MIN(
		        vscreen->monitors[current_monitor].area.bottom -
		        vscreen->monitors[current_monitor].area.top + 1, *pMaxHeight);
		settings->MonitorDefArray[0].orig_screen = current_monitor;
		nmonitors = 1;
	}

	settings->MonitorCount = nmonitors;

	/* If we have specific monitor information */
	if (settings->MonitorCount)
	{
		/* Initialize bounding rectangle for all monitors */
		int vX = settings->MonitorDefArray[0].x;
		int vY = settings->MonitorDefArray[0].y;
		int vR = vX + settings->MonitorDefArray[0].width;
		int vB = vY + settings->MonitorDefArray[0].height;
		xfc->fullscreenMonitors.top = xfc->fullscreenMonitors.bottom =
		                                  xfc->fullscreenMonitors.left = xfc->fullscreenMonitors.right =
		                                          settings->MonitorDefArray[0].orig_screen;

		/* Calculate bounding rectangle around all monitors to be used AND
		 * also set the Xinerama indices which define left/top/right/bottom monitors.
		 */
		for (i = 1; i < settings->MonitorCount; i++)
		{
			/* does the same as gdk_rectangle_union */
			int destX = MIN(vX, settings->MonitorDefArray[i].x);
			int destY = MIN(vY, settings->MonitorDefArray[i].y);
			int destR = MAX(vR, settings->MonitorDefArray[i].x +
			                settings->MonitorDefArray[i].width);
			int destB = MAX(vB, settings->MonitorDefArray[i].y +
			                settings->MonitorDefArray[i].height);

			if (vX != destX)
				xfc->fullscreenMonitors.left = settings->MonitorDefArray[i].orig_screen;

			if (vY != destY)
				xfc->fullscreenMonitors.top = settings->MonitorDefArray[i].orig_screen;

			if (vR != destR)
				xfc->fullscreenMonitors.right = settings->MonitorDefArray[i].orig_screen;

			if (vB != destB)
				xfc->fullscreenMonitors.bottom = settings->MonitorDefArray[i].orig_screen;

			vX = destX;
			vY = destY;
			vR = destR;
			vB = destB;
		}

		settings->DesktopPosX = vX;
		settings->DesktopPosY = vY;
		vscreen->area.left = 0;
		vscreen->area.right = vR - vX - 1;
		vscreen->area.top = 0;
		vscreen->area.bottom = vB - vY - 1;

		if (settings->Workarea)
		{
			vscreen->area.top = xfc->workArea.y;
			vscreen->area.bottom = xfc->workArea.height + xfc->workArea.y - 1;
		}

		if (!primaryMonitorFound)
		{
			/* If we have a command line setting we should use it */
			if (settings->NumMonitorIds)
			{
				/* The first monitor is the first in the setting which should be used */
				monitor_index =  settings->MonitorIds[0];
			}
			else
			{
				/* This is the same as when we would trust the Xinerama results..
				   and set the monitor index to zero.
				   The monitor listed with /monitor-list on index zero is always the primary
				*/
				screen = DefaultScreenOfDisplay(xfc->display);
				monitor_index = XScreenNumberOfScreen(screen);
			}

			int j = monitor_index;

			/* If the "default" monitor is not 0,0 use it */
			if (settings->MonitorDefArray[j].x != 0 || settings->MonitorDefArray[j].y != 0)
			{
				settings->MonitorDefArray[j].is_primary = TRUE;
				settings->MonitorLocalShiftX = settings->MonitorDefArray[j].x;
				settings->MonitorLocalShiftY = settings->MonitorDefArray[j].y;
				primaryMonitorFound = TRUE;
			}
			else
			{
				/* Lets try to see if there is a monitor with a 0,0 coordinate and use it as a fallback*/
				for (i = 0; i < settings->MonitorCount; i++)
				{
					if (!primaryMonitorFound && settings->MonitorDefArray[i].x == 0
					    && settings->MonitorDefArray[i].y == 0)
					{
						settings->MonitorDefArray[i].is_primary = TRUE;
						settings->MonitorLocalShiftX = settings->MonitorDefArray[i].x;
						settings->MonitorLocalShiftY = settings->MonitorDefArray[i].y;
						primaryMonitorFound = TRUE;
					}
				}
			}
		}

		/* Subtract monitor shift from monitor variables for server-side use.
		 * We maintain monitor shift value as Window requires the primary monitor to have a coordinate of 0,0
		 * In some X configurations, no monitor may have a coordinate of 0,0. This can also be happen if the user
		 * requests specific monitors from the command-line as well. So, we make sure to translate our primary monitor's
		 * upper-left corner to 0,0 on the server.
		 */
		for (i = 0; i < settings->MonitorCount; i++)
		{
			settings->MonitorDefArray[i].x = settings->MonitorDefArray[i].x -
			                                 settings->MonitorLocalShiftX;
			settings->MonitorDefArray[i].y = settings->MonitorDefArray[i].y -
			                                 settings->MonitorLocalShiftY;
		}

		/* Set the desktop width and height according to the bounding rectangle around the active monitors */
		*pMaxWidth = vscreen->area.right - vscreen->area.left + 1;
		*pMaxHeight = vscreen->area.bottom - vscreen->area.top + 1;
	}

	/* some 2008 server freeze at logon if we announce support for monitor layout PDU with
	 * #monitors < 2. So let's announce it only if we have more than 1 monitor.
	 */
	if (settings->MonitorCount)
		settings->SupportMonitorLayoutPdu = TRUE;

	return TRUE;
}
