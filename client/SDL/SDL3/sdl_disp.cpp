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

static auto operator==(const DISPLAY_CONTROL_MONITOR_LAYOUT& a,
                       const DISPLAY_CONTROL_MONITOR_LAYOUT& b)
{
	if (a.Flags != b.Flags)
		return false;
	if (a.Left != b.Left)
		return false;
	if (a.Top != b.Top)
		return false;
	if (a.Width != b.Width)
		return false;
	if (a.Height != b.Height)
		return false;
	if (a.PhysicalWidth != b.PhysicalWidth)
		return false;
	if (a.PhysicalHeight != b.PhysicalHeight)
		return false;
	if (a.Orientation != b.Orientation)
		return false;
	if (a.DesktopScaleFactor != b.DesktopScaleFactor)
		return false;
	if (a.DeviceScaleFactor != b.DeviceScaleFactor)
		return false;
	return true;
}

bool sdlDispContext::settings_changed(const std::vector<DISPLAY_CONTROL_MONITOR_LAYOUT>& layout)
{
	return (layout != _last_sent_layout);
}

bool sdlDispContext::sendResize()
{
	auto settings = _sdl->context()->settings;

	if (!settings)
		return false;

	if (!_activated || !_disp)
		return true;

	if (GetTickCount64() - _lastSentDate < RESIZE_MIN_DELAY)
		return true;

	_lastSentDate = GetTickCount64();

	const UINT32 mcount = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	auto monitors = static_cast<const rdpMonitor*>(
	    freerdp_settings_get_pointer(settings, FreeRDP_MonitorDefArray));
	return sendLayout(monitors, mcount) != CHANNEL_RC_OK;
}

bool sdlDispContext::set_window_resizable()
{
	return _sdl->update_resizeable(true);
}

static bool sdl_disp_check_context(void* context, SdlContext** ppsdl, sdlDispContext** ppsdlDisp,
                                   rdpSettings** ppSettings)
{
	if (!context)
		return false;

	auto sdl = get_context(context);
	if (!sdl)
		return false;

	if (!sdl->context()->settings)
		return false;

	*ppsdl = sdl;
	*ppsdlDisp = &sdl->disp;
	*ppSettings = sdl->context()->settings;
	return true;
}

void sdlDispContext::OnActivated(void* context, const ActivatedEventArgs* e)
{
	SdlContext* sdl = nullptr;
	sdlDispContext* sdlDisp = nullptr;
	rdpSettings* settings = nullptr;

	if (!sdl_disp_check_context(context, &sdl, &sdlDisp, &settings))
		return;

	sdlDisp->_waitingResize = false;

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

	sdlDisp->_waitingResize = false;

	if (sdlDisp->_activated && !freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
	{
		sdlDisp->set_window_resizable();
		sdlDisp->addTimer();
	}
}

Uint32 sdlDispContext::OnTimer(void* param, [[maybe_unused]] SDL_TimerID timerID, Uint32 interval)
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

	auto rc = sdlDisp->sendResize();
	if (!rc)
		WLog_Print(sdl->log, WLOG_TRACE, "sent new display layout, result %d", rc);

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
	layouts.reserve(nmonitors);

	for (size_t i = 0; i < nmonitors; i++)
	{
		auto monitor = &monitors[i];
		DISPLAY_CONTROL_MONITOR_LAYOUT layout = {};

		layout.Flags = (monitor->is_primary ? DISPLAY_CONTROL_MONITOR_PRIMARY : 0);
		layout.Left = monitor->x;
		layout.Top = monitor->y;
		layout.Width = WINPR_ASSERTING_INT_CAST(uint32_t, monitor->width);
		layout.Height = WINPR_ASSERTING_INT_CAST(uint32_t, monitor->height);
		layout.Orientation = ORIENTATION_LANDSCAPE;
		layout.PhysicalWidth = monitor->attributes.physicalWidth;
		layout.PhysicalHeight = monitor->attributes.physicalHeight;

		switch (monitor->attributes.orientation)
		{
			case ORIENTATION_PORTRAIT:
				layout.Orientation = ORIENTATION_PORTRAIT;
				break;

			case ORIENTATION_LANDSCAPE_FLIPPED:
				layout.Orientation = ORIENTATION_LANDSCAPE_FLIPPED;
				break;

			case ORIENTATION_PORTRAIT_FLIPPED:
				layout.Orientation = ORIENTATION_PORTRAIT_FLIPPED;
				break;

			case ORIENTATION_LANDSCAPE:
			default:
				/* MS-RDPEDISP - 2.2.2.2.1:
				 * Orientation (4 bytes): A 32-bit unsigned integer that specifies the
				 * orientation of the monitor in degrees. Valid values are 0, 90, 180
				 * or 270
				 *
				 * So we default to ORIENTATION_LANDSCAPE
				 */
				layout.Orientation = ORIENTATION_LANDSCAPE;
				break;
		}

		layout.DesktopScaleFactor = monitor->attributes.desktopScaleFactor;
		layout.DeviceScaleFactor = monitor->attributes.deviceScaleFactor;

		auto mask = freerdp_settings_get_uint64(settings, FreeRDP_MonitorOverrideFlags);
		if ((mask & FREERDP_MONITOR_OVERRIDE_ORIENTATION) != 0)
			layout.Orientation = freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
		if ((mask & FREERDP_MONITOR_OVERRIDE_DESKTOP_SCALE) != 0)
			layout.DesktopScaleFactor =
			    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
		if ((mask & FREERDP_MONITOR_OVERRIDE_DEVICE_SCALE) != 0)
			layout.DeviceScaleFactor =
			    freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
		layouts.emplace_back(layout);
	}

	if (!settings_changed(layouts))
		return true;

	WINPR_ASSERT(_disp);
	const size_t len = layouts.size();
	WINPR_ASSERT(len <= UINT32_MAX);
	ret = IFCALLRESULT(CHANNEL_RC_OK, _disp->SendMonitorLayout, _disp, static_cast<UINT32>(len),
	                   layouts.data());
	if (ret != CHANNEL_RC_OK)
		return ret;
	_last_sent_layout = layouts;
	return ret;
}

bool sdlDispContext::addTimer()
{
	if (SDL_WasInit(SDL_INIT_EVENTS) == 0)
		return false;

	SDL_RemoveTimer(_timer);
	WLog_Print(_sdl->log, WLOG_TRACE, "adding new display check timer");

	_timer_retries = 0;
	sendResize();
	_timer = SDL_AddTimer(1000, sdlDispContext::OnTimer, this);
	return true;
}

bool sdlDispContext::updateMonitor(SDL_WindowID id)
{
	auto settings = _sdl->context()->settings;
	if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		return updateMonitors(SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED);

	if (!freerdp_settings_get_bool(_sdl->context()->settings, FreeRDP_DynamicResolutionUpdate))
		return true;

	const auto& window = _sdl->windows.at(id);
	auto monitor = window.monitor();

	monitor.is_primary = TRUE;
	if (!freerdp_settings_set_monitor_def_array_sorted(settings, &monitor, 1))
		return false;

	return addTimer();
}

bool sdlDispContext::updateMonitors(SDL_EventType type)
{
	switch (type)
	{
		case SDL_EVENT_DISPLAY_ADDED:
		case SDL_EVENT_DISPLAY_REMOVED:
		case SDL_EVENT_DISPLAY_MOVED:
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "TODO [%s] Not fully supported yet",
			            sdl_event_type_str(type));
			break;
		default:
			break;
	}

	auto settings = _sdl->context()->settings;
	if (!freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
		return true;

	if (!freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
		return true;

	std::vector<rdpMonitor> monitors;
	monitors.reserve(_sdl->windows.size());
	for (auto& smon : _sdl->windows)
	{
		monitors.emplace_back(smon.second.monitor());
	}

	if (!freerdp_settings_set_monitor_def_array_sorted(settings, monitors.data(), monitors.size()))
		return false;
	return addTimer();
}

bool sdlDispContext::handle_display_event(const SDL_DisplayEvent* ev)
{
	WINPR_ASSERT(ev);

	switch (ev->type)
	{
		case SDL_EVENT_DISPLAY_ADDED:
			SDL_Log("A new display with id %u was connected", ev->displayID);
			return updateMonitors(ev->type);
		case SDL_EVENT_DISPLAY_REMOVED:
			SDL_Log("The display with id %u was disconnected", ev->displayID);
			return updateMonitors(ev->type);
		case SDL_EVENT_DISPLAY_ORIENTATION:
			SDL_Log("The orientation of display with id %u was changed", ev->displayID);
			return updateMonitors(ev->type);
		case SDL_EVENT_DISPLAY_MOVED:
			SDL_Log("The display with id %u was moved", ev->displayID);
			return updateMonitors(ev->type);
		case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
			SDL_Log("The display with id %u changed scale", ev->displayID);
			return updateMonitors(ev->type);
		case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
			SDL_Log("The display with id %u changed mode", ev->displayID);
			return updateMonitors(ev->type);
		case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
			SDL_Log("The display with id %u changed desktop mode", ev->displayID);
			return updateMonitors(ev->type);
		default:
			return true;
	}
}

bool sdlDispContext::handle_window_event(const SDL_WindowEvent* ev)
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
			return _sdl->redraw(true);
		case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
			return updateMonitor(ev->windowID);
		case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
			return updateMonitor(ev->windowID);

		case SDL_EVENT_WINDOW_EXPOSED:
		case SDL_EVENT_WINDOW_SHOWN:
		case SDL_EVENT_WINDOW_MAXIMIZED:
		case SDL_EVENT_WINDOW_RESTORED:
			if (!_sdl->redraw())
				return false;

			/* fallthrough */
			WINPR_FALLTHROUGH
		case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_RESIZED:
			return updateMonitor(ev->windowID);
		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			WINPR_ASSERT(_sdl);
			_sdl->input.keyboard_grab(ev->windowID, false);
			return true;
		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			WINPR_ASSERT(_sdl);
			_sdl->input.keyboard_grab(ev->windowID, true);
			return _sdl->input.keyboard_focus_in();
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			return _sdl->input.keyboard_focus_in();

		default:
			return true;
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
	_activated = true;

	if (freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
		return CHANNEL_RC_OK;

	WLog_DBG(TAG, "DisplayControlCapsPdu: setting the window as resizable");
	return set_window_resizable() ? CHANNEL_RC_OK : CHANNEL_RC_NO_MEMORY;
}

bool sdlDispContext::init(DispClientContext* disp)
{
	if (!disp)
		return false;

	auto settings = _sdl->context()->settings;

	if (!settings)
		return false;

	_disp = disp;
	disp->custom = this;

	if (freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
	{
		disp->DisplayControlCaps = sdlDispContext::DisplayControlCaps;
	}

	return _sdl->update_resizeable(true);
}

bool sdlDispContext::uninit(DispClientContext* disp)
{
	if (!disp)
		return false;

	_disp = nullptr;
	return _sdl->update_resizeable(false);
}

sdlDispContext::sdlDispContext(SdlContext* sdl) : _sdl(sdl)
{
	WINPR_ASSERT(_sdl);
	WINPR_ASSERT(_sdl->context()->settings);
	WINPR_ASSERT(_sdl->context()->pubSub);

	auto pubSub = _sdl->context()->pubSub;

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
}
