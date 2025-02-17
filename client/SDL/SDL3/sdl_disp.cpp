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

bool sdlDispContext::sendResize()
{
	auto settings = _sdl->context()->settings;

	if (!settings)
		return false;

	if (!_activated || !_disp)
		return true;

	if (GetTickCount64() - _lastSentDate < RESIZE_MIN_DELAY)
		return true;

	auto layout = currentMonitorLayout();
	if (layout == _lastSentLayout)
		return true;

	return sendLayout(layout) == CHANNEL_RC_OK;
}

bool sdlDispContext::set_window_resizable()
{
	_sdl->update_resizeable(TRUE);
	return true;
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
	if (!sdlDisp->_activated)
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

UINT sdlDispContext::sendLayout(const std::vector<rdpMonitor>& monitors)
{
	_lastSentDate = GetTickCount64();
	_lastSentLayout = monitors;
	if (monitors.empty())
		return CHANNEL_RC_OK;

	std::vector<DISPLAY_CONTROL_MONITOR_LAYOUT> layouts;
	layouts.reserve(monitors.size());

	for (const auto& monitor : monitors)
	{
		DISPLAY_CONTROL_MONITOR_LAYOUT layout = {};

		layout.Flags = (monitor.is_primary ? DISPLAY_CONTROL_MONITOR_PRIMARY : 0);
		layout.Left = monitor.x;
		layout.Top = monitor.y;
		layout.Width = WINPR_ASSERTING_INT_CAST(uint32_t, monitor.width);
		layout.Height = WINPR_ASSERTING_INT_CAST(uint32_t, monitor.height);
		layout.Orientation = ORIENTATION_LANDSCAPE;
		layout.PhysicalWidth = monitor.attributes.physicalWidth;
		layout.PhysicalHeight = monitor.attributes.physicalHeight;

		switch (monitor.attributes.orientation)
		{
			case 90:
				layout.Orientation = ORIENTATION_PORTRAIT;
				break;

			case 180:
				layout.Orientation = ORIENTATION_LANDSCAPE_FLIPPED;
				break;

			case 270:
				layout.Orientation = ORIENTATION_PORTRAIT_FLIPPED;
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
				layout.Orientation = ORIENTATION_LANDSCAPE;
				break;
		}

		layout.DesktopScaleFactor = monitor.attributes.desktopScaleFactor;
		layout.DeviceScaleFactor = monitor.attributes.deviceScaleFactor;

		layouts.push_back(layout);
	}

	WINPR_ASSERT(_disp);
	const size_t len = layouts.size();
	WINPR_ASSERT(len <= UINT32_MAX);
	return IFCALLRESULT(CHANNEL_RC_OK, _disp->SendMonitorLayout, _disp, static_cast<UINT32>(len),
	                    layouts.data());
}

bool sdlDispContext::addTimer()
{
	if (SDL_WasInit(SDL_INIT_EVENTS) == 0)
		return FALSE;

	SDL_RemoveTimer(_timer);
	WLog_Print(_sdl->log, WLOG_TRACE, "adding new display check timer");

	_timer_retries = 0;
	sendResize();
	_timer = SDL_AddTimer(1000, sdlDispContext::OnTimer, this);
	return true;
}

bool sdlDispContext::handle_display_event(const SDL_DisplayEvent* ev)
{
	WINPR_ASSERT(ev);

	switch (ev->type)
	{
		case SDL_EVENT_DISPLAY_ADDED:
			SDL_Log("A new display with id %u was connected", ev->displayID);
			return true;
		case SDL_EVENT_DISPLAY_REMOVED:
			SDL_Log("The display with id %u was disconnected", ev->displayID);
			return true;
		case SDL_EVENT_DISPLAY_ORIENTATION:
			SDL_Log("The orientation of display with id %u was changed", ev->displayID);
			return true;
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
			gdi_send_suppress_output(_sdl->context()->gdi, TRUE);

			return true;

		case SDL_EVENT_WINDOW_EXPOSED:
		case SDL_EVENT_WINDOW_SHOWN:
		case SDL_EVENT_WINDOW_MAXIMIZED:
		case SDL_EVENT_WINDOW_RESTORED:
			gdi_send_suppress_output(_sdl->context()->gdi, FALSE);
			return true;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			return addTimer();

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

uint32_t sdlDispContext::DisplayControlCaps(DispClientContext* disp, uint32_t maxNumMonitors,
                                            uint32_t maxMonitorAreaFactorA,
                                            uint32_t maxMonitorAreaFactorB)
{
	/* we're called only if dynamic resolution update is activated */
	WINPR_ASSERT(disp);

	auto sdlDisp = reinterpret_cast<sdlDispContext*>(disp->custom);
	return sdlDisp->DisplayControlCaps(maxNumMonitors, maxMonitorAreaFactorA,
	                                   maxMonitorAreaFactorB);
}

std::vector<rdpMonitor> sdlDispContext::currentMonitorLayout() const
{
	auto monitors = static_cast<const rdpMonitor*>(
	    freerdp_settings_get_pointer_array(_sdl->context()->settings, FreeRDP_MonitorDefArray, 0));
	auto count = freerdp_settings_get_uint32(_sdl->context()->settings, FreeRDP_MonitorCount);

	std::vector<rdpMonitor> layout;
	layout.reserve(count);
	for (size_t x = 0; x < count; x++)
		layout.push_back(monitors[x]);

	return layout;
}

uint32_t sdlDispContext::DisplayControlCaps(uint32_t maxNumMonitors, uint32_t maxMonitorAreaFactorA,
                                            uint32_t maxMonitorAreaFactorB)
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

	_sdl->update_resizeable(TRUE);
	return true;
}

bool sdlDispContext::uninit(DispClientContext* disp)
{
	if (!disp)
		return false;

	_disp = nullptr;
	_sdl->update_resizeable(FALSE);
	return true;
}

sdlDispContext::sdlDispContext(SdlContext* sdl) : _sdl(sdl)
{
	SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);

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
	SDL_Quit();
}
