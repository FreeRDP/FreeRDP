/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Monitor Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
 * Copyright 2018 Kai Harms <kharms@rangee.com>
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

#include <freerdp/config.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#include <SDL3/SDL.h>

#include <winpr/assert.h>
#include <winpr/crt.h>

#include <freerdp/log.h>

#define TAG CLIENT_TAG("sdl")

#include "sdl_monitor.hpp"
#include "sdl_freerdp.hpp"

typedef struct
{
	RECTANGLE_16 area;
	RECTANGLE_16 workarea;
	BOOL primary;
} MONITOR_INFO;

typedef struct
{
	int nmonitors;
	RECTANGLE_16 area;
	RECTANGLE_16 workarea;
	MONITOR_INFO* monitors;
} VIRTUAL_SCREEN;

/* See MSDN Section on Multiple Display Monitors: http://msdn.microsoft.com/en-us/library/dd145071
 */

int sdl_list_monitors([[maybe_unused]] SdlContext* sdl)
{
	SDL_Init(SDL_INIT_VIDEO);

	int nmonitors = 0;
	auto ids = SDL_GetDisplays(&nmonitors);

	printf("listing %d monitors:\n", nmonitors);
	for (int i = 0; i < nmonitors; i++)
	{
		SDL_Rect rect = {};
		auto id = ids[i];
		const auto brc = SDL_GetDisplayBounds(id, &rect);
		const char* name = SDL_GetDisplayName(id);

		if (!brc)
			continue;
		printf("     %s [%u] [%s] %dx%d\t+%d+%d\n", (i == 0) ? "*" : " ", id, name, rect.w, rect.h,
		       rect.x, rect.y);
	}
	SDL_free(ids);
	SDL_Quit();
	return 0;
}

static BOOL sdl_apply_mon_max_size(SdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	int32_t left = 0;
	int32_t right = 0;
	int32_t top = 0;
	int32_t bottom = 0;

	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	auto settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	for (size_t x = 0; x < freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount); x++)
	{
		auto monitor = static_cast<const rdpMonitor*>(
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x));

		left = std::min(monitor->x, left);
		top = std::min(monitor->y, top);
		right = std::max(monitor->x + monitor->width, right);
		bottom = std::max(monitor->y + monitor->height, bottom);
	}

	const int32_t w = right - left;
	const int32_t h = bottom - top;

	*pMaxWidth = WINPR_ASSERTING_INT_CAST(uint32_t, w);
	*pMaxHeight = WINPR_ASSERTING_INT_CAST(uint32_t, h);
	return TRUE;
}

static BOOL sdl_apply_max_size(SdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	auto settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	*pMaxWidth = 0;
	*pMaxHeight = 0;

	for (size_t x = 0; x < freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount); x++)
	{
		auto monitor = static_cast<const rdpMonitor*>(
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x));

		if (freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
		{
			*pMaxWidth = WINPR_ASSERTING_INT_CAST(uint32_t, monitor->width);
			*pMaxHeight = WINPR_ASSERTING_INT_CAST(uint32_t, monitor->height);
		}
		else if (freerdp_settings_get_bool(settings, FreeRDP_Workarea))
		{
			SDL_Rect rect = {};
			SDL_GetDisplayUsableBounds(monitor->orig_screen, &rect);
			*pMaxWidth = WINPR_ASSERTING_INT_CAST(uint32_t, rect.w);
			*pMaxHeight = WINPR_ASSERTING_INT_CAST(uint32_t, rect.h);
		}
		else if (freerdp_settings_get_uint32(settings, FreeRDP_PercentScreen) > 0)
		{
			SDL_Rect rect = {};
			SDL_GetDisplayUsableBounds(monitor->orig_screen, &rect);

			*pMaxWidth = WINPR_ASSERTING_INT_CAST(uint32_t, rect.w);
			*pMaxHeight = WINPR_ASSERTING_INT_CAST(uint32_t, rect.h);

			if (freerdp_settings_get_bool(settings, FreeRDP_PercentScreenUseWidth))
				*pMaxWidth = (WINPR_ASSERTING_INT_CAST(uint32_t, rect.w) *
				              freerdp_settings_get_uint32(settings, FreeRDP_PercentScreen)) /
				             100;

			if (freerdp_settings_get_bool(settings, FreeRDP_PercentScreenUseHeight))
				*pMaxHeight = (WINPR_ASSERTING_INT_CAST(uint32_t, rect.h) *
				               freerdp_settings_get_uint32(settings, FreeRDP_PercentScreen)) /
				              100;
		}
		else if (freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth) &&
		         freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight))
		{
			*pMaxWidth = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
			*pMaxHeight = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
		}
	}
	return TRUE;
}

static Uint32 scale(Uint32 val, float scale)
{
	const auto dval = static_cast<float>(val);
	const auto sval = dval / scale;
	return static_cast<Uint32>(sval);
}

static BOOL sdl_apply_monitor_properties(rdpMonitor& monitor, SDL_DisplayID id, bool isPrimary)
{

	float dpi = SDL_GetDisplayContentScale(id);
	float hdpi = dpi;
	float vdpi = dpi;
	SDL_Rect rect = {};

	if (!SDL_GetDisplayBounds(id, &rect))
		return FALSE;

	WINPR_ASSERT(rect.w > 0);
	WINPR_ASSERT(rect.h > 0);

	bool highDpi = dpi > 100;

	if (highDpi)
	{
		// HighDPI is problematic with SDL: We can only get native resolution by creating a
		// window. Work around this by checking the supported resolutions (and keep maximum)
		// Also scale the DPI
		const SDL_Rect scaleRect = rect;
		int count = 0;
		auto modes = SDL_GetFullscreenDisplayModes(id, &count);
		for (int i = 0; i < count; i++)
		{
			auto mode = modes[i];
			if (!mode)
				break;

			if (mode->w > rect.w)
			{
				rect.w = mode->w;
				rect.h = mode->h;
			}
			else if (mode->w == rect.w)
			{
				if (mode->h > rect.h)
				{
					rect.w = mode->w;
					rect.h = mode->h;
				}
			}
		}
		SDL_free(static_cast<void*>(modes));

		const float dw = 1.0f * static_cast<float>(rect.w) / static_cast<float>(scaleRect.w);
		const float dh = 1.0f * static_cast<float>(rect.h) / static_cast<float>(scaleRect.h);
		hdpi /= dw;
		vdpi /= dh;
	}

	const SDL_DisplayOrientation orientation = SDL_GetCurrentDisplayOrientation(id);
	const UINT32 rdp_orientation = sdl::utils::orientaion_to_rdp(orientation);

	/* windows uses 96 dpi as 'default' and the scale factors are in percent. */
	const auto factor = dpi / 96.0f * 100.0f;
	monitor.orig_screen = id;
	monitor.x = rect.x;
	monitor.y = rect.y;
	monitor.width = rect.w;
	monitor.height = rect.h;
	monitor.is_primary = isPrimary;
	monitor.attributes.desktopScaleFactor = static_cast<UINT32>(factor);
	monitor.attributes.deviceScaleFactor = 100;
	monitor.attributes.orientation = rdp_orientation;
	monitor.attributes.physicalWidth = scale(WINPR_ASSERTING_INT_CAST(uint32_t, rect.w), hdpi);
	monitor.attributes.physicalHeight = scale(WINPR_ASSERTING_INT_CAST(uint32_t, rect.h), vdpi);
	return TRUE;
}

static BOOL sdl_apply_display_properties(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	rdpSettings* settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	std::vector<rdpMonitor> monitors;
	if (!freerdp_settings_get_bool(settings, FreeRDP_Fullscreen) &&
	    !freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
	{
		if (freerdp_settings_get_bool(settings, FreeRDP_Workarea))
		{
			if (sdl->monitorIds().empty())
				return FALSE;
			const auto id = sdl->monitorIds().front();
			rdpMonitor monitor = {};
			if (!sdl_apply_monitor_properties(monitor, id, TRUE))
				return FALSE;
			monitors.emplace_back(monitor);
			return freerdp_settings_set_monitor_def_array_sorted(settings, monitors.data(),
			                                                     monitors.size());
		}
		return TRUE;
	}
	for (const auto& id : sdl->monitorIds())
	{
		rdpMonitor monitor = {};
		const auto primary = SDL_GetPrimaryDisplay();
		if (!sdl_apply_monitor_properties(monitor, id, id == primary))
			return FALSE;
		monitors.emplace_back(monitor);
	}
	return freerdp_settings_set_monitor_def_array_sorted(settings, monitors.data(),
	                                                     monitors.size());
}

static BOOL sdl_detect_single_window(SdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	rdpSettings* settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	if ((!freerdp_settings_get_bool(settings, FreeRDP_UseMultimon) &&
	     !freerdp_settings_get_bool(settings, FreeRDP_SpanMonitors)) ||
	    (freerdp_settings_get_bool(settings, FreeRDP_Workarea) &&
	     !freerdp_settings_get_bool(settings, FreeRDP_RemoteApplicationMode)))
	{
		/* If no monitors were specified on the command-line then set the current monitor as active
		 */
		if (freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds) == 0)
		{
			SDL_DisplayID id = 0;
			const auto& ids = sdl->monitorIds();
			if (!ids.empty())
			{
				id = ids.front();
			}
			sdl->setMonitorIds({ id });
		}

		// TODO: Fill monitor struct
		if (!sdl_apply_display_properties(sdl))
			return FALSE;
		return sdl_apply_max_size(sdl, pMaxWidth, pMaxHeight);
	}
	return sdl_apply_mon_max_size(sdl, pMaxWidth, pMaxHeight);
}

BOOL sdl_detect_monitors(SdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	rdpSettings* settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	std::vector<SDL_DisplayID> ids;
	{
		int numDisplays = 0;
		auto sids = SDL_GetDisplays(&numDisplays);
		if (sids && (numDisplays > 0))
			ids = std::vector<SDL_DisplayID>(sids, sids + numDisplays);
		SDL_free(sids);
		if (numDisplays < 0)
			return FALSE;
	}

	auto nr = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	if (nr == 0)
	{
		if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
			sdl->setMonitorIds(ids);
		else
		{
			sdl->setMonitorIds({ ids.front() });
		}
	}
	else
	{
		/* There were more IDs supplied than there are monitors */
		if (nr > ids.size())
		{
			WLog_ERR(TAG,
			         "Found %" PRIu32 " monitor IDs, but only have %" PRIuz " monitors connected",
			         nr, ids.size());
			return FALSE;
		}

		std::vector<SDL_DisplayID> used;
		for (size_t x = 0; x < nr; x++)
		{
			auto cur = static_cast<const UINT32*>(
			    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorIds, x));
			WINPR_ASSERT(cur);

			SDL_DisplayID id = *cur;

			/* the ID is no valid monitor index */
			if (std::find(ids.begin(), ids.end(), id) == ids.end())
			{
				WLog_ERR(TAG, "Supplied monitor ID[%" PRIuz "]=%" PRIu32 " is invalid", x, id);
				return FALSE;
			}

			/* The ID is already taken */
			if (std::find(used.begin(), used.end(), id) != used.end())
			{
				WLog_ERR(TAG, "Duplicate monitor ID[%" PRIuz "]=%" PRIu32 " detected", x, id);
				return FALSE;
			}
			used.push_back(id);
		}
		sdl->setMonitorIds(used);
	}

	if (!sdl_apply_display_properties(sdl))
		return FALSE;

	auto size = static_cast<uint32_t>(sdl->monitorIds().size());
	if (!freerdp_settings_set_uint32(settings, FreeRDP_NumMonitorIds, size))
		return FALSE;

	return sdl_detect_single_window(sdl, pMaxWidth, pMaxHeight);
}
