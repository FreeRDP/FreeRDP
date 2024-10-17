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

#include <vector>
#include <winpr/sysinfo.h>
#include <winpr/assert.h>

#include <freerdp/gdi/gdi.h>

#include <SDL3/SDL.h>

#include "sdl_disp.hpp"
#include "sdl_kbd.hpp"
#include "sdl_utils.hpp"
#include "sdl_freerdp.hpp"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("sdl.disp")

static constexpr UINT64 RESIZE_MIN_DELAY = 200; /* minimum delay in ms between two resizes */
static constexpr unsigned MAX_RETRIES = 5;

BOOL sdlDispContext::settings_changed()
{
	auto settings = _sdl->context()->settings;
	WINPR_ASSERT(settings);

	if (_lastSentWidth != _targetWidth)
		return TRUE;

	if (_lastSentHeight != _targetHeight)
		return TRUE;

	if (_lastSentDesktopOrientation !=
	    freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation))
		return TRUE;

	if (_lastSentDesktopScaleFactor !=
	    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor))
		return TRUE;

	if (_lastSentDeviceScaleFactor !=
	    freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor))
		return TRUE;
	/* TODO
	    if (_fullscreen != _sdl->fullscreen)
	        return TRUE;
	*/
	return FALSE;
}

BOOL sdlDispContext::update_last_sent()
{
	WINPR_ASSERT(_sdl);

	auto settings = _sdl->context()->settings;
	WINPR_ASSERT(settings);

	_lastSentWidth = _targetWidth;
	_lastSentHeight = _targetHeight;
	_lastSentDesktopOrientation = freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
	_lastSentDesktopScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
	_lastSentDeviceScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
	// TODO _fullscreen = _sdl->fullscreen;
	return TRUE;
}

BOOL sdlDispContext::sendResize()
{
	DISPLAY_CONTROL_MONITOR_LAYOUT layout = {};
	auto settings = _sdl->context()->settings;

	if (!settings)
		return FALSE;

	if (!_activated || !_disp)
		return TRUE;

	if (GetTickCount64() - _lastSentDate < RESIZE_MIN_DELAY)
		return TRUE;

	_lastSentDate = GetTickCount64();

	if (!settings_changed())
		return TRUE;

	const UINT32 mcount = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	if (_sdl->fullscreen && (mcount > 0))
	{
		auto monitors = static_cast<const rdpMonitor*>(
		    freerdp_settings_get_pointer(settings, FreeRDP_MonitorDefArray));
		if (sendLayout(monitors, mcount) != CHANNEL_RC_OK)
			return FALSE;
	}
	else
	{
		_waitingResize = TRUE;
		layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
		layout.Top = layout.Left = 0;
		layout.Width = _targetWidth;
		layout.Height = _targetHeight;
		layout.Orientation = freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
		layout.DesktopScaleFactor =
		    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
		layout.DeviceScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
		layout.PhysicalWidth = _targetWidth;
		layout.PhysicalHeight = _targetHeight;

		if (IFCALLRESULT(CHANNEL_RC_OK, _disp->SendMonitorLayout, _disp, 1, &layout) !=
		    CHANNEL_RC_OK)
			return FALSE;
	}
	return update_last_sent();
}

BOOL sdlDispContext::set_window_resizable()
{
	_sdl->update_resizeable(TRUE);
	return TRUE;
}

static BOOL sdl_disp_check_context(void* context, SdlContext** ppsdl, sdlDispContext** ppsdlDisp,
                                   rdpSettings** ppSettings)
{
	if (!context)
		return FALSE;

	auto sdl = get_context(context);
	if (!sdl)
		return FALSE;

	if (!sdl->context()->settings)
		return FALSE;

	*ppsdl = sdl;
	*ppsdlDisp = &sdl->disp;
	*ppSettings = sdl->context()->settings;
	return TRUE;
}

void sdlDispContext::OnActivated(void* context, const ActivatedEventArgs* e)
{
	SdlContext* sdl = nullptr;
	sdlDispContext* sdlDisp = nullptr;
	rdpSettings* settings = nullptr;

	if (!sdl_disp_check_context(context, &sdl, &sdlDisp, &settings))
		return;

	sdlDisp->_waitingResize = FALSE;

	if (sdlDisp->_activated && !freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
	{
		sdlDisp->set_window_resizable();

		if (e->firstActivation)
			return;

		sdlDisp->addTimer();
	}
}

void sdlDispContext::OnGraphicsReset(void* context, const GraphicsResetEventArgs* e)
{
	SdlContext* sdl = nullptr;
	sdlDispContext* sdlDisp = nullptr;
	rdpSettings* settings = nullptr;

	WINPR_UNUSED(e);
	if (!sdl_disp_check_context(context, &sdl, &sdlDisp, &settings))
		return;

	sdlDisp->_waitingResize = FALSE;

	if (sdlDisp->_activated && !freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
	{
		sdlDisp->set_window_resizable();
		sdlDisp->addTimer();
	}
}

Uint32 sdlDispContext::OnTimer(void* param, SDL_TimerID timerID, Uint32 interval)
{
	auto ctx = static_cast<sdlDispContext*>(param);
	if (!ctx)
		return 0;

	SdlContext* sdl = ctx->_sdl;
	if (!sdl)
		return 0;

	sdlDispContext* sdlDisp = nullptr;
	rdpSettings* settings = nullptr;

	if (!sdl_disp_check_context(sdl->context(), &sdl, &sdlDisp, &settings))
		return 0;

	WLog_Print(sdl->log, WLOG_TRACE, "checking for display changes...");
	if (!sdlDisp->_activated || freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
		return 0;
	else
	{
		auto rc = sdlDisp->sendResize();
		if (!rc)
			WLog_Print(sdl->log, WLOG_TRACE, "sent new display layout, result %d", rc);
	}
	if (sdlDisp->_timer_retries++ >= MAX_RETRIES)
	{
		WLog_Print(sdl->log, WLOG_TRACE, "deactivate timer, retries exceeded");
		return 0;
	}

	WLog_Print(sdl->log, WLOG_TRACE, "fire timer one more time");
	return interval;
}

UINT sdlDispContext::sendLayout(const rdpMonitor* monitors, size_t nmonitors)
{
	UINT ret = CHANNEL_RC_OK;

	WINPR_ASSERT(monitors);
	WINPR_ASSERT(nmonitors > 0);

	auto settings = _sdl->context()->settings;
	WINPR_ASSERT(settings);

	std::vector<DISPLAY_CONTROL_MONITOR_LAYOUT> layouts;
	layouts.resize(nmonitors);

	for (size_t i = 0; i < nmonitors; i++)
	{
		auto monitor = &monitors[i];
		auto layout = &layouts[i];

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

		layout->DesktopScaleFactor =
		    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
		layout->DeviceScaleFactor =
		    freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
	}

	WINPR_ASSERT(_disp);
	ret = IFCALLRESULT(CHANNEL_RC_OK, _disp->SendMonitorLayout, _disp, layouts.size(),
	                   layouts.data());
	return ret;
}

BOOL sdlDispContext::addTimer()
{
	if (SDL_WasInit(SDL_INIT_EVENTS) == 0)
		return FALSE;

	SDL_RemoveTimer(_timer);
	WLog_Print(_sdl->log, WLOG_TRACE, "adding new display check timer");

	_timer_retries = 0;
	sendResize();
	_timer = SDL_AddTimer(1000, sdlDispContext::OnTimer, this);
	return TRUE;
}

BOOL sdlDispContext::handle_display_event(const SDL_DisplayEvent* ev)
{
	WINPR_ASSERT(ev);

	switch (ev->type)
	{
		case SDL_EVENT_DISPLAY_ADDED:
			SDL_Log("A new display with id %u was connected", ev->displayID);
			return TRUE;
		case SDL_EVENT_DISPLAY_REMOVED:
			SDL_Log("The display with id %u was disconnected", ev->displayID);
			return TRUE;
		case SDL_EVENT_DISPLAY_ORIENTATION:
			SDL_Log("The orientation of display with id %u was changed", ev->displayID);
			return TRUE;
		default:
			return TRUE;
	}
}

BOOL sdlDispContext::handle_window_event(const SDL_WindowEvent* ev)
{
	WINPR_ASSERT(ev);

	auto bordered = freerdp_settings_get_bool(_sdl->context()->settings, FreeRDP_Decorations);

	auto it = _sdl->windows.find(ev->windowID);
	if (it != _sdl->windows.end())
		it->second.setBordered(bordered);

	switch (ev->type)
	{
		case SDL_EVENT_WINDOW_HIDDEN:
		case SDL_EVENT_WINDOW_MINIMIZED:
			gdi_send_suppress_output(_sdl->context()->gdi, TRUE);

			return TRUE;

		case SDL_EVENT_WINDOW_EXPOSED:
		case SDL_EVENT_WINDOW_SHOWN:
		case SDL_EVENT_WINDOW_MAXIMIZED:
		case SDL_EVENT_WINDOW_RESTORED:
			gdi_send_suppress_output(_sdl->context()->gdi, FALSE);
			return TRUE;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			_targetWidth = ev->data1;
			_targetHeight = ev->data2;
			return addTimer();

		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			WINPR_ASSERT(_sdl);
			_sdl->input.keyboard_grab(ev->windowID, false);
			return TRUE;
		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			WINPR_ASSERT(_sdl);
			_sdl->input.keyboard_grab(ev->windowID, true);
			return _sdl->input.keyboard_focus_in();
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			return _sdl->input.keyboard_focus_in();

		default:
			return TRUE;
	}
}

UINT sdlDispContext::DisplayControlCaps(DispClientContext* disp, UINT32 maxNumMonitors,
                                        UINT32 maxMonitorAreaFactorA, UINT32 maxMonitorAreaFactorB)
{
	/* we're called only if dynamic resolution update is activated */
	WINPR_ASSERT(disp);

	auto sdlDisp = reinterpret_cast<sdlDispContext*>(disp->custom);
	return sdlDisp->DisplayControlCaps(maxNumMonitors, maxMonitorAreaFactorA,
	                                   maxMonitorAreaFactorB);
}

UINT sdlDispContext::DisplayControlCaps(UINT32 maxNumMonitors, UINT32 maxMonitorAreaFactorA,
                                        UINT32 maxMonitorAreaFactorB)
{
	auto settings = _sdl->context()->settings;
	WINPR_ASSERT(settings);

	WLog_DBG(TAG,
	         "DisplayControlCapsPdu: MaxNumMonitors: %" PRIu32 " MaxMonitorAreaFactorA: %" PRIu32
	         " MaxMonitorAreaFactorB: %" PRIu32 "",
	         maxNumMonitors, maxMonitorAreaFactorA, maxMonitorAreaFactorB);
	_activated = TRUE;

	if (freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
		return CHANNEL_RC_OK;

	WLog_DBG(TAG, "DisplayControlCapsPdu: setting the window as resizable");
	return set_window_resizable() ? CHANNEL_RC_OK : CHANNEL_RC_NO_MEMORY;
}

BOOL sdlDispContext::init(DispClientContext* disp)
{
	if (!disp)
		return FALSE;

	auto settings = _sdl->context()->settings;

	if (!settings)
		return FALSE;

	_disp = disp;
	disp->custom = this;

	if (freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
	{
		disp->DisplayControlCaps = sdlDispContext::DisplayControlCaps;
	}

	_sdl->update_resizeable(TRUE);
	return TRUE;
}

BOOL sdlDispContext::uninit(DispClientContext* disp)
{
	if (!disp)
		return FALSE;

	_disp = nullptr;
	_sdl->update_resizeable(FALSE);
	return TRUE;
}

sdlDispContext::sdlDispContext(SdlContext* sdl) : _sdl(sdl)
{
	SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

	WINPR_ASSERT(_sdl);
	WINPR_ASSERT(_sdl->context()->settings);
	WINPR_ASSERT(_sdl->context()->pubSub);

	auto settings = _sdl->context()->settings;
	auto pubSub = _sdl->context()->pubSub;

	_lastSentWidth = _targetWidth = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
	_lastSentHeight = _targetHeight = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
	PubSub_SubscribeActivated(pubSub, sdlDispContext::OnActivated);
	PubSub_SubscribeGraphicsReset(pubSub, sdlDispContext::OnGraphicsReset);
	addTimer();
}

sdlDispContext::~sdlDispContext()
{
	wPubSub* pubSub = _sdl->context()->pubSub;
	WINPR_ASSERT(pubSub);

	PubSub_UnsubscribeActivated(pubSub, sdlDispContext::OnActivated);
	PubSub_UnsubscribeGraphicsReset(pubSub, sdlDispContext::OnGraphicsReset);
	SDL_RemoveTimer(_timer);
	SDL_Quit();
}
