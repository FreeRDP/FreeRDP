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

#include <winpr/assert.h>
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

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11disp")
#define RESIZE_MIN_DELAY 200 /* minimum delay in ms between two resizes */

struct s_xfDispContext
{
	xfContext* xfc;
	DispClientContext* disp;
	BOOL haveXRandr;
	int eventBase;
	int errorBase;
	UINT32 lastSentWidth;
	UINT32 lastSentHeight;
	BYTE reserved[4];
	UINT64 lastSentDate;
	UINT32 targetWidth;
	UINT32 targetHeight;
	BOOL activated;
	BOOL fullscreen;
	UINT16 lastSentDesktopOrientation;
	BYTE reserved2[2];
	UINT32 lastSentDesktopScaleFactor;
	UINT32 lastSentDeviceScaleFactor;
	BYTE reserved3[4];
};

static UINT xf_disp_sendLayout(DispClientContext* disp, const rdpMonitor* monitors,
                               UINT32 nmonitors);

static BOOL xf_disp_settings_changed(xfDispContext* xfDisp)
{
	rdpSettings* settings;

	WINPR_ASSERT(xfDisp);
	WINPR_ASSERT(xfDisp->xfc);

	settings = xfDisp->xfc->common.context.settings;
	WINPR_ASSERT(settings);

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
	rdpSettings* settings;

	WINPR_ASSERT(xfDisp);
	WINPR_ASSERT(xfDisp->xfc);

	settings = xfDisp->xfc->common.context.settings;
	WINPR_ASSERT(settings);

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
	DISPLAY_CONTROL_MONITOR_LAYOUT layout = { 0 };
	xfContext* xfc;
	rdpSettings* settings;

	if (!xfDisp || !xfDisp->xfc)
		return FALSE;

	xfc = xfDisp->xfc;
	settings = xfc->common.context.settings;

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
		layout.PhysicalWidth = xfDisp->targetWidth / 75.0 * 25.4;
		layout.PhysicalHeight = xfDisp->targetHeight / 75.0 * 25.4;

		if (IFCALLRESULT(CHANNEL_RC_OK, xfDisp->disp->SendMonitorLayout, xfDisp->disp, 1,
		                 &layout) != CHANNEL_RC_OK)
			return FALSE;
	}

	return xf_update_last_sent(xfDisp);
}

static BOOL xf_disp_queueResize(xfDispContext* xfDisp, UINT32 width, UINT32 height)
{
	if ((xfDisp->targetWidth == (INT64)width) && (xfDisp->targetHeight == (INT64)height))
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

	if (!xfc->common.context.settings)
		return FALSE;

	*ppXfc = xfc;
	*ppXfDisp = xfc->xfDisp;
	*ppSettings = xfc->common.context.settings;
	return TRUE;
}

static void xf_disp_OnActivated(void* context, const ActivatedEventArgs* e)
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

static void xf_disp_OnGraphicsReset(void* context, const GraphicsResetEventArgs* e)
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

static void xf_disp_OnTimer(void* context, const TimerEventArgs* e)
{
	xfContext* xfc;
	xfDispContext* xfDisp;
	rdpSettings* settings;

	WINPR_UNUSED(e);

	if (!xf_disp_check_context(context, &xfc, &xfDisp, &settings))
		return;

	if (!xfDisp->activated || xfc->fullscreen)
		return;

	xf_disp_sendResize(xfDisp);
}

static void xf_disp_OnWindowStateChange(void* context, const WindowStateChangeEventArgs* e)
{
	xfContext* xfc;
	xfDispContext* xfDisp;
	rdpSettings* settings;

	WINPR_UNUSED(e);

	if (!xf_disp_check_context(context, &xfc, &xfDisp, &settings))
		return;

	if (!xfDisp->activated || !xfc->fullscreen)
		return;

	xf_disp_sendResize(xfDisp);
}

xfDispContext* xf_disp_new(xfContext* xfc)
{
	xfDispContext* ret;
	const rdpSettings* settings;
	wPubSub* pubSub;

	WINPR_ASSERT(xfc);

	pubSub = xfc->common.context.pubSub;
	WINPR_ASSERT(pubSub);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

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
	ret->lastSentWidth = ret->targetWidth = settings->DesktopWidth;
	ret->lastSentHeight = ret->targetHeight = settings->DesktopHeight;
	PubSub_SubscribeActivated(pubSub, xf_disp_OnActivated);
	PubSub_SubscribeGraphicsReset(pubSub, xf_disp_OnGraphicsReset);
	PubSub_SubscribeTimer(pubSub, xf_disp_OnTimer);
	PubSub_SubscribeWindowStateChange(pubSub, xf_disp_OnWindowStateChange);
	return ret;
}

void xf_disp_free(xfDispContext* disp)
{
	if (!disp)
		return;

	if (disp->xfc)
	{
		wPubSub* pubSub = disp->xfc->common.context.pubSub;
		PubSub_UnsubscribeActivated(pubSub, xf_disp_OnActivated);
		PubSub_UnsubscribeGraphicsReset(pubSub, xf_disp_OnGraphicsReset);
		PubSub_UnsubscribeTimer(pubSub, xf_disp_OnTimer);
		PubSub_UnsubscribeWindowStateChange(pubSub, xf_disp_OnWindowStateChange);
	}

	free(disp);
}

UINT xf_disp_sendLayout(DispClientContext* disp, const rdpMonitor* monitors, UINT32 nmonitors)
{
	UINT ret = CHANNEL_RC_OK;
	UINT32 i;
	xfDispContext* xfDisp;
	rdpSettings* settings;
	DISPLAY_CONTROL_MONITOR_LAYOUT* layouts;

	WINPR_ASSERT(disp);
	WINPR_ASSERT(monitors);
	WINPR_ASSERT(nmonitors > 0);

	xfDisp = (xfDispContext*)disp->custom;
	WINPR_ASSERT(xfDisp);
	WINPR_ASSERT(xfDisp->xfc);

	settings = xfDisp->xfc->common.context.settings;
	WINPR_ASSERT(settings);

	layouts = calloc(nmonitors, sizeof(DISPLAY_CONTROL_MONITOR_LAYOUT));

	if (!layouts)
		return CHANNEL_RC_NO_MEMORY;

	for (i = 0; i < nmonitors; i++)
	{
		const rdpMonitor* monitor = &monitors[i];
		DISPLAY_CONTROL_MONITOR_LAYOUT* layout = &layouts[i];

		layout->Flags = (monitor->is_primary ? DISPLAY_CONTROL_MONITOR_PRIMARY : 0);
		layout->Left = monitor->x;
		layout->Top = monitor->y;
		layout->Width = monitor->width;
		layout->Height = monitor->height;
		layout->Orientation = ORIENTATION_LANDSCAPE;
		layout->PhysicalWidth = monitor->attributes.physicalWidth;
		layout->PhysicalHeight = monitor->attributes.physicalHeight;

		switch (monitor->attributes.orientation)
		{
			case 90:
				layout->Orientation = ORIENTATION_PORTRAIT;
				break;

			case 180:
				layout->Orientation = ORIENTATION_LANDSCAPE_FLIPPED;
				break;

			case 270:
				layout->Orientation = ORIENTATION_PORTRAIT_FLIPPED;
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
				layout->Orientation = ORIENTATION_LANDSCAPE;
				break;
		}

		layout->DesktopScaleFactor = settings->DesktopScaleFactor;
		layout->DeviceScaleFactor = settings->DeviceScaleFactor;
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

	settings = xfc->common.context.settings;

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
	xfDispContext* xfDisp;
	rdpSettings* settings;

	WINPR_ASSERT(disp);

	xfDisp = (xfDispContext*)disp->custom;
	WINPR_ASSERT(xfDisp);
	WINPR_ASSERT(xfDisp->xfc);

	settings = xfDisp->xfc->common.context.settings;
	WINPR_ASSERT(settings);

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

	settings = xfDisp->xfc->common.context.settings;

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
