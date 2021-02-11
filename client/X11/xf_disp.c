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

#include <winpr/sysinfo.h>
#include <X11/Xutil.h>

#ifdef WITH_XRANDR
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>

#if (RANDR_MAJOR * 100 + RANDR_MINOR) >= 105
#define USABLE_XRANDR
#endif

#endif

#include "xf_disp.h"
#include "xf_monitor.h"

#define TAG CLIENT_TAG("x11disp")
#define RESIZE_MIN_DELAY 200 /* minimum delay in ms between two resizes */

struct _xfDispContext
{
	xfContext* xfc;
	DispClientContext* disp;
	BOOL haveXRandr;
	int eventBase, errorBase;
	int lastSentWidth, lastSentHeight;
	UINT64 lastSentDate;
	int targetWidth, targetHeight;
	BOOL activated;
	BOOL fullscreen;
	UINT16 lastSentDesktopOrientation;
	UINT32 lastSentDesktopScaleFactor;
	UINT32 lastSentDeviceScaleFactor;
};

static UINT xf_disp_sendLayout(DispClientContext* disp, rdpMonitor* monitors, int nmonitors);

static BOOL xf_disp_settings_changed(xfDispContext* xfDisp)
{
	rdpSettings* settings = xfDisp->xfc->context.settings;

	if (xfDisp->lastSentWidth != xfDisp->targetWidth)
		return TRUE;

	if (xfDisp->lastSentHeight != xfDisp->targetHeight)
		return TRUE;

	if (xfDisp->lastSentDesktopOrientation != settings->DesktopOrientation)
		return TRUE;

	if (xfDisp->lastSentDesktopScaleFactor != settings->DesktopScaleFactor)
		return TRUE;

	if (xfDisp->lastSentDeviceScaleFactor != settings->DeviceScaleFactor)
		return TRUE;

	if (xfDisp->fullscreen != xfDisp->xfc->fullscreen)
		return TRUE;

	return FALSE;
}

static BOOL xf_update_last_sent(xfDispContext* xfDisp)
{
	rdpSettings* settings = xfDisp->xfc->context.settings;
	xfDisp->lastSentWidth = xfDisp->targetWidth;
	xfDisp->lastSentHeight = xfDisp->targetHeight;
	xfDisp->lastSentDesktopOrientation = settings->DesktopOrientation;
	xfDisp->lastSentDesktopScaleFactor = settings->DesktopScaleFactor;
	xfDisp->lastSentDeviceScaleFactor = settings->DeviceScaleFactor;
	xfDisp->fullscreen = xfDisp->xfc->fullscreen;
	return TRUE;
}

static BOOL xf_disp_sendResize(xfDispContext* xfDisp)
{
	DISPLAY_CONTROL_MONITOR_LAYOUT layout;
	xfContext* xfc;
	rdpSettings* settings;

	if (!xfDisp || !xfDisp->xfc)
		return FALSE;

	xfc = xfDisp->xfc;
	settings = xfc->context.settings;

	if (!settings)
		return FALSE;

	if (!xfDisp->activated || !xfDisp->disp)
		return TRUE;

	if (GetTickCount64() - xfDisp->lastSentDate < RESIZE_MIN_DELAY)
		return TRUE;

	if (!xf_disp_settings_changed(xfDisp))
		return TRUE;

	xfDisp->lastSentDate = GetTickCount64();
	if (xfc->fullscreen && (settings->MonitorCount > 0))
	{
		if (xf_disp_sendLayout(xfDisp->disp, settings->MonitorDefArray, settings->MonitorCount) !=
		    CHANNEL_RC_OK)
			return FALSE;
	}
	else
	{
		layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
		layout.Top = layout.Left = 0;
		layout.Width = xfDisp->targetWidth;
		layout.Height = xfDisp->targetHeight;
		layout.Orientation = settings->DesktopOrientation;
		layout.DesktopScaleFactor = settings->DesktopScaleFactor;
		layout.DeviceScaleFactor = settings->DeviceScaleFactor;
		layout.PhysicalWidth = xfDisp->targetWidth / 75 * 25.4f;
		layout.PhysicalHeight = xfDisp->targetHeight / 75 * 25.4f;

		if (IFCALLRESULT(CHANNEL_RC_OK, xfDisp->disp->SendMonitorLayout, xfDisp->disp, 1,
		                 &layout) != CHANNEL_RC_OK)
			return FALSE;
	}

	return xf_update_last_sent(xfDisp);
}

static BOOL xf_disp_queueResize(xfDispContext* xfDisp, UINT32 width, UINT32 height)
{
	if ((xfDisp->targetWidth == width) && (xfDisp->targetHeight == height))
		return TRUE;
	xfDisp->targetWidth = width;
	xfDisp->targetHeight = height;
	xfDisp->lastSentDate = GetTickCount64();
	return xf_disp_sendResize(xfDisp);
}

static BOOL xf_disp_set_window_resizable(xfDispContext* xfDisp)
{
	XSizeHints* size_hints;

	if (!(size_hints = XAllocSizeHints()))
		return FALSE;

	size_hints->flags = PMinSize | PMaxSize | PWinGravity;
	size_hints->win_gravity = NorthWestGravity;
	size_hints->min_width = size_hints->min_height = 320;
	size_hints->max_width = size_hints->max_height = 8192;

	if (xfDisp->xfc->window)
		XSetWMNormalHints(xfDisp->xfc->display, xfDisp->xfc->window->handle, size_hints);

	XFree(size_hints);
	return TRUE;
}

static BOOL xf_disp_check_context(void* context, xfContext** ppXfc, xfDispContext** ppXfDisp,
                                  rdpSettings** ppSettings)
{
	xfContext* xfc;

	if (!context)
		return FALSE;

	xfc = (xfContext*)context;

	if (!(xfc->xfDisp))
		return FALSE;

	if (!xfc->context.settings)
		return FALSE;

	*ppXfc = xfc;
	*ppXfDisp = xfc->xfDisp;
	*ppSettings = xfc->context.settings;
	return TRUE;
}

static void xf_disp_OnActivated(void* context, ActivatedEventArgs* e)
{
	xfContext* xfc;
	xfDispContext* xfDisp;
	rdpSettings* settings;

	if (!xf_disp_check_context(context, &xfc, &xfDisp, &settings))
		return;

	if (xfDisp->activated && !xfc->fullscreen)
	{
		xf_disp_set_window_resizable(xfDisp);

		if (e->firstActivation)
			return;

		xf_disp_sendResize(xfDisp);
	}
}

static void xf_disp_OnGraphicsReset(void* context, GraphicsResetEventArgs* e)
{
	xfContext* xfc;
	xfDispContext* xfDisp;
	rdpSettings* settings;

	WINPR_UNUSED(e);

	if (!xf_disp_check_context(context, &xfc, &xfDisp, &settings))
		return;

	if (xfDisp->activated && !settings->Fullscreen)
	{
		xf_disp_set_window_resizable(xfDisp);
		xf_disp_sendResize(xfDisp);
	}
}

static void xf_disp_OnTimer(void* context, TimerEventArgs* e)
{
	xfContext* xfc;
	xfDispContext* xfDisp;
	rdpSettings* settings;

	WINPR_UNUSED(e);

	if (!xf_disp_check_context(context, &xfc, &xfDisp, &settings))
		return;

	if (!xfDisp->activated || settings->Fullscreen)
		return;

	xf_disp_sendResize(xfDisp);
}

xfDispContext* xf_disp_new(xfContext* xfc)
{
	xfDispContext* ret;

	if (!xfc || !xfc->context.settings || !xfc->context.pubSub)
		return NULL;

	ret = calloc(1, sizeof(xfDispContext));

	if (!ret)
		return NULL;

	ret->xfc = xfc;
#ifdef USABLE_XRANDR

	if (XRRQueryExtension(xfc->display, &ret->eventBase, &ret->errorBase))
	{
		ret->haveXRandr = TRUE;
	}

#endif
	ret->lastSentWidth = ret->targetWidth = xfc->context.settings->DesktopWidth;
	ret->lastSentHeight = ret->targetHeight = xfc->context.settings->DesktopHeight;
	PubSub_SubscribeActivated(xfc->context.pubSub, xf_disp_OnActivated);
	PubSub_SubscribeGraphicsReset(xfc->context.pubSub, xf_disp_OnGraphicsReset);
	PubSub_SubscribeTimer(xfc->context.pubSub, xf_disp_OnTimer);
	return ret;
}

void xf_disp_free(xfDispContext* disp)
{
	if (!disp)
		return;

	if (disp->xfc)
	{
		PubSub_UnsubscribeActivated(disp->xfc->context.pubSub, xf_disp_OnActivated);
		PubSub_UnsubscribeGraphicsReset(disp->xfc->context.pubSub, xf_disp_OnGraphicsReset);
		PubSub_UnsubscribeTimer(disp->xfc->context.pubSub, xf_disp_OnTimer);
	}

	free(disp);
}

UINT xf_disp_sendLayout(DispClientContext* disp, rdpMonitor* monitors, int nmonitors)
{
	UINT ret = CHANNEL_RC_OK;
	DISPLAY_CONTROL_MONITOR_LAYOUT* layouts;
	int i;
	xfDispContext* xfDisp = (xfDispContext*)disp->custom;
	rdpSettings* settings = xfDisp->xfc->context.settings;
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
		layouts[i].PhysicalWidth = monitors[i].attributes.physicalWidth;
		layouts[i].PhysicalHeight = monitors[i].attributes.physicalHeight;

		switch (monitors[i].attributes.orientation)
		{
			case 90:
				layouts[i].Orientation = ORIENTATION_PORTRAIT;
				break;

			case 180:
				layouts[i].Orientation = ORIENTATION_LANDSCAPE_FLIPPED;
				break;

			case 270:
				layouts[i].Orientation = ORIENTATION_PORTRAIT_FLIPPED;
				break;

			case 0:
			default:
				/* MS-RDPEDISP - 2.2.2.2.1:
				 * Orientation (4 bytes): A 32-bit unsigned integer that specifies the
				 * orientation of the monitor in degrees. Valid values are 0, 90, 180
				 * or 270
				 *
				 * So we default to ORIENTATION_LANDSCAPE
				 */
				layouts[i].Orientation = ORIENTATION_LANDSCAPE;
				break;
		}

		layouts[i].DesktopScaleFactor = settings->DesktopScaleFactor;
		layouts[i].DeviceScaleFactor = settings->DeviceScaleFactor;
	}

	ret = IFCALLRESULT(CHANNEL_RC_OK, disp->SendMonitorLayout, disp, nmonitors, layouts);
	free(layouts);
	return ret;
}

BOOL xf_disp_handle_xevent(xfContext* xfc, const XEvent* event)
{
	xfDispContext* xfDisp;
	rdpSettings* settings;
	UINT32 maxWidth, maxHeight;

	if (!xfc || !event)
		return FALSE;

	xfDisp = xfc->xfDisp;

	if (!xfDisp)
		return FALSE;

	settings = xfc->context.settings;

	if (!settings)
		return FALSE;

	if (!xfDisp->haveXRandr || !xfDisp->disp)
		return TRUE;

#ifdef USABLE_XRANDR

	if (event->type != xfDisp->eventBase + RRScreenChangeNotify)
		return TRUE;

#endif
	xf_detect_monitors(xfc, &maxWidth, &maxHeight);
	return xf_disp_sendLayout(xfDisp->disp, settings->MonitorDefArray, settings->MonitorCount) ==
	       CHANNEL_RC_OK;
}

BOOL xf_disp_handle_configureNotify(xfContext* xfc, int width, int height)
{
	xfDispContext* xfDisp;

	if (!xfc)
		return FALSE;

	xfDisp = xfc->xfDisp;

	if (!xfDisp)
		return FALSE;

	return xf_disp_queueResize(xfDisp, width, height);
}

static UINT xf_DisplayControlCaps(DispClientContext* disp, UINT32 maxNumMonitors,
                                  UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB)
{
	/* we're called only if dynamic resolution update is activated */
	xfDispContext* xfDisp = (xfDispContext*)disp->custom;
	rdpSettings* settings = xfDisp->xfc->context.settings;
	WLog_DBG(TAG,
	         "DisplayControlCapsPdu: MaxNumMonitors: %" PRIu32 " MaxMonitorAreaFactorA: %" PRIu32
	         " MaxMonitorAreaFactorB: %" PRIu32 "",
	         maxNumMonitors, maxMonitorAreaFactorA, maxMonitorAreaFactorB);
	xfDisp->activated = TRUE;

	if (settings->Fullscreen)
		return CHANNEL_RC_OK;

	WLog_DBG(TAG, "DisplayControlCapsPdu: setting the window as resizable");
	return xf_disp_set_window_resizable(xfDisp) ? CHANNEL_RC_OK : CHANNEL_RC_NO_MEMORY;
}

BOOL xf_disp_init(xfDispContext* xfDisp, DispClientContext* disp)
{
	rdpSettings* settings;

	if (!xfDisp || !xfDisp->xfc || !disp)
		return FALSE;

	settings = xfDisp->xfc->context.settings;

	if (!settings)
		return FALSE;

	xfDisp->disp = disp;
	disp->custom = (void*)xfDisp;

	if (settings->DynamicResolutionUpdate)
	{
		disp->DisplayControlCaps = xf_DisplayControlCaps;
#ifdef USABLE_XRANDR

		if (settings->Fullscreen)
		{
			/* ask X11 to notify us of screen changes */
			XRRSelectInput(xfDisp->xfc->display, DefaultRootWindow(xfDisp->xfc->display),
			               RRScreenChangeNotifyMask);
		}

#endif
	}

	return TRUE;
}

BOOL xf_disp_uninit(xfDispContext* xfDisp, DispClientContext* disp)
{
	if (!xfDisp || !disp)
		return FALSE;

	xfDisp->disp = NULL;
	return TRUE;
}
