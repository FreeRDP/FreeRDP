/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Settings
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include "settings.h"

#include <freerdp/crypto/certificate.h>

#include <ctype.h>

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>
#include <winpr/wtsapi.h>

#include <freerdp/settings.h>
#include <freerdp/build-config.h>

#include "../crypto/certificate.h"
#include "../crypto/privatekey.h"
#include "capabilities.h"

#define TAG FREERDP_TAG("settings")

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

static const char client_dll[] = "C:\\Windows\\System32\\mstscax.dll";

#define SERVER_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\Server"
#define CLIENT_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\Client"
#define BITMAP_CACHE_KEY CLIENT_KEY "\\BitmapCacheV2"
#define GLYPH_CACHE_KEY CLIENT_KEY "\\GlyphCache"
#define POINTER_CACHE_KEY CLIENT_KEY "\\PointerCache"

struct bounds_t
{
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;
};

static const char* bounds2str(const struct bounds_t* bounds, char* buffer, size_t len)
{
	WINPR_ASSERT(bounds);
	(void)_snprintf(buffer, len, "{%dx%d-%dx%d}", bounds->x, bounds->y, bounds->x + bounds->width,
	                bounds->y + bounds->height);
	return buffer;
}

static struct bounds_t union_rect(const struct bounds_t* a, const struct bounds_t* b)
{
	WINPR_ASSERT(a);
	WINPR_ASSERT(b);

	struct bounds_t u = *a;
	if (b->x < u.x)
		u.x = b->x;
	if (b->y < u.y)
		u.y = b->y;

	const INT32 rightA = a->x + a->width;
	const INT32 rightB = b->x + b->width;
	const INT32 right = MAX(rightA, rightB);
	u.width = right - u.x;

	const INT32 bottomA = a->y + a->height;
	const INT32 bottomB = b->y + b->height;
	const INT32 bottom = MAX(bottomA, bottomB);
	u.height = bottom - u.y;

	return u;
}

static BOOL intersect_rects(const struct bounds_t* r1, const struct bounds_t* r2)
{
	WINPR_ASSERT(r1);
	WINPR_ASSERT(r2);

	const INT32 left = MAX(r1->x, r2->x);
	const INT32 top = MAX(r1->y, r2->y);
	const INT32 right = MIN(r1->x + r1->width, r2->x + r2->width);
	const INT32 bottom = MIN(r1->y + r1->height, r2->y + r2->height);

	return (left < right) && (top < bottom);
}

static BOOL align_rects(const struct bounds_t* r1, const struct bounds_t* r2)
{
	WINPR_ASSERT(r1);
	WINPR_ASSERT(r2);

	const INT32 left = MAX(r1->x, r2->x);
	const INT32 top = MAX(r1->y, r2->y);
	const INT32 right = MIN(r1->x + r1->width, r2->x + r2->width);
	const INT32 bottom = MIN(r1->y + r1->height, r2->y + r2->height);

	return (left == right) || (top == bottom);
}

static BOOL settings_reg_query_dword_val(HKEY hKey, const TCHAR* sub, DWORD* value)
{
	DWORD dwType = 0;
	DWORD dwSize = 0;

	dwSize = sizeof(DWORD);
	if (RegQueryValueEx(hKey, sub, NULL, &dwType, (BYTE*)value, &dwSize) != ERROR_SUCCESS)
		return FALSE;
	if (dwType != REG_DWORD)
		return FALSE;

	return TRUE;
}

static BOOL settings_reg_query_word_val(HKEY hKey, const TCHAR* sub, UINT16* value)
{
	DWORD dwValue = 0;

	if (!settings_reg_query_dword_val(hKey, sub, &dwValue))
		return FALSE;

	if (dwValue > UINT16_MAX)
		return FALSE;

	*value = (UINT16)dwValue;
	return TRUE;
}

static BOOL settings_reg_query_bool_val(HKEY hKey, const TCHAR* sub, BOOL* value)
{
	DWORD dwValue = 0;

	if (!settings_reg_query_dword_val(hKey, sub, &dwValue))
		return FALSE;
	*value = dwValue != 0;
	return TRUE;
}

static BOOL settings_reg_query_dword(rdpSettings* settings, FreeRDP_Settings_Keys_UInt32 id,
                                     HKEY hKey, const TCHAR* sub)
{
	DWORD dwValue = 0;

	if (!settings_reg_query_dword_val(hKey, sub, &dwValue))
		return FALSE;

	return freerdp_settings_set_uint32(settings, id, dwValue);
}

static BOOL settings_reg_query_bool(rdpSettings* settings, FreeRDP_Settings_Keys_Bool id, HKEY hKey,
                                    const TCHAR* sub)
{
	DWORD dwValue = 0;

	if (!settings_reg_query_dword_val(hKey, sub, &dwValue))
		return FALSE;

	return freerdp_settings_set_bool(settings, id, dwValue != 0 ? TRUE : FALSE);
}

static void settings_client_load_hkey_local_machine(rdpSettings* settings)
{
	HKEY hKey = NULL;
	LONG status = 0;
	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, CLIENT_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		settings_reg_query_dword(settings, FreeRDP_DesktopWidth, hKey, _T("DesktopWidth"));
		settings_reg_query_dword(settings, FreeRDP_DesktopHeight, hKey, _T("DesktopHeight"));
		settings_reg_query_bool(settings, FreeRDP_Fullscreen, hKey, _T("Fullscreen"));
		settings_reg_query_dword(settings, FreeRDP_ColorDepth, hKey, _T("ColorDepth"));
		settings_reg_query_dword(settings, FreeRDP_KeyboardType, hKey, _T("KeyboardType"));
		settings_reg_query_dword(settings, FreeRDP_KeyboardSubType, hKey, _T("KeyboardSubType"));
		settings_reg_query_dword(settings, FreeRDP_KeyboardFunctionKey, hKey,
		                         _T("KeyboardFunctionKeys"));
		settings_reg_query_dword(settings, FreeRDP_KeyboardLayout, hKey, _T("KeyboardLayout"));
		settings_reg_query_bool(settings, FreeRDP_ExtSecurity, hKey, _T("ExtSecurity"));
		settings_reg_query_bool(settings, FreeRDP_NlaSecurity, hKey, _T("NlaSecurity"));
		settings_reg_query_bool(settings, FreeRDP_TlsSecurity, hKey, _T("TlsSecurity"));
		settings_reg_query_bool(settings, FreeRDP_RdpSecurity, hKey, _T("RdpSecurity"));
		settings_reg_query_bool(settings, FreeRDP_MstscCookieMode, hKey, _T("MstscCookieMode"));
		settings_reg_query_dword(settings, FreeRDP_CookieMaxLength, hKey, _T("CookieMaxLength"));
		settings_reg_query_bool(settings, FreeRDP_BitmapCacheEnabled, hKey, _T("BitmapCache"));
		settings_reg_query_dword(settings, FreeRDP_OffscreenSupportLevel, hKey,
		                         _T("OffscreenBitmapCache"));
		settings_reg_query_dword(settings, FreeRDP_OffscreenCacheSize, hKey,
		                         _T("OffscreenBitmapCacheSize"));
		settings_reg_query_dword(settings, FreeRDP_OffscreenCacheEntries, hKey,
		                         _T("OffscreenBitmapCacheEntries"));
		RegCloseKey(hKey);
	}

	status =
	    RegOpenKeyExA(HKEY_LOCAL_MACHINE, BITMAP_CACHE_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		settings_reg_query_dword(settings, FreeRDP_BitmapCacheV2NumCells, hKey, _T("NumCells"));
		for (unsigned x = 0; x < 5; x++)
		{
			DWORD val = 0;
			TCHAR numentries[64] = { 0 };
			TCHAR persist[64] = { 0 };
			BITMAP_CACHE_V2_CELL_INFO cache = { 0 };
			(void)_sntprintf(numentries, ARRAYSIZE(numentries), _T("Cell%uNumEntries"), x);
			(void)_sntprintf(persist, ARRAYSIZE(persist), _T("Cell%uPersistent"), x);
			if (!settings_reg_query_dword_val(hKey, numentries, &val) ||
			    !settings_reg_query_bool_val(hKey, persist, &cache.persistent) ||
			    !freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, x,
			                                        &cache))
				WLog_WARN(TAG, "Failed to load registry keys to settings!");
			cache.numEntries = val;
		}

		settings_reg_query_bool(settings, FreeRDP_AllowCacheWaitingList, hKey,
		                        _T("AllowCacheWaitingList"));
		RegCloseKey(hKey);
	}

	status =
	    RegOpenKeyExA(HKEY_LOCAL_MACHINE, GLYPH_CACHE_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		unsigned x = 0;
		UINT32 GlyphSupportLevel = 0;
		settings_reg_query_dword(settings, FreeRDP_GlyphSupportLevel, hKey, _T("SupportLevel"));
		for (; x < 10; x++)
		{
			GLYPH_CACHE_DEFINITION cache = { 0 };
			TCHAR numentries[64] = { 0 };
			TCHAR maxsize[64] = { 0 };
			(void)_sntprintf(numentries, ARRAYSIZE(numentries), _T("Cache%uNumEntries"), x);
			(void)_sntprintf(maxsize, ARRAYSIZE(maxsize), _T("Cache%uMaxCellSize"), x);

			settings_reg_query_word_val(hKey, numentries, &cache.cacheEntries);
			settings_reg_query_word_val(hKey, maxsize, &cache.cacheMaximumCellSize);
			if (!freerdp_settings_set_pointer_array(settings, FreeRDP_GlyphCache, x, &cache))
				WLog_WARN(TAG, "Failed to store GlyphCache %" PRIuz, x);
		}
		{
			GLYPH_CACHE_DEFINITION cache = { 0 };
			settings_reg_query_word_val(hKey, _T("FragCacheNumEntries"), &cache.cacheEntries);
			settings_reg_query_word_val(hKey, _T("FragCacheMaxCellSize"),
			                            &cache.cacheMaximumCellSize);
			if (!freerdp_settings_set_pointer_array(settings, FreeRDP_FragCache, x, &cache))
				WLog_WARN(TAG, "Failed to store FragCache");
		}

		RegCloseKey(hKey);

		if (!freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel, GlyphSupportLevel))
			WLog_WARN(TAG, "Failed to load registry keys to settings!");
	}

	status =
	    RegOpenKeyExA(HKEY_LOCAL_MACHINE, POINTER_CACHE_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		settings_reg_query_dword(settings, FreeRDP_LargePointerFlag, hKey, _T("LargePointer"));
		settings_reg_query_dword(settings, FreeRDP_PointerCacheSize, hKey, _T("PointerCacheSize"));
		settings_reg_query_dword(settings, FreeRDP_ColorPointerCacheSize, hKey,
		                         _T("ColorPointerCacheSize"));
		RegCloseKey(hKey);
	}
}

static void settings_server_load_hkey_local_machine(rdpSettings* settings)
{
	HKEY hKey = NULL;
	LONG status = 0;

	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, SERVER_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return;

	settings_reg_query_bool(settings, FreeRDP_ExtSecurity, hKey, _T("ExtSecurity"));
	settings_reg_query_bool(settings, FreeRDP_NlaSecurity, hKey, _T("NlaSecurity"));
	settings_reg_query_bool(settings, FreeRDP_TlsSecurity, hKey, _T("TlsSecurity"));
	settings_reg_query_dword(settings, FreeRDP_TlsSecLevel, hKey, _T("TlsSecLevel"));
	settings_reg_query_bool(settings, FreeRDP_RdpSecurity, hKey, _T("RdpSecurity"));

	RegCloseKey(hKey);
}

static void settings_load_hkey_local_machine(rdpSettings* settings)
{
	if (freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
		settings_server_load_hkey_local_machine(settings);
	else
		settings_client_load_hkey_local_machine(settings);
}

static BOOL settings_get_computer_name(rdpSettings* settings)
{
	CHAR computerName[256] = { 0 };
	DWORD nSize = sizeof(computerName);

	if (!GetComputerNameExA(ComputerNameNetBIOS, computerName, &nSize))
		return FALSE;

	if (nSize > MAX_COMPUTERNAME_LENGTH)
		computerName[MAX_COMPUTERNAME_LENGTH] = '\0';

	return freerdp_settings_set_string(settings, FreeRDP_ComputerName, computerName);
}

void freerdp_settings_print_warnings(const rdpSettings* settings)
{
	const UINT32 level = freerdp_settings_get_uint32(settings, FreeRDP_GlyphSupportLevel);
	if (level != GLYPH_SUPPORT_NONE)
	{
		char buffer[32] = { 0 };
		WLog_WARN(TAG, "[experimental] enabled GlyphSupportLevel %s, expect visual artefacts!",
		          freerdp_settings_glyph_level_string(level, buffer, sizeof(buffer)));
	}
}

static BOOL monitor_operlaps(const rdpSettings* settings, UINT32 orig, UINT32 start, UINT32 count,
                             const rdpMonitor* compare)
{
	const struct bounds_t rect1 = {
		.x = compare->x, .y = compare->y, .width = compare->width, .height = compare->height
	};
	for (UINT32 x = start; x < count; x++)
	{
		const rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x);
		const struct bounds_t rect2 = {
			.x = monitor->x, .y = monitor->y, .width = monitor->width, .height = monitor->height
		};

		if (intersect_rects(&rect1, &rect2))
		{
			char buffer1[32] = { 0 };
			char buffer2[32] = { 0 };

			WLog_ERR(TAG, "Monitor %" PRIu32 " and %" PRIu32 " are overlapping:", orig, x);
			WLog_ERR(TAG, "%s overlapps with %s", bounds2str(&rect1, buffer1, sizeof(buffer1)),
			         bounds2str(&rect2, buffer2, sizeof(buffer2)));
			WLog_ERR(
			    TAG,
			    "Mulitimonitor mode requested, but local layout has gaps or overlapping areas!");
			WLog_ERR(TAG,
			         "Please reconfigure your local monitor setup so that there are no gaps or "
			         "overlapping areas!");
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL monitor_has_gaps(const rdpSettings* settings, UINT32 start, UINT32 count,
                             const rdpMonitor* compare, UINT32** graph)
{
	const struct bounds_t rect1 = {
		.x = compare->x, .y = compare->y, .width = compare->width, .height = compare->height
	};

	BOOL hasNeighbor = FALSE;
	for (UINT32 i = 0; i < count; i++)
	{
		if (i == start)
			continue;

		const rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, i);

		const struct bounds_t rect2 = {
			.x = monitor->x, .y = monitor->y, .width = monitor->width, .height = monitor->height
		};
		if (align_rects(&rect1, &rect2))
		{
			hasNeighbor = TRUE;
			graph[start][i] = 1;
			graph[i][start] = 1;
		}
	}

	if (!hasNeighbor)
	{
		WLog_ERR(TAG,
		         "Monitor configuration has gaps! Monitor %" PRIu32 " does not have any neighbor",
		         start);
	}

	return !hasNeighbor;
}

static void alloc_free(UINT32** ptr)
{
	free((void*)ptr);
}

WINPR_ATTR_MALLOC(alloc_free, 1)
static UINT32** alloc_array(size_t count)
{
	// NOLINTNEXTLINE(clang-analyzer-unix.MallocSizeof)
	BYTE* array = calloc(count * sizeof(uintptr_t), count * sizeof(UINT32));
	UINT32** dst = (UINT32**)array;
	UINT32* val = (UINT32*)(array + count * sizeof(uintptr_t));
	for (size_t x = 0; x < count; x++)
		dst[x] = &val[x];

	return dst;
}

/* Monitors in the array need to:
 *
 * 1. be connected to another monitor (edges touch but don't overlap or have gaps)
 * 2. all monitors need to be connected so there are no separate groups.
 *
 * So, what we do here is we use dijkstra algorithm to find a path from any start node
 * (lazy as we are we always use the first in the array) to each other node.
 */
static BOOL find_path_exists_with_dijkstra(UINT32** graph, size_t count, UINT32 start)
{
	if (count < 1)
		return FALSE;

	WINPR_ASSERT(start < count);

	UINT32** cost = alloc_array(count);
	WINPR_ASSERT(cost);

	UINT32* distance = calloc(count, sizeof(UINT32));
	WINPR_ASSERT(distance);

	UINT32* visited = calloc(count, sizeof(UINT32));
	WINPR_ASSERT(visited);

	UINT32* parent = calloc(count, sizeof(UINT32));
	WINPR_ASSERT(parent);

	for (size_t x = 0; x < count; x++)
	{
		for (size_t y = 0; y < count; y++)
		{
			if (graph[x][y] == 0)
				cost[x][y] = UINT32_MAX;
			else
				cost[x][y] = graph[x][y];
		}
	}

	for (UINT32 x = 0; x < count; x++)
	{
		distance[x] = cost[start][x];
		parent[x] = start;
		visited[x] = 0;
	}

	distance[start] = 0;
	visited[start] = 1;

	size_t pos = 1;
	UINT32 nextnode = UINT32_MAX;
	while (pos < count - 1)
	{
		UINT32 mindistance = UINT32_MAX;

		for (UINT32 x = 0; x < count; x++)
		{
			if ((distance[x] < mindistance) && !visited[x])
			{
				mindistance = distance[x];
				nextnode = x;
			}
		}

		visited[nextnode] = 1;

		for (size_t y = 0; y < count; y++)
		{
			if (!visited[y])
			{
				UINT32 dist = mindistance + cost[nextnode][y];
				if (dist < distance[y])
				{
					distance[y] = dist;
					parent[y] = nextnode;
				}
			}
		}
		pos++;
	}

	BOOL rc = TRUE;
	for (size_t x = 0; x < count; x++)
	{
		if (x != start)
		{
			if (distance[x] == UINT32_MAX)
			{
				WLog_ERR(TAG, "monitor %" PRIu32 " not connected with monitor %" PRIuz, start, x);
				rc = FALSE;
				break;
			}
		}
	}
	alloc_free(cost);
	free(distance);
	free(visited);
	free(parent);

	return rc;
}

static BOOL freerdp_settings_client_monitors_have_gaps(const rdpSettings* settings)
{
	BOOL rc = TRUE;
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	if (count <= 1)
		return FALSE;

	UINT32** graph = alloc_array(count);
	WINPR_ASSERT(graph);

	for (UINT32 x = 0; x < count; x++)
	{
		const rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x);
		if (monitor_has_gaps(settings, x, count, monitor, graph))
			goto fail;
	}

	rc = !find_path_exists_with_dijkstra(graph, count, 0);

fail:
	alloc_free(graph);

	return rc;
}

static void log_monitor(UINT32 idx, const rdpMonitor* monitor, wLog* log, DWORD level)
{
	WINPR_ASSERT(monitor);

	WLog_Print(log, level,
	           "[%" PRIu32 "] [%s] {%dx%d-%dx%d} [%" PRIu32 "] {%" PRIu32 "x%" PRIu32
	           ", orientation: %" PRIu32 ", desktopScale: %" PRIu32 ", deviceScale: %" PRIu32 "}",
	           idx, monitor->is_primary ? "primary" : "       ", monitor->x, monitor->y,
	           monitor->width, monitor->height, monitor->orig_screen,
	           monitor->attributes.physicalWidth, monitor->attributes.physicalHeight,
	           monitor->attributes.orientation, monitor->attributes.desktopScaleFactor,
	           monitor->attributes.deviceScaleFactor);
}

static void log_monitor_configuration(const rdpSettings* settings, wLog* log, DWORD level)
{
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	WLog_Print(log, level, "[BEGIN] MonitorDefArray[%" PRIu32 "]", count);
	for (UINT32 x = 0; x < count; x++)
	{
		const rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x);
		log_monitor(x, monitor, log, level);
	}
	WLog_Print(log, level, "[END] MonitorDefArray[%" PRIu32 "]", count);
}

static BOOL freerdp_settings_client_monitors_overlap(const rdpSettings* settings)
{
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	for (UINT32 x = 0; x < count; x++)
	{
		const rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x);
		if (monitor_operlaps(settings, x, x + 1, count, monitor))
			return TRUE;
	}
	return FALSE;
}

/* See [MS-RDPBCGR] 2.2.1.3.6.1 for details on limits
 * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpbcgr/c3964b39-3d54-4ae1-a84a-ceaed311e0f6
 */
static BOOL freerdp_settings_client_monitors_check_primary_and_origin(const rdpSettings* settings)
{
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	BOOL havePrimary = FALSE;
	BOOL foundOrigin = FALSE;
	BOOL rc = TRUE;

	struct bounds_t bounds = { 0 };

	if (count == 0)
	{
		WLog_WARN(TAG, "Monitor configuration empty.");
		return TRUE;
	}

	for (UINT32 x = 0; x < count; x++)
	{
		const rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorDefArray, x);
		struct bounds_t cur = {
			.x = monitor->x, .y = monitor->y, .width = monitor->width, .height = monitor->height
		};

		bounds = union_rect(&bounds, &cur);

		if (monitor->is_primary)
		{
			if (havePrimary)
			{
				WLog_ERR(TAG, "Monitor configuration contains multiple primary monitors!");
				rc = FALSE;
			}
			havePrimary = TRUE;
		}

		if ((monitor->x == 0) && (monitor->y == 0))
		{
			if (foundOrigin)
			{
				WLog_ERR(TAG, "Monitor configuration does have multiple origin 0/0");
				rc = FALSE;
			}
			foundOrigin = TRUE;
		}
	}

	if ((bounds.width > 32766) || (bounds.width < 200))
	{
		WLog_ERR(TAG,
		         "Monitor configuration virtual desktop width must be 200 <= %" PRId32 " <= 32766",
		         bounds.width);
		rc = FALSE;
	}
	if ((bounds.height > 32766) || (bounds.height < 200))
	{
		WLog_ERR(TAG,
		         "Monitor configuration virtual desktop height must be 200 <= %" PRId32 " <= 32766",
		         bounds.height);
		rc = FALSE;
	}

	if (!havePrimary)
	{
		WLog_ERR(TAG, "Monitor configuration does not contain a primary monitor!");
		rc = FALSE;
	}
	if (!foundOrigin)
	{
		WLog_ERR(TAG, "Monitor configuration must start at 0/0 for first monitor!");
		rc = FALSE;
	}

	return rc;
}

BOOL freerdp_settings_check_client_after_preconnect(const rdpSettings* settings)
{
	wLog* log = WLog_Get(TAG);
	BOOL rc = TRUE;
	log_monitor_configuration(settings, log, WLOG_DEBUG);
	if (freerdp_settings_client_monitors_overlap(settings))
		rc = FALSE;
	if (freerdp_settings_client_monitors_have_gaps(settings))
		rc = FALSE;
	if (!freerdp_settings_client_monitors_check_primary_and_origin(settings))
		rc = FALSE;
	if (!rc)
	{
		DWORD level = WLOG_ERROR;
		WLog_Print(log, level, "Invalid or unsupported monitor configuration detected");
		WLog_Print(log, level, "Check if the configuration is valid.");
		WLog_Print(log, level,
		           "If you suspect a bug create a new issue at "
		           "https://github.com/FreeRDP/FreeRDP/issues/new");
		WLog_Print(
		    log, level,
		    "Provide at least the following log lines detailing your monitor configuration:");
		log_monitor_configuration(settings, log, level);
	}

	return rc;
}

BOOL freerdp_settings_set_default_order_support(rdpSettings* settings)
{
	BYTE* OrderSupport = freerdp_settings_get_pointer_writable(settings, FreeRDP_OrderSupport);
	if (!OrderSupport)
		return FALSE;

	ZeroMemory(OrderSupport, 32);
	OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	OrderSupport[NEG_LINETO_INDEX] = TRUE;
	OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	OrderSupport[NEG_MEMBLT_INDEX] =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled) ? 1 : 0;
	OrderSupport[NEG_MEM3BLT_INDEX] =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled) ? 1 : 0;
	OrderSupport[NEG_MEMBLT_V2_INDEX] =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled) ? 1 : 0;
	OrderSupport[NEG_MEM3BLT_V2_INDEX] =
	    freerdp_settings_get_bool(settings, FreeRDP_BitmapCacheEnabled) ? 1 : 0;
	OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	OrderSupport[NEG_GLYPH_INDEX_INDEX] =
	    freerdp_settings_get_uint32(settings, FreeRDP_GlyphSupportLevel) != GLYPH_SUPPORT_NONE;
	OrderSupport[NEG_FAST_INDEX_INDEX] =
	    freerdp_settings_get_uint32(settings, FreeRDP_GlyphSupportLevel) != GLYPH_SUPPORT_NONE;
	OrderSupport[NEG_FAST_GLYPH_INDEX] =
	    freerdp_settings_get_uint32(settings, FreeRDP_GlyphSupportLevel) != GLYPH_SUPPORT_NONE;
	OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;
	return TRUE;
}

BOOL freerdp_capability_buffer_allocate(rdpSettings* settings, UINT32 count)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(count > 0);
	WINPR_ASSERT(count == 32);

	freerdp_capability_buffer_free(settings);
	WINPR_ASSERT(settings->ReceivedCapabilitiesSize == 0);

	settings->ReceivedCapabilitiesSize = count;
	void* tmp = realloc(settings->ReceivedCapabilities, count * sizeof(BYTE));
	if (!tmp)
		return FALSE;
	memset(tmp, 0, count * sizeof(BYTE));
	settings->ReceivedCapabilities = tmp;

	tmp = realloc((void*)settings->ReceivedCapabilityData, count * sizeof(BYTE*));
	if (!tmp)
		return FALSE;
	memset(tmp, 0, count * sizeof(BYTE*));
	settings->ReceivedCapabilityData = (BYTE**)tmp;

	tmp = realloc(settings->ReceivedCapabilityDataSizes, count * sizeof(UINT32));
	if (!tmp)
		return FALSE;
	memset(tmp, 0, count * sizeof(UINT32));
	settings->ReceivedCapabilityDataSizes = tmp;

	return (settings->ReceivedCapabilities && settings->ReceivedCapabilityData &&
	        settings->ReceivedCapabilityDataSizes);
}

#if !defined(WITH_FULL_CONFIG_PATH)
static char* freerdp_settings_get_legacy_config_path(void)
{
	char product[sizeof(FREERDP_PRODUCT_STRING)] = { 0 };

	for (size_t i = 0; i < sizeof(product); i++)
		product[i] = (char)tolower(FREERDP_PRODUCT_STRING[i]);

	return GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, product);
}
#endif

char* freerdp_settings_get_config_path(void)
{
	char* path = NULL;
	/* For default FreeRDP continue using same config directory
	 * as in old releases.
	 * Custom builds use <Vendor>/<Product> as config folder. */
#if !defined(WITH_FULL_CONFIG_PATH)
	if (_stricmp(FREERDP_VENDOR_STRING, FREERDP_PRODUCT_STRING) == 0)
		return freerdp_settings_get_legacy_config_path();
#endif

	char* base = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, FREERDP_VENDOR_STRING);
	if (base)
		path = GetCombinedPath(base, FREERDP_PRODUCT_STRING);
	free(base);

	return path;
}

rdpSettings* freerdp_settings_new(DWORD flags)
{
	char* issuers[] = { "FreeRDP", "FreeRDP-licenser" };
	const BOOL server = (flags & FREERDP_SETTINGS_SERVER_MODE) != 0 ? TRUE : FALSE;
	const BOOL remote = (flags & FREERDP_SETTINGS_REMOTE_MODE) != 0 ? TRUE : FALSE;
	rdpSettings* settings = (rdpSettings*)calloc(1, sizeof(rdpSettings));

	if (!settings)
		return NULL;

	if (!server && !remote)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPhysicalWidth, 1000))
			goto out_fail;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopPhysicalHeight, 1000))
			goto out_fail;
		if (!freerdp_settings_set_uint16(settings, FreeRDP_DesktopOrientation,
		                                 ORIENTATION_LANDSCAPE))
			goto out_fail;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DeviceScaleFactor, 100))
			goto out_fail;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopScaleFactor, 100))
			goto out_fail;
	}
	if (!freerdp_settings_set_uint32(settings, FreeRDP_SurfaceCommandsSupported,
	                                 SURFCMDS_SET_SURFACE_BITS | SURFCMDS_STREAM_SURFACE_BITS |
	                                     SURFCMDS_FRAME_MARKER))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxRlgrMode, RLGR3))
		goto out_fail;

	if (!freerdp_settings_set_uint16(settings, FreeRDP_CapsProtocolVersion,
	                                 TS_CAPS_PROTOCOLVERSION))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_ClipboardFeatureMask,
	                                 CLIPRDR_FLAG_DEFAULT_MASK))
		goto out_fail;
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerLicenseCompanyName, "FreeRDP"))
		goto out_fail;
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerLicenseProductName,
	                                 "FreeRDP-licensing-server"))
		goto out_fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerLicenseProductVersion, 1))
		goto out_fail;
	if (!freerdp_server_license_issuers_copy(settings, issuers, ARRAYSIZE(issuers)))
		goto out_fail;

	if (!freerdp_settings_set_uint16(settings, FreeRDP_SupportedColorDepths,
	                                 RNS_UD_32BPP_SUPPORT | RNS_UD_24BPP_SUPPORT |
	                                     RNS_UD_16BPP_SUPPORT | RNS_UD_15BPP_SUPPORT))
		goto out_fail;

	if (!freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_HasHorizontalWheel, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_HasExtendedMouseEvent, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_HasQoeEvent, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_HasRelativeMouseEvent, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_HiDefRemoteApp, TRUE) ||
	    !freerdp_settings_set_uint32(
	        settings, FreeRDP_RemoteApplicationSupportMask,
	        RAIL_LEVEL_SUPPORTED | RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED |
	            RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED | RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED |
	            RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED |
	            RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED | RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED |
	            RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED) ||
	    !freerdp_settings_set_uint16(settings, FreeRDP_TextANSICodePage, CP_UTF8) ||
	    !freerdp_settings_set_uint16(settings, FreeRDP_OrderSupportFlags,
	                                 NEGOTIATE_ORDER_SUPPORT | ZERO_BOUNDS_DELTA_SUPPORT |
	                                     COLOR_INDEX_SUPPORT) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SupportHeartbeatPdu, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_ServerMode, server) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_WaitForOutputBufferFlush, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ClusterInfoFlags, REDIRECTION_SUPPORTED) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 1024) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 768) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_Workarea, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_Fullscreen, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GrabKeyboard, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_Decorations, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_RdpVersion, RDP_VERSION_10_12) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AadSecurity, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_ExtSecurity, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_RdstlsSecurity, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NegotiateSecurityLayer, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_RestrictedAdminModeRequired, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_MstscCookieMode, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_CookieMaxLength,
	                                 DEFAULT_COOKIE_MAX_LENGTH) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ClientBuild,
	                                 18363) || /* Windows 10, Version 1909 */
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardType, 4) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardSubType, 0) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardFunctionKey, 12) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardLayout, 0) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeyboardHook,
	                                 KEYBOARD_HOOK_FULLSCREEN_ONLY) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SaltedChecksum, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, 3389) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_GatewayPort, 443) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DesktopResize, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_ToggleFullscreen, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosX, UINT32_MAX) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DesktopPosY, UINT32_MAX) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SoftwareGdi, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_UnmapButtons, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_PerformanceFlags, PERF_FLAG_NONE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AllowFontSmoothing, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AllowDesktopComposition, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableWallpaper, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableFullWindowDrag, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableMenuAnims, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableThemes, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ConnectionType,
	                                 CONNECTION_TYPE_AUTODETECT) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NetworkAutoDetect, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_EncryptionMethods, ENCRYPTION_METHOD_NONE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel, ENCRYPTION_LEVEL_NONE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_FIPSMode, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_CompressionEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_LogonNotify, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_BrushSupportLevel, BRUSH_COLOR_FULL) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, PACKET_COMPR_TYPE_RDP61) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_Authentication, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AuthenticationOnly, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_CredentialsFromStdin, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DisableCredentialsDelegation, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_AuthenticationLevel, 2) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ChannelCount, 0) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_CertificateCallbackPreferPEM, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_KeySpec, AT_KEYEXCHANGE))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ChannelDefArray, NULL,
	                                      CHANNEL_MAX_COUNT))
		goto out_fail;

	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportMonitorLayoutPdu, FALSE))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorCount, 0))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorDefArray, NULL, 32))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorLocalShiftX, 0))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorLocalShiftY, 0))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, NULL, 0))
		goto out_fail;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_MultitransportFlags,
	                                 TRANSPORT_TYPE_UDP_FECR))
		goto out_fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportMultitransport, TRUE))
		goto out_fail;

	if (!settings_get_computer_name(settings))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerCertificate, NULL, 1))
		goto out_fail;

	if (!freerdp_capability_buffer_allocate(settings, 32))
		goto out_fail;

	{
		char tmp[32] = { 0 };
		if (!freerdp_settings_set_string_len(settings, FreeRDP_ClientProductId, tmp, sizeof(tmp)))
			goto out_fail;
	}

	{
		char ClientHostname[33] = { 0 };
		DWORD size = sizeof(ClientHostname) - 2;
		GetComputerNameA(ClientHostname, &size);
		if (!freerdp_settings_set_string(settings, FreeRDP_ClientHostname, ClientHostname))
			goto out_fail;
	}

	/* [MS-RDPBCGR] 2.2.7.1.5 Pointer Capability Set (TS_POINTER_CAPABILITYSET)
	 *
	 * if we are in server mode send a reasonable large cache size,
	 * if we are in client mode just set the value to the maximum we want to
	 * support and during capability exchange that size will be limited to the
	 * sizes the server supports
	 *
	 * We have chosen 128 cursors in cache which is at worst 128 * 576kB (384x384 pixel cursor with
	 * 32bit color depth)
	 * */
	if (freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_PointerCacheSize, 25) ||
		    !freerdp_settings_set_uint32(settings, FreeRDP_ColorPointerCacheSize, 25))
			goto out_fail;
	}
	else
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_PointerCacheSize, 128) ||
		    !freerdp_settings_set_uint32(settings, FreeRDP_ColorPointerCacheSize, 128))
			goto out_fail;
	}

	if (!freerdp_settings_set_uint32(settings, FreeRDP_LargePointerFlag,
	                                 (LARGE_POINTER_FLAG_96x96 | LARGE_POINTER_FLAG_384x384)) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SoundBeepsEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DrawGdiPlusEnabled, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DrawAllowSkipAlpha, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DrawAllowColorSubsampling, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DrawAllowDynamicColorFidelity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_FrameMarkerCommandEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SurfaceFrameMarkerEnabled, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AllowCacheWaitingList, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_BitmapCacheV2NumCells, 5))
		goto out_fail;
	settings->BitmapCacheV2CellInfo =
	    (BITMAP_CACHE_V2_CELL_INFO*)calloc(6, sizeof(BITMAP_CACHE_V2_CELL_INFO));

	if (!settings->BitmapCacheV2CellInfo)
		goto out_fail;

	{
		BITMAP_CACHE_V2_CELL_INFO cache = { 0 };
		cache.numEntries = 600;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 0,
		                                        &cache) ||
		    !freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 1, &cache))
			goto out_fail;
		cache.numEntries = 2048;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 2,
		                                        &cache) ||
		    !freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 4, &cache))
			goto out_fail;
		cache.numEntries = 4096;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_BitmapCacheV2CellInfo, 3, &cache))
			goto out_fail;
	}
	if (!freerdp_settings_set_bool(settings, FreeRDP_NoBitmapCompressionHeader, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_RefreshRect, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SuppressOutput, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_GlyphSupportLevel, GLYPH_SUPPORT_NONE))
		goto out_fail;
	settings->GlyphCache = calloc(10, sizeof(GLYPH_CACHE_DEFINITION));

	if (!settings->GlyphCache)
		goto out_fail;

	settings->FragCache = calloc(1, sizeof(GLYPH_CACHE_DEFINITION));

	if (!settings->FragCache)
		goto out_fail;

	for (size_t x = 0; x < 10; x++)
	{
		GLYPH_CACHE_DEFINITION cache = { 0 };
		cache.cacheEntries = 254;
		switch (x)
		{
			case 0:
			case 1:
				cache.cacheMaximumCellSize = 4;
				break;
			case 2:
			case 3:
				cache.cacheMaximumCellSize = 8;
				break;
			case 4:
				cache.cacheMaximumCellSize = 16;
				break;
			case 5:
				cache.cacheMaximumCellSize = 32;
				break;
			case 6:
				cache.cacheMaximumCellSize = 64;
				break;
			case 7:
				cache.cacheMaximumCellSize = 128;
				break;
			case 8:
				cache.cacheMaximumCellSize = 256;
				break;
			case 9:
				cache.cacheMaximumCellSize = 256;
				break;
			default:
				goto out_fail;
		}

		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_GlyphCache, x, &cache))
			goto out_fail;
	}
	{
		GLYPH_CACHE_DEFINITION cache = { 0 };
		cache.cacheEntries = 256;
		cache.cacheMaximumCellSize = 256;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_FragCache, 0, &cache))
			goto out_fail;
	}
	if (!freerdp_settings_set_uint32(settings, FreeRDP_OffscreenSupportLevel, 0) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_OffscreenCacheSize, 7680) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_OffscreenCacheEntries, 2000) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DrawNineGridCacheSize, 2560) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_DrawNineGridCacheEntries, 256) ||
	    !freerdp_settings_set_string(settings, FreeRDP_ClientDir, client_dll) ||
	    !freerdp_settings_get_string(settings, FreeRDP_ClientDir) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_RemoteWndSupportLevel,
	                                 WINDOW_LEVEL_SUPPORTED | WINDOW_LEVEL_SUPPORTED_EX) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_RemoteAppNumIconCaches, 3) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_RemoteAppNumIconCacheEntries, 12) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_VCChunkSize,
	                                 (server && !remote) ? CHANNEL_CHUNK_MAX_LENGTH
	                                                     : CHANNEL_CHUNK_LENGTH) ||
	    /* [MS-RDPBCGR] 2.2.7.2.7 Large Pointer Capability Set (TS_LARGE_POINTER_CAPABILITYSET)
	       requires at least this size */
	    !freerdp_settings_set_uint32(settings, FreeRDP_MultifragMaxRequestSize,
	                                 server ? 0 : 608299) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayUseSameCredentials, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayBypassLocal, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayRpcTransport, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpTransport, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayUdpTransport, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpUseWebsockets, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayHttpExtAuthSspiNtlm, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayArmTransport, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_FastPathInput, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_FastPathOutput, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_LongCredentialsSupported, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_FrameAcknowledge, 2) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_MouseMotion, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_NSCodecColorLossLevel, 3) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NSCodecAllowSubsampling, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_NSCodecAllowDynamicColorFidelity, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled, FALSE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_AutoReconnectMaxRetries, 20) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxThinClient, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxSmallCache, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxProgressiveV2, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxPlanar, TRUE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxH264, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, FALSE) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GfxSendQoeAck, FALSE))
		goto out_fail;
	{
		ARC_CS_PRIVATE_PACKET cookie = { 0 };
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ClientAutoReconnectCookie, &cookie,
		                                      1))
			goto out_fail;
	}
	{
		ARC_SC_PRIVATE_PACKET cookie = { 0 };
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerAutoReconnectCookie, &cookie,
		                                      1))
			goto out_fail;
	}

	settings->ClientTimeZone = (LPTIME_ZONE_INFORMATION)calloc(1, sizeof(TIME_ZONE_INFORMATION));

	if (!settings->ClientTimeZone)
		goto out_fail;

	if (!settings->ServerMode)
	{
		DYNAMIC_TIME_ZONE_INFORMATION dynamic = { 0 };
		TIME_ZONE_INFORMATION* tz =
		    freerdp_settings_get_pointer_writable(settings, FreeRDP_ClientTimeZone);
		WINPR_ASSERT(tz);

		GetTimeZoneInformation(tz);
		GetDynamicTimeZoneInformation(&dynamic);

		if (!freerdp_settings_set_string_from_utf16N(settings, FreeRDP_DynamicDSTTimeZoneKeyName,
		                                             dynamic.TimeZoneKeyName,
		                                             ARRAYSIZE(dynamic.TimeZoneKeyName)))
			goto out_fail;

		if (!freerdp_settings_set_bool(settings, FreeRDP_DynamicDaylightTimeDisabled,
		                               dynamic.DynamicDaylightTimeDisabled))
			goto out_fail;
	}

	if (!freerdp_settings_set_bool(settings, FreeRDP_TcpKeepAlive, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpKeepAliveRetries, 3) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpKeepAliveDelay, 5) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpKeepAliveInterval, 2) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpAckTimeout, 9000) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_TcpConnectTimeout, 15000))
		goto out_fail;

	if (!freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
	{
		BOOL rc = FALSE;
		char* path = NULL;
		if (!freerdp_settings_set_bool(settings, FreeRDP_RedirectClipboard, TRUE))
			goto out_fail;
		/* these values are used only by the client part */
		path = GetKnownPath(KNOWN_PATH_HOME);
		rc = freerdp_settings_set_string(settings, FreeRDP_HomePath, path);
		free(path);

		if (!rc || !freerdp_settings_get_string(settings, FreeRDP_HomePath))
			goto out_fail;

		char* config = freerdp_settings_get_config_path();
		rc = freerdp_settings_set_string(settings, FreeRDP_ConfigPath, config);
		if (rc)
		{
			char* action = GetCombinedPath(config, "action.sh");
			rc = freerdp_settings_set_string(settings, FreeRDP_ActionScript, action);
			free(action);
		}

		free(config);
		if (!rc)
			goto out_fail;
	}

	settings_load_hkey_local_machine(settings);

	if (!freerdp_settings_set_bool(settings, FreeRDP_SmartcardLogon, FALSE))
		goto out_fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_TlsSecLevel, 1))
		goto out_fail;
	settings->OrderSupport = calloc(1, 32);

	if (!freerdp_settings_set_uint16(settings, FreeRDP_TLSMinVersion, TLS1_VERSION))
		goto out_fail;
	if (!freerdp_settings_set_uint16(settings, FreeRDP_TLSMaxVersion, 0))
		goto out_fail;

	if (!settings->OrderSupport)
		goto out_fail;

	if (!freerdp_settings_set_default_order_support(settings))
		goto out_fail;

	const BOOL enable = freerdp_settings_get_bool(settings, FreeRDP_ServerMode);

	{
		const FreeRDP_Settings_Keys_Bool keys[] = { FreeRDP_SupportGraphicsPipeline,
			                                        FreeRDP_SupportStatusInfoPdu,
			                                        FreeRDP_SupportErrorInfoPdu,
			                                        FreeRDP_SupportAsymetricKeys };

		for (size_t x = 0; x < ARRAYSIZE(keys); x++)
		{
			if (!freerdp_settings_set_bool(settings, keys[x], enable))
				goto out_fail;
		}
	}

	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportDynamicTimeZone, TRUE))
		goto out_fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_SupportSkipChannelJoin, TRUE))
		goto out_fail;

	return settings;
out_fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	freerdp_settings_free(settings);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

static void freerdp_settings_free_internal(rdpSettings* settings)
{
	freerdp_server_license_issuers_free(settings);
	freerdp_target_net_addresses_free(settings);
	freerdp_device_collection_free(settings);
	freerdp_static_channel_collection_free(settings);
	freerdp_dynamic_channel_collection_free(settings);

	freerdp_capability_buffer_free(settings);

	/* Free all strings, set other pointers NULL */
	freerdp_settings_free_keys(settings, TRUE);
}

void freerdp_settings_free(rdpSettings* settings)
{
	if (!settings)
		return;

	freerdp_settings_free_internal(settings);
	free(settings);
}

static BOOL freerdp_settings_int_buffer_copy(rdpSettings* _settings, const rdpSettings* settings)
{
	BOOL rc = FALSE;

	if (!_settings || !settings)
		return FALSE;

	{
		const void* data = freerdp_settings_get_pointer(settings, FreeRDP_LoadBalanceInfo);
		const UINT32 len = freerdp_settings_get_uint32(settings, FreeRDP_LoadBalanceInfoLength);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_LoadBalanceInfo, data, len))
			return FALSE;
	}
	{
		const void* data = freerdp_settings_get_pointer(settings, FreeRDP_ServerRandom);
		const UINT32 len = freerdp_settings_get_uint32(settings, FreeRDP_ServerRandomLength);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ServerRandom, data, len))
			return FALSE;
	}
	{
		const void* data = freerdp_settings_get_pointer(settings, FreeRDP_ClientRandom);
		const UINT32 len = freerdp_settings_get_uint32(settings, FreeRDP_ClientRandomLength);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ClientRandom, data, len))
			return FALSE;
	}
	if (!freerdp_server_license_issuers_copy(_settings, settings->ServerLicenseProductIssuers,
	                                         settings->ServerLicenseProductIssuersCount))
		return FALSE;

	{
		const void* data = freerdp_settings_get_pointer(settings, FreeRDP_ServerCertificate);
		const UINT32 len = freerdp_settings_get_uint32(settings, FreeRDP_ServerCertificateLength);
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ServerCertificate, data, len))
			return FALSE;
	}
	if (settings->RdpServerCertificate)
	{
		rdpCertificate* cert = freerdp_certificate_clone(settings->RdpServerCertificate);
		if (!cert)
			goto out_fail;
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RdpServerCertificate, cert, 1))
			goto out_fail;
	}
	else
	{
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RdpServerCertificate, NULL, 0))
			goto out_fail;
	}

	if (settings->RdpServerRsaKey)
	{
		rdpPrivateKey* key = freerdp_key_clone(settings->RdpServerRsaKey);
		if (!key)
			goto out_fail;
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RdpServerRsaKey, key, 1))
			goto out_fail;
	}
	else
	{
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RdpServerRsaKey, NULL, 0))
			goto out_fail;
	}

	if (!freerdp_settings_set_uint32(_settings, FreeRDP_ChannelCount,
	                                 freerdp_settings_get_uint32(settings, FreeRDP_ChannelCount)))
		goto out_fail;
	if (!freerdp_settings_set_uint32(
	        _settings, FreeRDP_ChannelDefArraySize,
	        freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize)))
		goto out_fail;

	const UINT32 defArraySize = freerdp_settings_get_uint32(settings, FreeRDP_ChannelDefArraySize);
	const CHANNEL_DEF* defArray = freerdp_settings_get_pointer(settings, FreeRDP_ChannelDefArray);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ChannelDefArray, defArray,
	                                      defArraySize))
		goto out_fail;

	{
		const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorDefArraySize);
		const rdpMonitor* monitors =
		    freerdp_settings_get_pointer(settings, FreeRDP_MonitorDefArray);

		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_MonitorDefArray, monitors, count))
			goto out_fail;
	}

	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_MonitorIds, NULL, 16))
		goto out_fail;

	const UINT32 monitorIdSize = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	const UINT32* monitorIds = freerdp_settings_get_pointer(settings, FreeRDP_MonitorIds);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_MonitorIds, monitorIds, monitorIdSize))
		goto out_fail;

	_settings->OrderSupport = malloc(32);

	if (!_settings->OrderSupport)
		goto out_fail;

	if (!freerdp_capability_buffer_copy(_settings, settings))
		goto out_fail;
	CopyMemory(_settings->OrderSupport, settings->OrderSupport, 32);

	const UINT32 cellInfoSize =
	    freerdp_settings_get_uint32(settings, FreeRDP_BitmapCacheV2NumCells);
	const BITMAP_CACHE_V2_CELL_INFO* cellInfo =
	    freerdp_settings_get_pointer(settings, FreeRDP_BitmapCacheV2CellInfo);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_BitmapCacheV2CellInfo, cellInfo,
	                                      cellInfoSize))
		goto out_fail;

	const UINT32 glyphCacheCount = 10;
	const GLYPH_CACHE_DEFINITION* glyphCache =
	    freerdp_settings_get_pointer(settings, FreeRDP_GlyphCache);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_GlyphCache, glyphCache,
	                                      glyphCacheCount))
		goto out_fail;

	const UINT32 fragCacheCount = 1;
	const GLYPH_CACHE_DEFINITION* fragCache =
	    freerdp_settings_get_pointer(settings, FreeRDP_FragCache);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_FragCache, fragCache, fragCacheCount))
		goto out_fail;

	if (!freerdp_settings_set_pointer_len(
	        _settings, FreeRDP_ClientAutoReconnectCookie,
	        freerdp_settings_get_pointer(settings, FreeRDP_ClientAutoReconnectCookie), 1))
		goto out_fail;
	if (!freerdp_settings_set_pointer_len(
	        _settings, FreeRDP_ServerAutoReconnectCookie,
	        freerdp_settings_get_pointer(settings, FreeRDP_ServerAutoReconnectCookie), 1))
		goto out_fail;

	const TIME_ZONE_INFORMATION* tz =
	    freerdp_settings_get_pointer(settings, FreeRDP_ClientTimeZone);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_ClientTimeZone, tz, 1))
		goto out_fail;

	if (!freerdp_settings_set_uint32(
	        _settings, FreeRDP_RedirectionPasswordLength,
	        freerdp_settings_get_uint32(settings, FreeRDP_RedirectionPasswordLength)))
		goto out_fail;
	const UINT32 redirectionPasswordLength =
	    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionPasswordLength);
	const BYTE* pwd = freerdp_settings_get_pointer(settings, FreeRDP_RedirectionPassword);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RedirectionPassword, pwd,
	                                      redirectionPasswordLength))
		goto out_fail;

	const UINT32 RedirectionTsvUrlLength =
	    freerdp_settings_get_uint32(settings, FreeRDP_RedirectionTsvUrlLength);
	const BYTE* RedirectionTsvUrl =
	    freerdp_settings_get_pointer(settings, FreeRDP_RedirectionTsvUrl);
	if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_RedirectionTsvUrl, RedirectionTsvUrl,
	                                      RedirectionTsvUrlLength))
		goto out_fail;

	const UINT32 nrports = freerdp_settings_get_uint32(settings, FreeRDP_TargetNetAddressCount);
	if (!freerdp_target_net_adresses_reset(_settings, nrports))
		return FALSE;

	for (UINT32 i = 0; i < nrports; i++)
	{
		const char* address =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_TargetNetAddresses, i);
		const UINT32* port =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_TargetNetPorts, i);
		if (!freerdp_settings_set_pointer_array(_settings, FreeRDP_TargetNetAddresses, i, address))
			return FALSE;
		if (!freerdp_settings_set_pointer_array(_settings, FreeRDP_TargetNetPorts, i, port))
			return FALSE;
	}

	{
		const UINT32 len = freerdp_settings_get_uint32(_settings, FreeRDP_DeviceArraySize);
		const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_DeviceCount);

		if (len < count)
			goto out_fail;
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_DeviceArray, NULL, len))
			goto out_fail;
		if (!freerdp_settings_set_uint32(_settings, FreeRDP_DeviceCount, count))
			goto out_fail;

		for (size_t index = 0; index < count; index++)
		{
			const RDPDR_DEVICE* argv =
			    freerdp_settings_get_pointer_array(settings, FreeRDP_DeviceArray, index);
			if (!freerdp_settings_set_pointer_array(_settings, FreeRDP_DeviceArray, index, argv))
				goto out_fail;
		}
	}
	{
		const UINT32 len = freerdp_settings_get_uint32(_settings, FreeRDP_StaticChannelArraySize);
		const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_StaticChannelCount);

		if (len < count)
			goto out_fail;
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_StaticChannelArray, NULL, len))
			goto out_fail;
		if (!freerdp_settings_set_uint32(_settings, FreeRDP_StaticChannelCount, count))
			goto out_fail;

		for (size_t index = 0; index < count; index++)
		{
			const ADDIN_ARGV* argv =
			    freerdp_settings_get_pointer_array(settings, FreeRDP_StaticChannelArray, index);
			if (!freerdp_settings_set_pointer_array(_settings, FreeRDP_StaticChannelArray, index,
			                                        argv))
				goto out_fail;
		}
	}
	{
		const UINT32 len = freerdp_settings_get_uint32(_settings, FreeRDP_DynamicChannelArraySize);
		const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);

		if (len < count)
			goto out_fail;
		if (!freerdp_settings_set_pointer_len(_settings, FreeRDP_DynamicChannelArray, NULL, len))
			goto out_fail;
		if (!freerdp_settings_set_uint32(_settings, FreeRDP_DynamicChannelCount, count))
			goto out_fail;

		for (size_t index = 0; index < count; index++)
		{
			const ADDIN_ARGV* argv =
			    freerdp_settings_get_pointer_array(settings, FreeRDP_DynamicChannelArray, index);
			if (!freerdp_settings_set_pointer_array(_settings, FreeRDP_DynamicChannelArray, index,
			                                        argv))
				goto out_fail;
		}
	}

	rc = freerdp_settings_set_string(_settings, FreeRDP_ActionScript,
	                                 freerdp_settings_get_string(settings, FreeRDP_ActionScript));

out_fail:
	return rc;
}

BOOL freerdp_settings_copy(rdpSettings* _settings, const rdpSettings* settings)
{
	BOOL rc = 0;

	if (!settings || !_settings)
		return FALSE;

	/* This is required to free all non string buffers */
	freerdp_settings_free_internal(_settings);
	/* This copies everything except allocated non string buffers. reset all allocated buffers to
	 * NULL to fix issues during cleanup */
	rc = freerdp_settings_clone_keys(_settings, settings);

	_settings->LoadBalanceInfo = NULL;
	_settings->ServerRandom = NULL;
	_settings->ClientRandom = NULL;
	_settings->ServerCertificate = NULL;
	_settings->RdpServerCertificate = NULL;
	_settings->RdpServerRsaKey = NULL;
	_settings->ChannelDefArray = NULL;
	_settings->MonitorDefArray = NULL;
	_settings->MonitorIds = NULL;
	_settings->OrderSupport = NULL;
	_settings->BitmapCacheV2CellInfo = NULL;
	_settings->GlyphCache = NULL;
	_settings->FragCache = NULL;
	_settings->ClientAutoReconnectCookie = NULL;
	_settings->ServerAutoReconnectCookie = NULL;
	_settings->ClientTimeZone = NULL;
	_settings->RedirectionPassword = NULL;
	_settings->RedirectionTsvUrl = NULL;
	_settings->TargetNetAddresses = NULL;
	_settings->TargetNetPorts = NULL;
	_settings->DeviceArray = NULL;
	_settings->StaticChannelArray = NULL;
	_settings->DynamicChannelArray = NULL;
	_settings->ReceivedCapabilities = NULL;
	_settings->ReceivedCapabilityData = NULL;
	_settings->ReceivedCapabilityDataSizes = NULL;

	_settings->ServerLicenseProductIssuersCount = 0;
	_settings->ServerLicenseProductIssuers = NULL;

	if (!rc)
		goto out_fail;

	/* Begin copying */
	if (!freerdp_settings_int_buffer_copy(_settings, settings))
		goto out_fail;
	return TRUE;
out_fail:
	freerdp_settings_free_internal(_settings);
	return FALSE;
}

rdpSettings* freerdp_settings_clone(const rdpSettings* settings)
{
	rdpSettings* _settings = (rdpSettings*)calloc(1, sizeof(rdpSettings));

	if (!freerdp_settings_copy(_settings, settings))
		goto out_fail;

	return _settings;
out_fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	freerdp_settings_free(_settings);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

static void zfree(WCHAR* str, size_t len)
{
	if (str)
		memset(str, 0, len * sizeof(WCHAR));
	free(str);
}

BOOL identity_set_from_settings_with_pwd(SEC_WINNT_AUTH_IDENTITY* identity,
                                         const rdpSettings* settings,
                                         FreeRDP_Settings_Keys_String UserId,
                                         FreeRDP_Settings_Keys_String DomainId,
                                         const WCHAR* Password, size_t pwdLen)
{
	WINPR_ASSERT(identity);
	WINPR_ASSERT(settings);

	size_t UserLen = 0;
	size_t DomainLen = 0;

	WCHAR* Username = freerdp_settings_get_string_as_utf16(settings, UserId, &UserLen);
	WCHAR* Domain = freerdp_settings_get_string_as_utf16(settings, DomainId, &DomainLen);

	const int rc = sspi_SetAuthIdentityWithLengthW(identity, Username, UserLen, Domain, DomainLen,
	                                               Password, pwdLen);
	zfree(Username, UserLen);
	zfree(Domain, DomainLen);
	if (rc < 0)
		return FALSE;
	return TRUE;
}

BOOL identity_set_from_settings(SEC_WINNT_AUTH_IDENTITY_W* identity, const rdpSettings* settings,
                                FreeRDP_Settings_Keys_String UserId,
                                FreeRDP_Settings_Keys_String DomainId,
                                FreeRDP_Settings_Keys_String PwdId)
{
	WINPR_ASSERT(identity);
	WINPR_ASSERT(settings);

	size_t PwdLen = 0;

	WCHAR* Password = freerdp_settings_get_string_as_utf16(settings, PwdId, &PwdLen);

	const BOOL rc =
	    identity_set_from_settings_with_pwd(identity, settings, UserId, DomainId, Password, PwdLen);
	zfree(Password, PwdLen);
	return rc;
}

BOOL identity_set_from_smartcard_hash(SEC_WINNT_AUTH_IDENTITY_W* identity,
                                      const rdpSettings* settings,
                                      FreeRDP_Settings_Keys_String userId,
                                      FreeRDP_Settings_Keys_String domainId,
                                      FreeRDP_Settings_Keys_String pwdId, const BYTE* certSha1,
                                      size_t sha1len)
{
#ifdef _WIN32
	CERT_CREDENTIAL_INFO certInfo = { sizeof(CERT_CREDENTIAL_INFO), { 0 } };
	LPWSTR marshalledCredentials = NULL;

	memcpy(certInfo.rgbHashOfCert, certSha1, MIN(sha1len, sizeof(certInfo.rgbHashOfCert)));

	if (!CredMarshalCredentialW(CertCredential, &certInfo, &marshalledCredentials))
	{
		WLog_ERR(TAG, "error marshalling cert credentials");
		return FALSE;
	}

	size_t pwdLen = 0;
	WCHAR* Password = freerdp_settings_get_string_as_utf16(settings, pwdId, &pwdLen);
	const int rc = sspi_SetAuthIdentityWithLengthW(
	    identity, marshalledCredentials, _wcslen(marshalledCredentials), NULL, 0, Password, pwdLen);
	zfree(Password, pwdLen);
	CredFree(marshalledCredentials);
	if (rc < 0)
		return FALSE;

#else
	if (!identity_set_from_settings(identity, settings, userId, domainId, pwdId))
		return FALSE;
#endif /* _WIN32 */
	return TRUE;
}

const char* freerdp_settings_glyph_level_string(UINT32 level, char* buffer, size_t size)
{
	const char* str = "GLYPH_SUPPORT_UNKNOWN";
	switch (level)
	{
		case GLYPH_SUPPORT_NONE:
			str = "GLYPH_SUPPORT_NONE";
			break;
		case GLYPH_SUPPORT_PARTIAL:
			str = "GLYPH_SUPPORT_PARTIAL";
			break;
		case GLYPH_SUPPORT_FULL:
			str = "GLYPH_SUPPORT_FULL";
			break;
		case GLYPH_SUPPORT_ENCODE:
			str = "GLYPH_SUPPORT_ENCODE";
			break;
		default:
			break;
	}

	(void)_snprintf(buffer, size, "%s[0x%08" PRIx32 "]", str, level);
	return buffer;
}

BOOL freerdp_target_net_adresses_reset(rdpSettings* settings, size_t size)
{
	freerdp_target_net_addresses_free(settings);

	if (size > 0)
	{
		if (!freerdp_settings_set_pointer_len_(settings, FreeRDP_TargetNetPorts,
		                                       FreeRDP_UINT32_UNUSED, NULL, size, sizeof(UINT32)))
			return FALSE;
		if (!freerdp_settings_set_pointer_len_(settings, FreeRDP_TargetNetAddresses,
		                                       FreeRDP_TargetNetAddressCount, NULL, size,
		                                       sizeof(char*)))
			return FALSE;
	}
	return TRUE;
}

BOOL freerdp_settings_enforce_monitor_exists(rdpSettings* settings)
{
	const UINT32 nrIds = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_MonitorCount);
	const BOOL fullscreen = freerdp_settings_get_bool(settings, FreeRDP_Fullscreen);
	const BOOL multimon = freerdp_settings_get_bool(settings, FreeRDP_UseMultimon);
	const BOOL useMonitors = fullscreen || multimon;

	if (nrIds == 0)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_NumMonitorIds, 1))
			return FALSE;
	}
	if (!useMonitors || (count == 0))
	{
		const UINT32 width = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
		const UINT32 height = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
		const UINT32 pwidth = freerdp_settings_get_uint32(settings, FreeRDP_DesktopPhysicalWidth);
		const UINT32 pheight = freerdp_settings_get_uint32(settings, FreeRDP_DesktopPhysicalHeight);
		const UINT16 orientation =
		    freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
		const UINT32 desktopScaleFactor =
		    freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);
		const UINT32 deviceScaleFactor =
		    freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);

		if (!freerdp_settings_set_uint32(settings, FreeRDP_MonitorCount, 1))
			return FALSE;

		rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, 0);
		if (!monitor)
			return FALSE;
		monitor->x = 0;
		monitor->y = 0;
		WINPR_ASSERT(width <= INT32_MAX);
		monitor->width = (INT32)width;
		WINPR_ASSERT(height <= INT32_MAX);
		monitor->height = (INT32)height;
		monitor->is_primary = TRUE;
		monitor->orig_screen = 0;
		monitor->attributes.physicalWidth = pwidth;
		monitor->attributes.physicalHeight = pheight;
		monitor->attributes.orientation = orientation;
		monitor->attributes.desktopScaleFactor = desktopScaleFactor;
		monitor->attributes.deviceScaleFactor = deviceScaleFactor;
	}
	else if (fullscreen || (multimon && (count == 1)))
	{

		/* not all platforms start primary monitor at 0/0, so enforce this to avoid issues with
		 * fullscreen mode */
		rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, 0);
		if (!monitor)
			return FALSE;
		monitor->x = 0;
		monitor->y = 0;
		monitor->is_primary = TRUE;
	}

	return TRUE;
}
