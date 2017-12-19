/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Display Control channel
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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

#include <X11/Xutil.h>

#ifdef WITH_XRANDR
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>
#endif

#include "xf_disp.h"
#include "xf_monitor.h"


#define TAG CLIENT_TAG("x11disp")

struct _xfDispContext
{
	xfContext *xfc;
	BOOL haveXRandr;
	int eventBase, errorBase;
	int lastWidth, lastHeight;
	BOOL activated;
	BOOL waitingResize;
};

xfDispContext *xf_disp_new(xfContext* xfc)
{
	xfDispContext *ret = calloc(1, sizeof(xfDispContext));
	if (!ret)
		return NULL;

	ret->xfc = xfc;
#ifdef WITH_XRANDR
	if (XRRQueryExtension(xfc->display, &ret->eventBase, &ret->errorBase))
	{
		ret->haveXRandr = TRUE;
	}
#endif
	ret->lastWidth = xfc->context.settings->DesktopWidth;
	ret->lastHeight = xfc->context.settings->DesktopHeight;

	return ret;
}

void xf_disp_free(xfDispContext *disp)
{
	free(disp);
}

static UINT xf_disp_sendLayout(DispClientContext *disp, rdpMonitor *monitors, int nmonitors)
{
	UINT ret = CHANNEL_RC_OK;
	DISPLAY_CONTROL_MONITOR_LAYOUT *layouts;
	int i;

	layouts = calloc(nmonitors, sizeof(DISPLAY_CONTROL_MONITOR_LAYOUT));
	if (!layouts)
		return CHANNEL_RC_NO_MEMORY;

	for (i = 0; i < nmonitors; i++)
	{
		layouts[i].Flags = (monitors[i].is_primary ? DISPLAY_CONTROL_MONITOR_PRIMARY : 0);
		layouts[i].Left = monitors[i].x;
		layouts[i].Top = monitors[i].y;
		layouts[i].Width = monitors[i].width;
		layouts[i].Height = monitors[i].height;
		layouts[i].Orientation = ORIENTATION_LANDSCAPE;
		layouts[i].PhysicalWidth = monitors[i].width;
		layouts[i].PhysicalHeight = monitors[i].height;
		layouts[i].DesktopScaleFactor = 100;
		layouts[i].DeviceScaleFactor = 100;

	}

	ret = disp->SendMonitorLayout(disp, nmonitors, layouts);

	free(layouts);

	return ret;
}

BOOL xf_disp_handle_xevent(xfContext *xfc, XEvent *event)
{
	xfDispContext *xfDisp = xfc->xfDisp;
	rdpSettings *settings = xfc->context.settings;
	UINT32 maxWidth, maxHeight;

	if (!xfDisp->haveXRandr)
		return TRUE;

#ifdef WITH_XRANDR
	if (event->type != xfDisp->eventBase + RRScreenChangeNotify)
		return TRUE;
#endif

	xf_detect_monitors(xfc, &maxWidth, &maxHeight);
	return xf_disp_sendLayout(xfc->disp, settings->MonitorDefArray, settings->MonitorCount) == CHANNEL_RC_OK;
}

BOOL xf_disp_handle_resize(xfContext *xfc, int width, int height)
{
	DISPLAY_CONTROL_MONITOR_LAYOUT layout;
	xfDispContext *xfDisp = xfc->xfDisp;

	if (xfDisp->lastWidth == width && xfDisp->lastHeight == height)
		return TRUE;

	if (xfDisp->waitingResize || !xfDisp->activated)
		return TRUE;

	xfDisp->lastWidth = width;
	xfDisp->lastHeight = height;
	xfDisp->waitingResize = TRUE;

	layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
	layout.Top = layout.Left = 0;
	layout.Width = width;
	layout.Height = height;
	layout.Orientation = ORIENTATION_LANDSCAPE;
	layout.DesktopScaleFactor = 100;
	layout.DeviceScaleFactor = 100;
	layout.PhysicalWidth = width;
	layout.PhysicalHeight = height;

	return xfc->disp->SendMonitorLayout(xfc->disp, 1, &layout) == CHANNEL_RC_OK;
}

BOOL xf_disp_set_window_resizable(xfDispContext *xfDisp)
{
	XSizeHints *size_hints;

	if (!(size_hints = XAllocSizeHints()))
			return FALSE;

	size_hints->flags = PMinSize | PMaxSize | PWinGravity;
	size_hints->win_gravity = NorthWestGravity;
	size_hints->min_width = size_hints->min_height = 320;
	size_hints->max_width = size_hints->max_height = 8192;
	XSetWMNormalHints(xfDisp->xfc->display, xfDisp->xfc->window->handle, size_hints);
	XFree(size_hints);
	return TRUE;
}

void xf_disp_resized(xfDispContext *xfDisp)
{
	rdpSettings *settings = xfDisp->xfc->context.settings;

	xfDisp->waitingResize = FALSE;

	if (xfDisp->activated && !settings->Fullscreen)
	{
		xf_disp_set_window_resizable(xfDisp);
	}
}

UINT xf_DisplayControlCaps(DispClientContext *disp, UINT32 maxNumMonitors, UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB)
{
	/* we're called only if dynamic resolution update is activated */
	xfDispContext *xfDisp = (xfDispContext *)disp->custom;
	rdpSettings *settings = xfDisp->xfc->context.settings;

	WLog_DBG(TAG, "DisplayControlCapsPdu: MaxNumMonitors: %"PRIu32" MaxMonitorAreaFactorA: %"PRIu32" MaxMonitorAreaFactorB: %"PRIu32"",
	       maxNumMonitors, maxMonitorAreaFactorA, maxMonitorAreaFactorB);

	xfDisp->activated = TRUE;

	if (settings->Fullscreen)
		return CHANNEL_RC_OK;

	WLog_DBG(TAG, "DisplayControlCapsPdu: setting the window as resizeable");
	return xf_disp_set_window_resizable(xfDisp) ? CHANNEL_RC_OK : CHANNEL_RC_NO_MEMORY;
}

BOOL xf_disp_init(xfContext* xfc, DispClientContext *disp)
{
	rdpSettings *settings = xfc->context.settings;
	xfc->disp = disp;
	disp->custom = (void*) xfc->xfDisp;

	if (settings->DynamicResolutionUpdate)
	{
		disp->DisplayControlCaps = xf_DisplayControlCaps;
#ifdef WITH_XRANDR
		if (settings->Fullscreen)
		{
			/* ask X11 to notify us of screen changes */
			XRRSelectInput(xfc->display, DefaultRootWindow(xfc->display), RRScreenChangeNotifyMask);
		}
#endif
	}

	return TRUE;
}

