/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Display Control Channel
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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
#include <winpr/cast.h>

#include <freerdp/timer.h>

#include "wlf_disp.h"

#define TAG CLIENT_TAG("wayland.disp")

#define RESIZE_MIN_DELAY_NS 200000UL /* minimum delay in ns between two resizes */

struct s_wlfDispContext
{
	wlfContext* wlc;
	DispClientContext* disp;
	BOOL haveXRandr;
	int eventBase, errorBase;
	int lastSentWidth, lastSentHeight;
	UINT64 lastSentDate;
	int targetWidth, targetHeight;
	BOOL activated;
	BOOL waitingResize;
	BOOL fullscreen;
	UINT16 lastSentDesktopOrientation;
	UINT32 lastSentDesktopScaleFactor;
	UINT32 lastSentDeviceScaleFactor;
	FreeRDP_TimerID timerID;
};

static BOOL wlf_disp_sendResize(wlfDispContext* wlfDisp, BOOL fromTimer);
static BOOL wlf_disp_check_context(void* context, wlfContext** ppwlc, wlfDispContext** ppwlfDisp,
                                   rdpSettings** ppSettings);
static UINT wlf_disp_sendLayout(DispClientContext* disp, const rdpMonitor* monitors,
                                size_t nmonitors);

static BOOL wlf_disp_settings_changed(wlfDispContext* wlfDisp)
{
	rdpSettings* settings = NULL;

	WINPR_ASSERT(wlfDisp);
	WINPR_ASSERT(wlfDisp->wlc);

	settings = wlfDisp->wlc->common.context.settings;
	WINPR_ASSERT(settings);

	if (wlfDisp->lastSentWidth != wlfDisp->targetWidth)
		return TRUE;

	if (wlfDisp->lastSentHeight != wlfDisp->targetHeight)
		return TRUE;

	if (wlfDisp->lastSentDesktopOrientation !=
	    freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation))
		return TRUE;

	if (wlfDisp->lastSentDesktopScaleFactor !=
	    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor))
		return TRUE;

	if (wlfDisp->lastSentDeviceScaleFactor !=
	    freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor))
		return TRUE;

	if (wlfDisp->fullscreen != wlfDisp->wlc->fullscreen)
		return TRUE;

	return FALSE;
}

static BOOL wlf_update_last_sent(wlfDispContext* wlfDisp)
{
	rdpSettings* settings = NULL;

	WINPR_ASSERT(wlfDisp);
	WINPR_ASSERT(wlfDisp->wlc);

	settings = wlfDisp->wlc->common.context.settings;
	WINPR_ASSERT(settings);

	wlfDisp->lastSentDate = winpr_GetTickCount64NS();
	wlfDisp->lastSentWidth = wlfDisp->targetWidth;
	wlfDisp->lastSentHeight = wlfDisp->targetHeight;
	wlfDisp->lastSentDesktopOrientation =
	    freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
	wlfDisp->lastSentDesktopScaleFactor =
	    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
	wlfDisp->lastSentDeviceScaleFactor =
	    freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
	wlfDisp->fullscreen = wlfDisp->wlc->fullscreen;
	return TRUE;
}

static uint64_t wlf_disp_OnTimer(rdpContext* context, WINPR_ATTR_UNUSED void* userdata,
                                 WINPR_ATTR_UNUSED FreeRDP_TimerID timerID,
                                 WINPR_ATTR_UNUSED uint64_t timestamp, uint64_t interval)
{
	wlfContext* wlc = NULL;
	wlfDispContext* wlfDisp = NULL;
	rdpSettings* settings = NULL;

	if (!wlf_disp_check_context(context, &wlc, &wlfDisp, &settings))
		return interval;

	if (!wlfDisp->activated || freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
		return interval;

	wlf_disp_sendResize(wlfDisp, TRUE);
	wlfDisp->timerID = 0;
	return 0;
}

static BOOL update_timer(wlfDispContext* wlfDisp, uint64_t intervalNS)
{
	WINPR_ASSERT(wlfDisp);

	if (wlfDisp->timerID == 0)
	{
		rdpContext* context = &wlfDisp->wlc->common.context;

		wlfDisp->timerID = freerdp_timer_add(context, intervalNS, wlf_disp_OnTimer, NULL, true);
	}
	return TRUE;
}

BOOL wlf_disp_sendResize(wlfDispContext* wlfDisp, BOOL fromTimer)
{
	DISPLAY_CONTROL_MONITOR_LAYOUT layout;
	wlfContext* wlc = NULL;
	rdpSettings* settings = NULL;

	if (!wlfDisp || !wlfDisp->wlc)
		return FALSE;

	wlc = wlfDisp->wlc;
	settings = wlc->common.context.settings;

	if (!settings)
		return FALSE;

	if (!wlfDisp->activated || !wlfDisp->disp)
		return update_timer(wlfDisp, RESIZE_MIN_DELAY_NS);

	const uint64_t now = winpr_GetTickCount64NS();
	const uint64_t diff = now - wlfDisp->lastSentDate;
	if (diff < RESIZE_MIN_DELAY_NS)
		return update_timer(wlfDisp, RESIZE_MIN_DELAY_NS);

	if (!fromTimer && (wlfDisp->timerID != 0))
		return TRUE;

	if (!wlf_disp_settings_changed(wlfDisp))
		return TRUE;

	if (wlc->fullscreen && (freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount) > 0))
	{
		const rdpMonitor* monitors =
		    freerdp_settings_get_pointer(settings, FreeRDP_MonitorDefArray);
		const size_t nmonitors = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
		if (wlf_disp_sendLayout(wlfDisp->disp, monitors, nmonitors) != CHANNEL_RC_OK)
			return FALSE;
	}
	else
	{
		wlfDisp->waitingResize = TRUE;
		layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
		layout.Top = layout.Left = 0;
		layout.Width = WINPR_ASSERTING_INT_CAST(uint32_t, wlfDisp->targetWidth);
		layout.Height = WINPR_ASSERTING_INT_CAST(uint32_t, wlfDisp->targetHeight);
		layout.Orientation = freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
		layout.DesktopScaleFactor =
		    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
		layout.DeviceScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
		layout.PhysicalWidth = WINPR_ASSERTING_INT_CAST(uint32_t, wlfDisp->targetWidth);
		layout.PhysicalHeight = WINPR_ASSERTING_INT_CAST(uint32_t, wlfDisp->targetHeight);

		if (IFCALLRESULT(CHANNEL_RC_OK, wlfDisp->disp->SendMonitorLayout, wlfDisp->disp, 1,
		                 &layout) != CHANNEL_RC_OK)
			return FALSE;
	}
	return wlf_update_last_sent(wlfDisp);
}

static BOOL wlf_disp_set_window_resizable(WINPR_ATTR_UNUSED wlfDispContext* wlfDisp)
{
	WLog_ERR("TODO", "TODO: implement");
	return TRUE;
}

BOOL wlf_disp_check_context(void* context, wlfContext** ppwlc, wlfDispContext** ppwlfDisp,
                            rdpSettings** ppSettings)
{
	wlfContext* wlc = NULL;

	if (!context)
		return FALSE;

	wlc = (wlfContext*)context;

	if (!(wlc->disp))
		return FALSE;

	if (!wlc->common.context.settings)
		return FALSE;

	*ppwlc = wlc;
	*ppwlfDisp = wlc->disp;
	*ppSettings = wlc->common.context.settings;
	return TRUE;
}

static void wlf_disp_OnActivated(void* context, const ActivatedEventArgs* e)
{
	wlfContext* wlc = NULL;
	wlfDispContext* wlfDisp = NULL;
	rdpSettings* settings = NULL;

	if (!wlf_disp_check_context(context, &wlc, &wlfDisp, &settings))
		return;

	wlfDisp->waitingResize = FALSE;

	if (wlfDisp->activated && !freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
	{
		wlf_disp_set_window_resizable(wlfDisp);

		if (e->firstActivation)
			return;

		wlf_disp_sendResize(wlfDisp, FALSE);
	}
}

static void wlf_disp_OnGraphicsReset(void* context, const GraphicsResetEventArgs* e)
{
	wlfContext* wlc = NULL;
	wlfDispContext* wlfDisp = NULL;
	rdpSettings* settings = NULL;

	WINPR_UNUSED(e);
	if (!wlf_disp_check_context(context, &wlc, &wlfDisp, &settings))
		return;

	wlfDisp->waitingResize = FALSE;

	if (wlfDisp->activated && !freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
	{
		wlf_disp_set_window_resizable(wlfDisp);
		wlf_disp_sendResize(wlfDisp, FALSE);
	}
}

wlfDispContext* wlf_disp_new(wlfContext* wlc)
{
	wlfDispContext* ret = NULL;
	wPubSub* pubSub = NULL;
	rdpSettings* settings = NULL;

	if (!wlc || !wlc->common.context.settings || !wlc->common.context.pubSub)
		return NULL;

	settings = wlc->common.context.settings;
	pubSub = wlc->common.context.pubSub;
	ret = calloc(1, sizeof(wlfDispContext));

	if (!ret)
		return NULL;

	ret->wlc = wlc;
	ret->lastSentWidth = ret->targetWidth =
	    WINPR_ASSERTING_INT_CAST(int, freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth));
	ret->lastSentHeight = ret->targetHeight =
	    WINPR_ASSERTING_INT_CAST(int, freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight));
	PubSub_SubscribeActivated(pubSub, wlf_disp_OnActivated);
	PubSub_SubscribeGraphicsReset(pubSub, wlf_disp_OnGraphicsReset);
	return ret;
}

void wlf_disp_free(wlfDispContext* disp)
{
	if (!disp)
		return;

	if (disp->wlc)
	{
		wPubSub* pubSub = disp->wlc->common.context.pubSub;
		PubSub_UnsubscribeActivated(pubSub, wlf_disp_OnActivated);
		PubSub_UnsubscribeGraphicsReset(pubSub, wlf_disp_OnGraphicsReset);
	}

	free(disp);
}

UINT wlf_disp_sendLayout(DispClientContext* disp, const rdpMonitor* monitors, size_t nmonitors)
{
	UINT ret = CHANNEL_RC_OK;
	DISPLAY_CONTROL_MONITOR_LAYOUT* layouts = NULL;
	wlfDispContext* wlfDisp = NULL;
	rdpSettings* settings = NULL;

	WINPR_ASSERT(disp);
	WINPR_ASSERT(monitors);
	WINPR_ASSERT(nmonitors > 0);
	WINPR_ASSERT(nmonitors <= UINT32_MAX);

	wlfDisp = (wlfDispContext*)disp->custom;
	WINPR_ASSERT(wlfDisp);
	WINPR_ASSERT(wlfDisp->wlc);

	settings = wlfDisp->wlc->common.context.settings;
	WINPR_ASSERT(settings);

	layouts = calloc(nmonitors, sizeof(DISPLAY_CONTROL_MONITOR_LAYOUT));

	if (!layouts)
		return CHANNEL_RC_NO_MEMORY;

	for (size_t i = 0; i < nmonitors; i++)
	{
		const rdpMonitor* monitor = &monitors[i];
		DISPLAY_CONTROL_MONITOR_LAYOUT* layout = &layouts[i];

		layout->Flags = (monitor->is_primary ? DISPLAY_CONTROL_MONITOR_PRIMARY : 0);
		layout->Left = monitor->x;
		layout->Top = monitor->y;
		layout->Width = WINPR_ASSERTING_INT_CAST(UINT32, monitor->width);
		layout->Height = WINPR_ASSERTING_INT_CAST(UINT32, monitor->height);
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

		layout->DesktopScaleFactor =
		    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
		layout->DeviceScaleFactor =
		    freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
	}

	ret = IFCALLRESULT(CHANNEL_RC_OK, disp->SendMonitorLayout, disp, (UINT32)nmonitors, layouts);
	free(layouts);
	return ret;
}

BOOL wlf_disp_handle_configure(wlfDispContext* disp, int32_t width, int32_t height)
{
	if (!disp)
		return FALSE;

	disp->targetWidth = width;
	disp->targetHeight = height;
	return wlf_disp_sendResize(disp, FALSE);
}

static UINT wlf_DisplayControlCaps(DispClientContext* disp, UINT32 maxNumMonitors,
                                   UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB)
{
	/* we're called only if dynamic resolution update is activated */
	wlfDispContext* wlfDisp = NULL;
	rdpSettings* settings = NULL;

	WINPR_ASSERT(disp);

	wlfDisp = (wlfDispContext*)disp->custom;
	WINPR_ASSERT(wlfDisp);
	WINPR_ASSERT(wlfDisp->wlc);

	settings = wlfDisp->wlc->common.context.settings;
	WINPR_ASSERT(settings);

	WLog_DBG(TAG,
	         "DisplayControlCapsPdu: MaxNumMonitors: %" PRIu32 " MaxMonitorAreaFactorA: %" PRIu32
	         " MaxMonitorAreaFactorB: %" PRIu32 "",
	         maxNumMonitors, maxMonitorAreaFactorA, maxMonitorAreaFactorB);
	wlfDisp->activated = TRUE;

	if (freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
		return CHANNEL_RC_OK;

	WLog_DBG(TAG, "DisplayControlCapsPdu: setting the window as resizable");
	return wlf_disp_set_window_resizable(wlfDisp) ? CHANNEL_RC_OK : CHANNEL_RC_NO_MEMORY;
}

BOOL wlf_disp_init(wlfDispContext* wlfDisp, DispClientContext* disp)
{
	rdpSettings* settings = NULL;

	if (!wlfDisp || !wlfDisp->wlc || !disp)
		return FALSE;

	settings = wlfDisp->wlc->common.context.settings;

	if (!settings)
		return FALSE;

	wlfDisp->disp = disp;
	disp->custom = (void*)wlfDisp;

	if (freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
	{
		disp->DisplayControlCaps = wlf_DisplayControlCaps;
	}

	return TRUE;
}

BOOL wlf_disp_uninit(wlfDispContext* wlfDisp, DispClientContext* disp)
{
	if (!wlfDisp || !disp)
		return FALSE;

	wlfDisp->disp = NULL;
	return TRUE;
}

int wlf_list_monitors(wlfContext* wlc)
{
	uint32_t nmonitors = UwacDisplayGetNbOutputs(wlc->display);

	for (uint32_t i = 0; i < nmonitors; i++)
	{
		const UwacOutput* monitor =
		    UwacDisplayGetOutput(wlc->display, WINPR_ASSERTING_INT_CAST(int, i));
		UwacSize resolution;
		UwacPosition pos;

		if (!monitor)
			continue;
		UwacOutputGetPosition(monitor, &pos);
		UwacOutputGetResolution(monitor, &resolution);

		printf("     %s [%u] %dx%d\t+%d+%d\n", (i == 0) ? "*" : " ", i, resolution.width,
		       resolution.height, pos.x, pos.y);
	}

	return 0;
}
