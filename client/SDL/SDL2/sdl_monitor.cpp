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

#include <SDL.h>

#include <winpr/assert.h>
#include <winpr/crt.h>

#include <freerdp/log.h>

#define TAG CLIENT_TAG("sdl")

#include "sdl_monitor.hpp"
#include "sdl_freerdp.hpp"

using MONITOR_INFO = struct
{
	RECTANGLE_16 area;
	RECTANGLE_16 workarea;
	BOOL primary;
};

using VIRTUAL_SCREEN = struct
{
	int nmonitors;
	RECTANGLE_16 area;
	RECTANGLE_16 workarea;
	MONITOR_INFO* monitors;
};

/* See MSDN Section on Multiple Display Monitors: http://msdn.microsoft.com/en-us/library/dd145071
 */

int sdl_list_monitors(SdlContext* sdl)
{
	SDL_Init(SDL_INIT_VIDEO);
	const int nmonitors = SDL_GetNumVideoDisplays();

	printf("listing %d monitors:\n", nmonitors);
	for (int i = 0; i < nmonitors; i++)
	{
		SDL_Rect rect = {};
		const int brc = SDL_GetDisplayBounds(i, &rect);
		const char* name = SDL_GetDisplayName(i);

		if (brc != 0)
			continue;
		printf("     %s [%d] [%s] %dx%d\t+%d+%d\n", (i == 0) ? "*" : " ", i, name, rect.w, rect.h,
		       rect.x, rect.y);
	}

	SDL_Quit();
	return 0;
}

static BOOL sdl_is_monitor_id_active(SdlContext* sdl, UINT32 id)
{
	const rdpSettings* settings = nullptr;

	WINPR_ASSERT(sdl);

	settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	const UINT32 NumMonitorIds = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	if (!NumMonitorIds)
		return TRUE;

	for (UINT32 index = 0; index < NumMonitorIds; index++)
	{
		auto cur = static_cast<const UINT32*>(
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorIds, index));
		if (cur && (*cur == id))
			return TRUE;
	}

	return FALSE;
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
			*pMaxWidth = monitor->width;
			*pMaxHeight = monitor->height;
		}
		else if (freerdp_settings_get_bool(settings, FreeRDP_Workarea))
		{
			SDL_Rect rect = {};
			SDL_GetDisplayUsableBounds(monitor->orig_screen, &rect);
			*pMaxWidth = rect.w;
			*pMaxHeight = rect.h;
		}
		else if (freerdp_settings_get_uint32(settings, FreeRDP_PercentScreen) > 0)
		{
			SDL_Rect rect = {};
			SDL_GetDisplayUsableBounds(monitor->orig_screen, &rect);

			*pMaxWidth = rect.w;
			*pMaxHeight = rect.h;

			if (freerdp_settings_get_bool(settings, FreeRDP_PercentScreenUseWidth))
				*pMaxWidth =
				    (rect.w * freerdp_settings_get_uint32(settings, FreeRDP_PercentScreen)) / 100;

			if (freerdp_settings_get_bool(settings, FreeRDP_PercentScreenUseHeight))
				*pMaxHeight =
				    (rect.h * freerdp_settings_get_uint32(settings, FreeRDP_PercentScreen)) / 100;
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

#if SDL_VERSION_ATLEAST(2, 0, 10)
static UINT32 sdl_orientaion_to_rdp(SDL_DisplayOrientation orientation)
{
	switch (orientation)
	{
		case SDL_ORIENTATION_LANDSCAPE:
			return ORIENTATION_LANDSCAPE;
		case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
			return ORIENTATION_LANDSCAPE_FLIPPED;
		case SDL_ORIENTATION_PORTRAIT_FLIPPED:
			return ORIENTATION_PORTRAIT_FLIPPED;
		case SDL_ORIENTATION_PORTRAIT:
		default:
			return ORIENTATION_PORTRAIT;
	}
}
#endif

static Uint32 scale(Uint32 val, float scale)
{
	const auto dval = static_cast<float>(val);
	const auto sval = dval / scale;
	return static_cast<Uint32>(sval);
}

static BOOL sdl_apply_display_properties(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	rdpSettings* settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	const UINT32 numIds = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorDefArray, nullptr, numIds))
		return FALSE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorCount, numIds))
		return FALSE;

	for (UINT32 x = 0; x < numIds; x++)
	{
		auto id = static_cast<const int*>(
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorIds, x));
		WINPR_ASSERT(id);

		float ddpi = 1.0f;
		float hdpi = 1.0f;
		float vdpi = 1.0f;
		SDL_Rect rect = {};

		if (SDL_GetDisplayBounds(*id, &rect) < 0)
			return FALSE;

		if (SDL_GetDisplayDPI(*id, &ddpi, &hdpi, &vdpi) < 0)
			return FALSE;

		WINPR_ASSERT(rect.w > 0);
		WINPR_ASSERT(rect.h > 0);
		WINPR_ASSERT(ddpi > 0);
		WINPR_ASSERT(hdpi > 0);
		WINPR_ASSERT(vdpi > 0);

		bool highDpi = hdpi > 100;

		if (highDpi)
		{
			// HighDPI is problematic with SDL: We can only get native resolution by creating a
			// window. Work around this by checking the supported resolutions (and keep maximum)
			// Also scale the DPI
			const SDL_Rect scaleRect = rect;
			for (int i = 0; i < SDL_GetNumDisplayModes(*id); i++)
			{
				SDL_DisplayMode mode = {};
				SDL_GetDisplayMode(x, i, &mode);

				if (mode.w > rect.w)
				{
					rect.w = mode.w;
					rect.h = mode.h;
				}
				else if (mode.w == rect.w)
				{
					if (mode.h > rect.h)
					{
						rect.w = mode.w;
						rect.h = mode.h;
					}
				}
			}

			const float dw = 1.0f * static_cast<float>(rect.w) / static_cast<float>(scaleRect.w);
			const float dh = 1.0f * static_cast<float>(rect.h) / static_cast<float>(scaleRect.h);
			hdpi /= dw;
			vdpi /= dh;
		}

#if SDL_VERSION_ATLEAST(2, 0, 10)
		const SDL_DisplayOrientation orientation = SDL_GetDisplayOrientation(*id);
		const UINT32 rdp_orientation = sdl_orientaion_to_rdp(orientation);
#else
		const UINT32 rdp_orientation = ORIENTATION_LANDSCAPE;
#endif

		auto monitor = static_cast<rdpMonitor*>(
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, x));
		WINPR_ASSERT(monitor);

		/* windows uses 96 dpi as 'default' and the scale factors are in percent. */
		const auto factor = ddpi / 96.0f * 100.0f;
		monitor->orig_screen = x;
		monitor->x = rect.x;
		monitor->y = rect.y;
		monitor->width = rect.w;
		monitor->height = rect.h;
		monitor->is_primary = x == 0;
		monitor->attributes.desktopScaleFactor = static_cast<UINT32>(factor);
		monitor->attributes.deviceScaleFactor = 100;
		monitor->attributes.orientation = rdp_orientation;
		monitor->attributes.physicalWidth = scale(rect.w, hdpi);
		monitor->attributes.physicalHeight = scale(rect.h, vdpi);
	}
	return TRUE;
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
			const size_t id =
			    (!sdl->windows.empty()) ? sdl->windows.begin()->second.displayIndex() : 0;
			if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, &id, 1))
				return FALSE;
		}
		else
		{

			/* Always sets number of monitors from command-line to just 1.
			 * If the monitor is invalid then we will default back to current monitor
			 * later as a fallback. So, there is no need to validate command-line entry here.
			 */
			if (!freerdp_settings_set_uint32(settings, FreeRDP_NumMonitorIds, 1))
				return FALSE;
		}

		// TODO: Fill monitor struct
		if (!sdl_apply_display_properties(sdl))
			return FALSE;
		return sdl_apply_max_size(sdl, pMaxWidth, pMaxHeight);
	}
	return TRUE;
}

BOOL sdl_detect_monitors(SdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	rdpSettings* settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	const int numDisplays = SDL_GetNumVideoDisplays();
	auto nr = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	if (numDisplays < 0)
		return FALSE;

	if (nr == 0)
	{
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, nullptr,
		                                      static_cast<size_t>(numDisplays)))
			return FALSE;
		for (size_t x = 0; x < static_cast<size_t>(numDisplays); x++)
		{
			if (!freerdp_settings_set_pointer_array(settings, FreeRDP_MonitorIds, x, &x))
				return FALSE;
		}
	}
	else
	{

		/* There were more IDs supplied than there are monitors */
		if (nr > static_cast<UINT32>(numDisplays))
		{
			WLog_ERR(TAG,
			         "Found %" PRIu32 " monitor IDs, but only have %" PRIu32 " monitors connected",
			         nr, numDisplays);
			return FALSE;
		}

		std::vector<UINT32> used;
		for (size_t x = 0; x < nr; x++)
		{
			auto cur = static_cast<const UINT32*>(
			    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorIds, x));
			WINPR_ASSERT(cur);

			auto id = *cur;

			/* the ID is no valid monitor index */
			if (id >= nr)
			{
				WLog_ERR(TAG,
				         "Supplied monitor ID[%" PRIuz "]=%" PRIu32 " is invalid, only [0-%" PRIu32
				         "] are allowed",
				         x, id, nr - 1);
				return FALSE;
			}

			/* The ID is already taken */
			if (std::find(used.begin(), used.end(), id) != used.end())
			{
				WLog_ERR(TAG, "Duplicate monitor ID[%" PRIuz "]=%" PRIu32 " detected", x, id);
				return FALSE;
			}
			used.push_back(*cur);
		}
	}

	if (!sdl_apply_display_properties(sdl))
		return FALSE;

	return sdl_detect_single_window(sdl, pMaxWidth, pMaxHeight);
}

INT64 sdl_monitor_id_for_index(SdlContext* sdl, UINT32 index)
{
	WINPR_ASSERT(sdl);
	auto settings = sdl->context()->settings;

	auto nr = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	if (nr == 0)
		return index;

	if (nr <= index)
		return -1;

	auto cur = static_cast<const UINT32*>(
	    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorIds, index));
	WINPR_ASSERT(cur);
	return *cur;
}
