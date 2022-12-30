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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include <winpr/assert.h>
#include <winpr/crt.h>

#include <freerdp/log.h>

#define TAG CLIENT_TAG("sdl")

#include "sdl_monitor.h"

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

int sdl_list_monitors(sdlContext* sdl)
{
	SDL_Init(SDL_INIT_VIDEO);
	const int nmonitors = SDL_GetNumVideoDisplays();

	for (int i = 0; i < nmonitors; i++)
	{
		SDL_Rect rect = { 0 };
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

static BOOL sdl_is_monitor_id_active(sdlContext* sdl, UINT32 id)
{
	UINT32 index;
	const rdpSettings* settings;

	WINPR_ASSERT(sdl);

	settings = sdl->common.context.settings;
	WINPR_ASSERT(settings);

	if (!settings->NumMonitorIds)
		return TRUE;

	for (index = 0; index < settings->NumMonitorIds; index++)
	{
		if (settings->MonitorIds[index] == id)
			return TRUE;
	}

	return FALSE;
}

static BOOL sdl_apply_max_size(sdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	rdpSettings* settings = sdl->common.context.settings;
	WINPR_ASSERT(settings);

	*pMaxWidth = 0;
	*pMaxHeight = 0;

	for (size_t x = 0; x < settings->MonitorCount; x++)
	{
		const rdpMonitor* monitor = &settings->MonitorDefArray[x];

		if (settings->Fullscreen)
		{
			*pMaxWidth = monitor->width;
			*pMaxHeight = monitor->height;
		}
		else if (settings->Workarea)
		{
			SDL_Rect rect = { 0 };
			SDL_GetDisplayUsableBounds(monitor->orig_screen, &rect);
			*pMaxWidth = rect.w;
			*pMaxHeight = rect.h;
		}
		else if (settings->PercentScreen)
		{
			SDL_Rect rect = { 0 };
			SDL_GetDisplayUsableBounds(monitor->orig_screen, &rect);

			*pMaxWidth = rect.w;
			*pMaxHeight = rect.h;

			if (settings->PercentScreenUseWidth)
				*pMaxWidth = (rect.w * settings->PercentScreen) / 100;

			if (settings->PercentScreenUseHeight)
				*pMaxHeight = (rect.h * settings->PercentScreen) / 100;
		}
		else if (settings->DesktopWidth && settings->DesktopHeight)
		{
			*pMaxWidth = settings->DesktopWidth;
			*pMaxHeight = settings->DesktopHeight;
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
			return ORIENTATION_PREFERENCE_LANDSCAPE_FLIPPED;
		case SDL_ORIENTATION_PORTRAIT_FLIPPED:
			return ORIENTATION_PORTRAIT_FLIPPED;
		case SDL_ORIENTATION_PORTRAIT:
		default:
			return ORIENTATION_PORTRAIT;
	}
}
#endif

static BOOL sdl_apply_display_properties(sdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	rdpSettings* settings = sdl->common.context.settings;
	WINPR_ASSERT(settings);

	const UINT32 numIds = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorDefArray, NULL, numIds))
		return FALSE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorCount, numIds))
		return FALSE;

	for (UINT32 x = 0; x < numIds; x++)
	{
		const UINT32* id = freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorIds, x);
		WINPR_ASSERT(id);

		float hdpi;
		float vdpi;
		SDL_Rect rect = { 0 };

		SDL_GetDisplayBounds(*id, &rect);
		SDL_GetDisplayDPI(*id, NULL, &hdpi, &vdpi);
		if (sdl->highDpi)
		{
			// HighDPI is problematic with SDL: We can only get native resolution by creating a
			// window. Work around this by checking the supported resolutions (and keep maximum)
			// Also scale the DPI
			const SDL_Rect scaleRect = rect;
			for (int i = 0; i < SDL_GetNumDisplayModes(*id); i++)
			{
				SDL_DisplayMode mode = { 0 };
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

			const float dw = rect.w / scaleRect.w;
			const float dh = rect.h / scaleRect.h;
			hdpi /= dw;
			vdpi /= dh;
		}

#if SDL_VERSION_ATLEAST(2, 0, 10)
		const SDL_DisplayOrientation orientation = SDL_GetDisplayOrientation(*id);
		const UINT32 rdp_orientation = sdl_orientaion_to_rdp(orientation);
#else
		const UINT32 rdp_orientation = ORIENTATION_LANDSCAPE;
#endif

		rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, x);
		WINPR_ASSERT(monitor);
		monitor->orig_screen = x;
		monitor->x = rect.x;
		monitor->y = rect.y;
		monitor->width = rect.w;
		monitor->height = rect.h;
		monitor->is_primary = (rect.x == 0) && (rect.y == 0);
		monitor->attributes.desktopScaleFactor = 100;
		monitor->attributes.deviceScaleFactor = 100;
		monitor->attributes.orientation = rdp_orientation;
		monitor->attributes.physicalWidth = rect.w / hdpi;
		monitor->attributes.physicalHeight = rect.h / vdpi;
	}
	return TRUE;
}

static BOOL sdl_detect_single_window(sdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	rdpSettings* settings = sdl->common.context.settings;
	WINPR_ASSERT(settings);

	if ((!settings->UseMultimon && !settings->SpanMonitors) ||
	    (settings->Workarea && !settings->RemoteApplicationMode))
	{
		/* If no monitors were specified on the command-line then set the current monitor as active
		 */
		if (!settings->NumMonitorIds)
		{
			const size_t id =
			    (sdl->windowCount > 0) ? SDL_GetWindowDisplayIndex(sdl->windows[0].window) : 0;
			if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, &id, 1))
				return FALSE;
		}
		else
		{

			/* Always sets number of monitors from command-line to just 1.
			 * If the monitor is invalid then we will default back to current monitor
			 * later as a fallback. So, there is no need to validate command-line entry here.
			 */
			settings->NumMonitorIds = 1;
		}

		// TODO: Fill monitor struct
		if (!sdl_apply_display_properties(sdl))
			return FALSE;
		return sdl_apply_max_size(sdl, pMaxWidth, pMaxHeight);
	}
	return TRUE;
}

BOOL sdl_detect_monitors(sdlContext* sdl, UINT32* pMaxWidth, UINT32* pMaxHeight)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(pMaxWidth);
	WINPR_ASSERT(pMaxHeight);

	rdpSettings* settings = sdl->common.context.settings;
	WINPR_ASSERT(settings);

	const int numDisplays = SDL_GetNumVideoDisplays();
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, NULL, numDisplays))
		return FALSE;

	for (size_t x = 0; x < numDisplays; x++)
	{
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_MonitorIds, x, &x))
			return FALSE;
	}

	if (!sdl_apply_display_properties(sdl))
		return FALSE;

	return sdl_detect_single_window(sdl, pMaxWidth, pMaxHeight);
}
