/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Display Control Channel
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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
#include <winpr/assert.h>

#include <freerdp/gdi/gdi.h>

#include <SDL.h>

#include "sdl_disp.h"
#include "sdl_kbd.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("sdl.disp")

#define RESIZE_MIN_DELAY 200 /* minimum delay in ms between two resizes */

struct s_sdlDispContext
{
	sdlContext* sdl;
	DispClientContext* disp;
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
};

static UINT sdl_disp_sendLayout(DispClientContext* disp, const rdpMonitor* monitors,
                                size_t nmonitors);

static BOOL sdl_disp_settings_changed(sdlDispContext* sdlDisp)
{
	rdpSettings* settings;

	WINPR_ASSERT(sdlDisp);
	WINPR_ASSERT(sdlDisp->sdl);

	settings = sdlDisp->sdl->common.context.settings;
	WINPR_ASSERT(settings);

	if (sdlDisp->lastSentWidth != sdlDisp->targetWidth)
		return TRUE;

	if (sdlDisp->lastSentHeight != sdlDisp->targetHeight)
		return TRUE;

	if (sdlDisp->lastSentDesktopOrientation != settings->DesktopOrientation)
		return TRUE;

	if (sdlDisp->lastSentDesktopScaleFactor != settings->DesktopScaleFactor)
		return TRUE;

	if (sdlDisp->lastSentDeviceScaleFactor != settings->DeviceScaleFactor)
		return TRUE;
	/* TODFO
	    if (sdlDisp->fullscreen != sdlDisp->sdl->fullscreen)
	        return TRUE;
	*/
	return FALSE;
}

static BOOL sdl_update_last_sent(sdlDispContext* sdlDisp)
{
	rdpSettings* settings;

	WINPR_ASSERT(sdlDisp);
	WINPR_ASSERT(sdlDisp->sdl);

	settings = sdlDisp->sdl->common.context.settings;
	WINPR_ASSERT(settings);

	sdlDisp->lastSentWidth = sdlDisp->targetWidth;
	sdlDisp->lastSentHeight = sdlDisp->targetHeight;
	sdlDisp->lastSentDesktopOrientation = settings->DesktopOrientation;
	sdlDisp->lastSentDesktopScaleFactor = settings->DesktopScaleFactor;
	sdlDisp->lastSentDeviceScaleFactor = settings->DeviceScaleFactor;
	// TODO sdlDisp->fullscreen = sdlDisp->sdl->fullscreen;
	return TRUE;
}

static BOOL sdl_disp_sendResize(sdlDispContext* sdlDisp)
{
	DISPLAY_CONTROL_MONITOR_LAYOUT layout;
	sdlContext* sdl;
	rdpSettings* settings;

	if (!sdlDisp || !sdlDisp->sdl)
		return FALSE;

	sdl = sdlDisp->sdl;
	settings = sdl->common.context.settings;

	if (!settings)
		return FALSE;

	if (!sdlDisp->activated || !sdlDisp->disp)
		return TRUE;

	if (GetTickCount64() - sdlDisp->lastSentDate < RESIZE_MIN_DELAY)
		return TRUE;

	sdlDisp->lastSentDate = GetTickCount64();

	if (!sdl_disp_settings_changed(sdlDisp))
		return TRUE;

	/* TODO: Multimonitor support for wayland
	if (sdl->fullscreen && (settings->MonitorCount > 0))
	{
	    if (sdl_disp_sendLayout(sdlDisp->disp, settings->MonitorDefArray,
	                           settings->MonitorCount) != CHANNEL_RC_OK)
	        return FALSE;
	}
	else
	*/
	{
		sdlDisp->waitingResize = TRUE;
		layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
		layout.Top = layout.Left = 0;
		layout.Width = sdlDisp->targetWidth;
		layout.Height = sdlDisp->targetHeight;
		layout.Orientation = settings->DesktopOrientation;
		layout.DesktopScaleFactor = settings->DesktopScaleFactor;
		layout.DeviceScaleFactor = settings->DeviceScaleFactor;
		layout.PhysicalWidth = sdlDisp->targetWidth;
		layout.PhysicalHeight = sdlDisp->targetHeight;

		if (IFCALLRESULT(CHANNEL_RC_OK, sdlDisp->disp->SendMonitorLayout, sdlDisp->disp, 1,
		                 &layout) != CHANNEL_RC_OK)
			return FALSE;
	}
	return sdl_update_last_sent(sdlDisp);
}

static BOOL sdl_disp_set_window_resizable(sdlDispContext* sdlDisp)
{
	WINPR_ASSERT(sdlDisp);
	update_resizeable(sdlDisp->sdl, TRUE);
	return TRUE;
}

static BOOL sdl_disp_check_context(void* context, sdlContext** ppsdl, sdlDispContext** ppsdlDisp,
                                   rdpSettings** ppSettings)
{
	sdlContext* sdl;

	if (!context)
		return FALSE;

	sdl = (sdlContext*)context;

	if (!(sdl->disp))
		return FALSE;

	if (!sdl->common.context.settings)
		return FALSE;

	*ppsdl = sdl;
	*ppsdlDisp = sdl->disp;
	*ppSettings = sdl->common.context.settings;
	return TRUE;
}

static void sdl_disp_OnActivated(void* context, const ActivatedEventArgs* e)
{
	sdlContext* sdl;
	sdlDispContext* sdlDisp;
	rdpSettings* settings;

	if (!sdl_disp_check_context(context, &sdl, &sdlDisp, &settings))
		return;

	sdlDisp->waitingResize = FALSE;

	if (sdlDisp->activated && !settings->Fullscreen)
	{
		sdl_disp_set_window_resizable(sdlDisp);

		if (e->firstActivation)
			return;

		sdl_disp_sendResize(sdlDisp);
	}
}

static void sdl_disp_OnGraphicsReset(void* context, const GraphicsResetEventArgs* e)
{
	sdlContext* sdl;
	sdlDispContext* sdlDisp;
	rdpSettings* settings;

	WINPR_UNUSED(e);
	if (!sdl_disp_check_context(context, &sdl, &sdlDisp, &settings))
		return;

	sdlDisp->waitingResize = FALSE;

	if (sdlDisp->activated && !settings->Fullscreen)
	{
		sdl_disp_set_window_resizable(sdlDisp);
		sdl_disp_sendResize(sdlDisp);
	}
}

static void sdl_disp_OnTimer(void* context, const TimerEventArgs* e)
{
	sdlContext* sdl;
	sdlDispContext* sdlDisp;
	rdpSettings* settings;

	WINPR_UNUSED(e);
	if (!sdl_disp_check_context(context, &sdl, &sdlDisp, &settings))
		return;

	if (!sdlDisp->activated || settings->Fullscreen)
		return;

	sdl_disp_sendResize(sdlDisp);
}

sdlDispContext* sdl_disp_new(sdlContext* sdl)
{
	sdlDispContext* ret;
	wPubSub* pubSub;
	rdpSettings* settings;

	if (!sdl || !sdl->common.context.settings || !sdl->common.context.pubSub)
		return NULL;

	settings = sdl->common.context.settings;
	pubSub = sdl->common.context.pubSub;
	ret = calloc(1, sizeof(sdlDispContext));

	if (!ret)
		return NULL;

	ret->sdl = sdl;
	ret->lastSentWidth = ret->targetWidth = settings->DesktopWidth;
	ret->lastSentHeight = ret->targetHeight = settings->DesktopHeight;
	PubSub_SubscribeActivated(pubSub, sdl_disp_OnActivated);
	PubSub_SubscribeGraphicsReset(pubSub, sdl_disp_OnGraphicsReset);
	PubSub_SubscribeTimer(pubSub, sdl_disp_OnTimer);
	return ret;
}

void sdl_disp_free(sdlDispContext* disp)
{
	if (!disp)
		return;

	if (disp->sdl)
	{
		wPubSub* pubSub = disp->sdl->common.context.pubSub;
		PubSub_UnsubscribeActivated(pubSub, sdl_disp_OnActivated);
		PubSub_UnsubscribeGraphicsReset(pubSub, sdl_disp_OnGraphicsReset);
		PubSub_UnsubscribeTimer(pubSub, sdl_disp_OnTimer);
	}

	free(disp);
}

static UINT sdl_disp_sendLayout(DispClientContext* disp, const rdpMonitor* monitors,
                                size_t nmonitors)
{
	UINT ret = CHANNEL_RC_OK;
	DISPLAY_CONTROL_MONITOR_LAYOUT* layouts;
	size_t i;
	sdlDispContext* sdlDisp;
	rdpSettings* settings;

	WINPR_ASSERT(disp);
	WINPR_ASSERT(monitors);
	WINPR_ASSERT(nmonitors > 0);

	sdlDisp = (sdlDispContext*)disp->custom;
	WINPR_ASSERT(sdlDisp);
	WINPR_ASSERT(sdlDisp->sdl);

	settings = sdlDisp->sdl->common.context.settings;
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

#if SDL_VERSION_ATLEAST(2, 0, 10)
BOOL sdl_disp_handle_display_event(sdlDispContext* disp, const SDL_DisplayEvent* ev)
{
	WINPR_ASSERT(ev);

	if (!disp)
		return FALSE;
	sdlContext* sdl = disp->sdl;
	WINPR_ASSERT(sdl);

	switch (ev->event)
	{
#if SDL_VERSION_ATLEAST(2, 0, 14)
		case SDL_DISPLAYEVENT_CONNECTED:
			SDL_Log("A new display with id %d was connected", ev->display);
			return TRUE;
		case SDL_DISPLAYEVENT_DISCONNECTED:
			SDL_Log("The display with id %d was disconnected", ev->display);
			return TRUE;
#endif
		case SDL_DISPLAYEVENT_ORIENTATION:
			SDL_Log("The orientation of display with id %d was changed", ev->display);
			return TRUE;
		default:
			return TRUE;
	}
}
#endif

#if !SDL_VERSION_ATLEAST(2, 0, 16)
static BOOL sdl_grab(sdlContext* sdl, Uint32 windowID, SDL_bool enable)
{
	SDL_Window* window = SDL_GetWindowFromID(windowID);
	if (!window)
		return FALSE;

	sdl->grab_mouse = enable;
	SDL_SetWindowGrab(window, enable);
	return TRUE;
}
#endif

BOOL sdl_grab_keyboard(sdlContext* sdl, Uint32 windowID, SDL_bool enable)
{
	SDL_Window* window = SDL_GetWindowFromID(windowID);
	if (!window)
		return FALSE;
#if SDL_VERSION_ATLEAST(2, 0, 16)
	sdl->grab_kbd = enable;
	SDL_SetWindowKeyboardGrab(window, enable);
	return TRUE;
#else
	WLog_WARN(TAG, "Keyboard grabbing not supported by SDL2 < 2.0.16");
	return FALSE;
#endif
}

BOOL sdl_grab_mouse(sdlContext* sdl, Uint32 windowID, SDL_bool enable)
{
	SDL_Window* window = SDL_GetWindowFromID(windowID);
	if (!window)
		return FALSE;
#if SDL_VERSION_ATLEAST(2, 0, 16)
	sdl->grab_mouse = enable;
	SDL_SetWindowMouseGrab(window, enable);
	return TRUE;
#else
	return sdl_grab(sdl, windowID, enable);
#endif
}

BOOL sdl_disp_handle_window_event(sdlDispContext* disp, const SDL_WindowEvent* ev)
{
	WINPR_ASSERT(ev);

	if (!disp)
		return FALSE;
	sdlContext* sdl = disp->sdl;
	WINPR_ASSERT(sdl);

	switch (ev->event)
	{
		case SDL_WINDOWEVENT_HIDDEN:
		case SDL_WINDOWEVENT_MINIMIZED:
			gdi_send_suppress_output(sdl->common.context.gdi, TRUE);
			return TRUE;

		case SDL_WINDOWEVENT_EXPOSED:
		case SDL_WINDOWEVENT_SHOWN:
		case SDL_WINDOWEVENT_MAXIMIZED:
		case SDL_WINDOWEVENT_RESTORED:
			gdi_send_suppress_output(sdl->common.context.gdi, FALSE);
			return TRUE;

		case SDL_WINDOWEVENT_RESIZED:
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			disp->targetWidth = ev->data1;
			disp->targetHeight = ev->data2;
			return sdl_disp_sendResize(disp);

		case SDL_WINDOWEVENT_LEAVE:
			sdl_grab_keyboard(sdl, ev->windowID, SDL_FALSE);
			return TRUE;
		case SDL_WINDOWEVENT_ENTER:
			sdl_grab_keyboard(sdl, ev->windowID, SDL_TRUE);
			return sdl_keyboard_focus_in(&sdl->common.context);
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_TAKE_FOCUS:
			return sdl_keyboard_focus_in(&sdl->common.context);

		default:
			return TRUE;
	}
}

static UINT sdl_DisplayControlCaps(DispClientContext* disp, UINT32 maxNumMonitors,
                                   UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB)
{
	/* we're called only if dynamic resolution update is activated */
	sdlDispContext* sdlDisp;
	rdpSettings* settings;

	WINPR_ASSERT(disp);

	sdlDisp = (sdlDispContext*)disp->custom;
	WINPR_ASSERT(sdlDisp);
	WINPR_ASSERT(sdlDisp->sdl);

	settings = sdlDisp->sdl->common.context.settings;
	WINPR_ASSERT(settings);

	WLog_DBG(TAG,
	         "DisplayControlCapsPdu: MaxNumMonitors: %" PRIu32 " MaxMonitorAreaFactorA: %" PRIu32
	         " MaxMonitorAreaFactorB: %" PRIu32 "",
	         maxNumMonitors, maxMonitorAreaFactorA, maxMonitorAreaFactorB);
	sdlDisp->activated = TRUE;

	if (settings->Fullscreen)
		return CHANNEL_RC_OK;

	WLog_DBG(TAG, "DisplayControlCapsPdu: setting the window as resizable");
	return sdl_disp_set_window_resizable(sdlDisp) ? CHANNEL_RC_OK : CHANNEL_RC_NO_MEMORY;
}

BOOL sdl_disp_init(sdlDispContext* sdlDisp, DispClientContext* disp)
{
	rdpSettings* settings;

	if (!sdlDisp || !sdlDisp->sdl || !disp)
		return FALSE;

	settings = sdlDisp->sdl->common.context.settings;

	if (!settings)
		return FALSE;

	sdlDisp->disp = disp;
	disp->custom = (void*)sdlDisp;

	if (settings->DynamicResolutionUpdate)
	{
		disp->DisplayControlCaps = sdl_DisplayControlCaps;
	}

	update_resizeable(sdlDisp->sdl, TRUE);
	return TRUE;
}

BOOL sdl_disp_uninit(sdlDispContext* sdlDisp, DispClientContext* disp)
{
	if (!sdlDisp || !disp)
		return FALSE;

	sdlDisp->disp = NULL;
	update_resizeable(sdlDisp->sdl, FALSE);
	return TRUE;
}
