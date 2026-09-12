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

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>

#include <SDL3/SDL.h>

#include <winpr/assert.h>
#include <winpr/cmdline.h>
#include <winpr/crt.h>

#include <freerdp/log.h>

#define TAG CLIENT_TAG("sdl")

#include "sdl_monitor.hpp"
#include "sdl_context.hpp"

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

/* Parses a single "<W>x<H>@<X>x<Y>" token. Uses strtoul/strtol with explicit endptr checks
 * (rather than sscanf) so trailing garbage after a well-formed prefix is rejected. */
[[nodiscard]] static bool parseVMonitorToken(const std::string& token, rdpMonitor& monitor)
{
	const char* input = token.c_str();
	char* endPtr = nullptr;

	errno = 0;
	const unsigned long w = strtoul(input, &endPtr, 10);
	if (((w == 0) || (w == ULONG_MAX)) && (errno != 0))
		return false;
	if ((w == 0) || (w > UINT16_MAX))
		return false;
	if (!endPtr || (*endPtr != 'x'))
		return false;

	errno = 0;
	const unsigned long h = strtoul(endPtr + 1, &endPtr, 10);
	if (((h == 0) || (h == ULONG_MAX)) && (errno != 0))
		return false;
	if ((h == 0) || (h > UINT16_MAX))
		return false;
	if (!endPtr || (*endPtr != '@'))
		return false;

	errno = 0;
	const long x = strtol(endPtr + 1, &endPtr, 10);
	if (((x == LONG_MIN) || (x == LONG_MAX)) && (errno != 0))
		return false;
	if (!endPtr || (*endPtr != 'x'))
		return false;

	errno = 0;
	const long y = strtol(endPtr + 1, &endPtr, 10);
	if (((y == LONG_MIN) || (y == LONG_MAX)) && (errno != 0))
		return false;
	if (!endPtr || (*endPtr != '\0'))
		return false;

	monitor = {};
	monitor.x = static_cast<INT32>(x);
	monitor.y = static_cast<INT32>(y);
	monitor.width = static_cast<INT32>(w);
	monitor.height = static_cast<INT32>(h);
	monitor.attributes.physicalWidth = static_cast<UINT32>(w);
	monitor.attributes.physicalHeight = static_cast<UINT32>(h);
	monitor.attributes.desktopScaleFactor = 100;
	monitor.attributes.deviceScaleFactor = 100;
	monitor.attributes.orientation = ORIENTATION_LANDSCAPE;
	return true;
}

/* Two monitors are considered adjacent if their rectangles touch or overlap along both axes
 * (sharing at least a corner point counts as touching). */
[[nodiscard]] static bool monitorsTouch(const rdpMonitor& a, const rdpMonitor& b)
{
	const bool xOverlap = (a.x <= b.x + b.width) && (b.x <= a.x + a.width);
	const bool yOverlap = (a.y <= b.y + b.height) && (b.y <= a.y + a.height);
	return xOverlap && yOverlap;
}

/* Strict rectangle intersection (positive area), as opposed to monitorsTouch() which also
 * counts zero-area edge/corner contact. */
[[nodiscard]] static bool monitorsOverlap(const rdpMonitor& a, const rdpMonitor& b)
{
	const bool xOverlap = (a.x < b.x + b.width) && (b.x < a.x + a.width);
	const bool yOverlap = (a.y < b.y + b.height) && (b.y < a.y + a.height);
	return xOverlap && yOverlap;
}

[[nodiscard]] bool monitorsHaveOverlap(const std::vector<rdpMonitor>& monitors, size_t& a,
                                       size_t& b)
{
	for (a = 0; a < monitors.size(); a++)
	{
		for (b = a + 1; b < monitors.size(); b++)
		{
			if (monitorsOverlap(monitors[a], monitors[b]))
				return true;
		}
	}
	return false;
}

/* Flood-fill adjacency starting from the first monitor; a layout is contiguous if every
 * monitor is reachable, i.e. there is a single connected region with no isolated islands. */
[[nodiscard]] bool monitorsAreContiguous(const std::vector<rdpMonitor>& monitors)
{
	if (monitors.size() <= 1)
		return true;

	std::vector<bool> visited(monitors.size(), false);
	std::vector<size_t> stack{ 0 };
	visited[0] = true;
	size_t visitedCount = 1;

	while (!stack.empty())
	{
		const auto current = stack.back();
		stack.pop_back();

		for (size_t x = 0; x < monitors.size(); x++)
		{
			if (visited[x])
				continue;
			if (!monitorsTouch(monitors[current], monitors[x]))
				continue;
			visited[x] = true;
			visitedCount++;
			stack.push_back(x);
		}
	}

	return visitedCount == monitors.size();
}

bool sdl_parse_vmonitors(const char* value, std::vector<rdpMonitor>& monitors)
{
	monitors.clear();
	if (!value)
	{
		WLog_ERR(TAG, "/vmonitors requires a value");
		return false;
	}

	size_t count = 0;
	char** ptr = CommandLineParseCommaSeparatedValues(value, &count);
	if (!ptr)
	{
		WLog_ERR(TAG, "/vmonitors: failed to parse '%s'", value);
		return false;
	}

	bool rc = true;
	if ((count == 0) || (count > 16))
	{
		WLog_ERR(TAG, "/vmonitors: expected between 1 and 16 monitor definitions, got %" PRIuz,
		         count);
		rc = false;
	}

	for (size_t x = 0; rc && (x < count); x++)
	{
		rdpMonitor monitor{};
		if (!parseVMonitorToken(ptr[x], monitor))
		{
			WLog_ERR(TAG,
			         "/vmonitors: invalid monitor definition '%s', expected <W>x<H>@<X>x<Y>",
			         ptr[x]);
			rc = false;
			break;
		}
		monitors.push_back(monitor);
	}

	CommandLineParserFree(ptr);

	if (rc)
	{
		size_t a = 0;
		size_t b = 0;
		if (monitorsHaveOverlap(monitors, a, b))
		{
			WLog_ERR(TAG, "/vmonitors: monitor %" PRIuz " and %" PRIuz " overlap, monitors must "
			              "not overlap",
			         a, b);
			rc = false;
		}
	}

	if (rc && !monitorsAreContiguous(monitors))
	{
		WLog_ERR(TAG, "/vmonitors: monitor layout is not contiguous, all monitors must form a "
		              "single connected region with no gaps");
		rc = false;
	}

	if (!rc)
	{
		monitors.clear();
		return false;
	}

	monitors.front().is_primary = TRUE;

	/* Sort deterministically (primary first, then ascending x/y) to match the order
	 * freerdp_settings_set_monitor_def_array_sorted() (libfreerdp/common/settings.c) produces in
	 * FreeRDP_MonitorDefArray. SdlContext relies on this correspondence to map each window back
	 * to its virtual monitor entry (by array index) for live resize handling. */
	std::sort(monitors.begin(), monitors.end(),
	         [](const rdpMonitor& a, const rdpMonitor& b)
	         {
		         if (a.is_primary != b.is_primary)
			         return a.is_primary != FALSE;
		         if (a.x != b.x)
			         return a.x < b.x;
		         return a.y < b.y;
	         });

	return true;
}

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

[[nodiscard]] static BOOL sdl_apply_mon_max_size(SdlContext* sdl, UINT32* pMaxWidth,
                                                 UINT32* pMaxHeight)
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

[[nodiscard]] static BOOL sdl_apply_max_size(SdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
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

[[nodiscard]] static BOOL sdl_apply_display_properties(SdlContext* sdl)
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
			auto monitor = sdl->getDisplay(id);
			monitor.is_primary = true;
			monitor.x = 0;
			monitor.y = 0;
			monitors.emplace_back(monitor);
			return freerdp_settings_set_monitor_def_array_sorted(settings, monitors.data(),
			                                                     monitors.size());
		}
		return TRUE;
	}
	for (const auto& id : sdl->monitorIds())
	{
		const auto monitor = sdl->getDisplay(id);
		monitors.emplace_back(monitor);
	}
	// /monitors: may select a subset that excludes the SDL primary. The
	// library rejects arrays without a primary, so promote the first entry
	// when none is marked.
	if (!monitors.empty() && std::none_of(monitors.cbegin(), monitors.cend(),
	                                      [](const rdpMonitor& m) { return m.is_primary; }))
		monitors.at(0).is_primary = true;
	return freerdp_settings_set_monitor_def_array_sorted(settings, monitors.data(),
	                                                     monitors.size());
}

[[nodiscard]] static BOOL sdl_detect_single_window(SdlContext* sdl, UINT32* pMaxWidth,
                                                   UINT32* pMaxHeight)
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

		if (!sdl_apply_display_properties(sdl))
			return FALSE;
		return sdl_apply_max_size(sdl, pMaxWidth, pMaxHeight);
	}
	return sdl_apply_mon_max_size(sdl, pMaxWidth, pMaxHeight);
}

[[nodiscard]] static BOOL sdl_apply_virtual_monitors(SdlContext* sdl, UINT32* pMaxWidth,
                                                     UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	rdpSettings* settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	if (freerdp_settings_get_bool(settings, FreeRDP_Fullscreen))
	{
		WLog_ERR(TAG, "/vmonitors is not compatible with /f");
		return FALSE;
	}
	if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
	{
		WLog_ERR(TAG, "/vmonitors is not compatible with /multimon or /span");
		return FALSE;
	}
	if (freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds) > 0)
	{
		WLog_ERR(TAG, "/vmonitors is not compatible with /monitors:<id,...>");
		return FALSE;
	}

	auto monitors = sdl->virtualMonitors();
	const auto primaryId = SDL_GetPrimaryDisplay();
	for (auto& monitor : monitors)
		monitor.orig_screen = primaryId;

	if (!freerdp_settings_set_monitor_def_array_sorted(settings, monitors.data(), monitors.size()))
		return FALSE;

	if (!freerdp_settings_set_bool(settings, FreeRDP_UseMultimon, TRUE))
		return FALSE;

	sdl->setMonitorIds(std::vector<SDL_DisplayID>(monitors.size(), primaryId));

	return sdl_apply_mon_max_size(sdl, pMaxWidth, pMaxHeight);
}

BOOL sdl_detect_monitors(SdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	rdpSettings* settings = sdl->context()->settings;
	WINPR_ASSERT(settings);

	if (sdl->hasVirtualMonitors())
		return sdl_apply_virtual_monitors(sdl, pMaxWidth, pMaxHeight);

	const auto& ids = sdl->getDisplayIds();
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
