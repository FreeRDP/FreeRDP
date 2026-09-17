/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Common client monitor scaling
 *
 * Copyright 2026 FreeRDP contributors
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

#include <stdlib.h>
#include <winpr/assert.h>
#include <freerdp/client/monitor.h>
#include <freerdp/log.h>

#define TAG CLIENT_TAG("common.monitor")

static BOOL parse_uint32(const char** cursor, UINT32* value, char separator)
{
	const char* p = *cursor;
	UINT32 result = 0;
	if ((*p < '0') || (*p > '9'))
		return FALSE;
	while ((*p >= '0') && (*p <= '9'))
	{
		const UINT32 digit = (UINT32)(*p - '0');
		if (result > (UINT32_MAX - digit) / 10)
			return FALSE;
		result = result * 10 + digit;
		p++;
	}
	if (*p != separator)
		return FALSE;
	*cursor = separator ? p + 1 : p;
	*value = result;
	return TRUE;
}

static BOOL valid_scale(const rdpMonitorScale* scale)
{
	WINPR_ASSERT(scale);
	return (scale->desktopScaleFactor >= 100) && (scale->desktopScaleFactor <= 500) &&
	               ((scale->deviceScaleFactor == 100) || (scale->deviceScaleFactor == 140) ||
	                (scale->deviceScaleFactor == 180))
	           ? TRUE
	           : FALSE;
}

BOOL freerdp_client_parse_monitor_scales(rdpSettings* settings, const char* value)
{
	BOOL rc = FALSE;
	size_t count = 1;
	WINPR_ASSERT(settings);
	if (!value || !*value)
		return FALSE;
	if (freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorScales) != 0)
	{
		WLog_ERR(TAG, "/monitor-scale must not be specified more than once");
		return FALSE;
	}
	for (const char* p = value; *p; p++)
	{
		if (*p == ',')
			count++;
	}
	if (count > UINT32_MAX)
		return FALSE;
	rdpMonitorScale* scales = calloc(count, sizeof(rdpMonitorScale));
	if (!scales)
		return FALSE;
	const char* cursor = value;
	for (size_t i = 0; i < count; i++)
	{
		rdpMonitorScale* scale = &scales[i];
		if (!parse_uint32(&cursor, &scale->id, ':') ||
		    !parse_uint32(&cursor, &scale->desktopScaleFactor, ':') ||
		    !parse_uint32(&cursor, &scale->deviceScaleFactor, (i + 1 < count) ? ',' : '\0') ||
		    !valid_scale(scale))
		{
			WLog_ERR(TAG, "Invalid /monitor-scale entry: expected id:desktop:device, desktop "
			              "100..500, device 100/140/180");
			goto out;
		}
		for (size_t j = 0; j < i; j++)
		{
			if (scales[j].id == scale->id)
			{
				WLog_ERR(TAG, "Duplicate /monitor-scale ID %" PRIu32, scale->id);
				goto out;
			}
		}
	}
	rc = freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorScales, scales, count);
	if (rc)
		rc = freerdp_settings_set_bool(settings, FreeRDP_HasMonitorAttributes, TRUE);
out:
	free(scales);
	return rc;
}

BOOL freerdp_client_validate_monitor_scales(const rdpSettings* settings, const UINT32* ids,
                                            size_t count)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(ids || (count == 0));
	const UINT32 num = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorScales);
	const UINT64 mask = freerdp_settings_get_uint64(settings, FreeRDP_MonitorOverrideFlags);
	if ((num > 0) && ((mask & (FREERDP_MONITOR_OVERRIDE_DESKTOP_SCALE |
	                           FREERDP_MONITOR_OVERRIDE_DEVICE_SCALE)) != 0))
	{
		WLog_ERR(TAG,
		         "/monitor-scale cannot be combined with global desktop/device scale overrides");
		return FALSE;
	}
	for (UINT32 i = 0; i < num; i++)
	{
		const rdpMonitorScale* scale =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorScales, i);
		if (!scale || !valid_scale(scale))
			return FALSE;
		for (UINT32 j = 0; j < i; j++)
		{
			const rdpMonitorScale* prev =
			    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorScales, j);
			if (!prev || (prev->id == scale->id))
				return FALSE;
		}
		if (ids)
		{
			BOOL found = FALSE;
			for (size_t j = 0; j < count; j++)
			{
				if (ids[j] == scale->id)
				{
					found = TRUE;
					break;
				}
			}
			if (!found)
			{
				WLog_ERR(TAG,
				         "Unknown /monitor-scale ID %" PRIu32
				         ". Use /list:monitor to list available IDs.",
				         scale->id);
				return FALSE;
			}
		}
	}
	return TRUE;
}

void freerdp_client_apply_monitor_scale(const rdpSettings* settings, rdpMonitor* monitor)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(monitor);
	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorScales);
	for (UINT32 i = 0; i < count; i++)
	{
		const rdpMonitorScale* scale =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorScales, i);
		WINPR_ASSERT(scale);
		if (scale->id != monitor->orig_screen)
			continue;
		monitor->attributes.desktopScaleFactor = scale->desktopScaleFactor;
		monitor->attributes.deviceScaleFactor = scale->deviceScaleFactor;
		return;
	}
}
